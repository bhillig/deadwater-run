// Content loop tests. Runs the real CLI with test/stub-claude.js in place of `claude` - no agent is launched.
//   node Tools/ContentPipeline/test/pipeline.test.js   ->   PIPELINE TEST PASS n/n
'use strict';
const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');
const { buildFacts } = require('../lib/facts');
const { checkEvents, checkPool } = require('../lib/checks');
const { buildStyleGuide, strangerTest, specificitySignals, styleMarkdown } = require('../lib/style');
const { scoreEvaluation, accepted, BLOCKER_CAP } = require('../lib/score');
const { enforceLocks } = require('../content-pipeline');

const ROOT = path.resolve(__dirname, '..', '..', '..');
const CLI = path.join(__dirname, '..', 'content-pipeline.js');
const STUB = path.join(__dirname, 'stub-claude.js');
let passed = 0;
function test(name, fn) {
  try { fn(); passed++; console.log(`  ok   ${name}`); } catch (e) { console.log(`  FAIL ${name}\n       ${e.message}`); process.exitCode = 1; }
}
const tmp = (p) => fs.mkdtempSync(path.join(os.tmpdir(), `dw-content-${p}-`));
const rules = (f) => f.filter((x) => x.severity !== 'minor').map((x) => x.rule);
const stubDraft = (agent, scenario = 'default') => JSON.parse(spawnSync(process.execPath, [STUB, '--agent', agent], { input: '', encoding: 'utf8', env: { ...process.env, STUB_LOG: '', STUB_SCENARIO: scenario } }).stdout).structured_output;
const run = (out, extraArgs = [], env = {}) => spawnSync(process.execPath, [CLI, 'run', '--batch', 'early-waters', '--out', out, '--no-calibrate', ...extraArgs],
  { cwd: ROOT, encoding: 'utf8', env: { ...process.env, CONTENT_PIPELINE_CLAUDE: STUB, ...env } });
const readJson = (p) => JSON.parse(fs.readFileSync(p, 'utf8'));

const facts = buildFacts(ROOT);
const guide = buildStyleGuide(ROOT, facts);

// ---------------------------------------------------------------- facts and code checks
test('facts come from the repo (11-Events.md, tuning, C++ headers)', () => {
  assert.deepStrictEqual(Object.keys(facts.event.outcomes).sort(), ['AbilityChoice', 'CrewHealth', 'Gold', 'GiveItem', 'GrantXp', 'LoseCrew', 'Nothing', 'Recruit', 'RandomOf', 'ShipHealth'].sort());
  assert.ok(facts.event.requirements.includes('CrewCountAtMost'));
  assert.deepStrictEqual(facts.event.removedOutcomes.map((r) => r.type), ['CrewMana']);
  assert.deepStrictEqual([facts.event.minChoices, facts.event.maxChoices, facts.event.maxLayer], [1, 3, 9]);
  assert.deepStrictEqual(facts.names.items, ['HealthRestore', 'ManaRestore']);
  assert.ok(facts.bannedWords.includes('matey'));
  assert.strictEqual(facts.guardrails.goldGainMax, 150);
});

test('facts stop the run when a source changes shape', () => {
  const root = tmp('facts');
  for (const rel of ['DeadwaterRun.md', 'Docs/Architecture/11-Events.md', 'Docs/Architecture/09-Run-Map.md', 'Docs/ContentPipeline/voice.md',
    'Source/DeadwaterRun/Data/DwEventDefinition.h', 'Source/DeadwaterRun/Data/DwDataTypes.h']) {
    fs.mkdirSync(path.dirname(path.join(root, rel)), { recursive: true });
    fs.copyFileSync(path.join(ROOT, rel), path.join(root, rel));
  }
  fs.cpSync(path.join(ROOT, 'Tuning'), path.join(root, 'Tuning'), { recursive: true });
  buildFacts(root);
  const doc = path.join(root, 'Docs/Architecture/11-Events.md');
  fs.writeFileSync(doc, fs.readFileSync(doc, 'utf8').replace(/^`UDwEventRequirement`.*$/m, 'Requirements: TBD'));
  assert.throws(() => buildFacts(root), /requirement/);
});

test('code checks catch each planted mistake', () => {
  const f = checkEvents(stubDraft('event-writer').events, facts, { count: 4, valences: { Good: 3, Bad: 1 }, layers: [1, 3] });
  const has = (rule, re) => assert.ok(f.some((x) => x.rule === rule && re.test(x.problem)), `expected ${rule} ${re}\n${JSON.stringify(f, null, 1)}`);
  has('E06 params', /"Rum" is not an item/);
  has('E05 outcome', /"CrewMana" was removed/);
  has('E08 text vs data', /"40 gold"/);
  has('E06 params', /never remove the captain/);
  has('E11 never stuck', /stuck/);
  has('E07 guardrail', /Gold \+500/);
  has('E10 voice', /"matey"/);
});

test('code checks pass the refined draft; Recruit without CrewCountAtMost is a trap', () => {
  const draft = stubDraft('content-refiner');
  assert.deepStrictEqual(rules(checkEvents(draft.events, facts, { count: 4, valences: { Good: 3, Bad: 1 }, layers: [1, 3] })), []);
  const hire = JSON.parse(JSON.stringify(draft.events[1]));
  hire.choices[0].requirements = [];
  assert.ok(checkEvents([hire], facts).some((x) => x.rule === 'E15 recruit trap'));
});

test('RandomOf options are checked recursively', () => {
  const e = { id: 'DA_Event_Dice', title: 'Dice', body: 'A game of dice. Stakes are high.', valence: 'Good', weight: 1, minLayer: 1, maxLayer: 9,
    choices: [{ label: 'Roll', requirements: [], resultText: 'The dice land.', outcomes: [{ type: 'RandomOf', options: [{ weight: 1, outcomes: [{ type: 'Gold', delta: 60 }] }, { weight: 0, outcomes: [{ type: 'Curse' }] }] }] }] };
  const f = checkEvents([e], facts);
  assert.ok(f.some((x) => /weight 0/.test(x.problem)));
  assert.ok(f.some((x) => /UDwEventOutcome_Curse/.test(x.problem)));
});

test('pool checks: scope size, 60/40, layer coverage', () => {
  const mk = (i, valence, a, b) => ({ id: `DA_Event_E${i}`, valence, minLayer: a, maxLayer: b });
  const f = checkPool([...Array(8)].map((_, i) => mk(i, 'Good', 1, 9)), facts);
  assert.ok(f.some((x) => x.rule === 'P1 scope'));
  assert.ok(f.some((x) => x.rule === 'P2 60/40'));
  assert.ok(f.some((x) => x.rule === 'P4 coverage' && /No Bad event/.test(x.problem)));
  const ok = [...Array(7)].map((_, i) => mk(i, 'Good', 1, 9)).concat([...Array(5)].map((_, i) => mk(i + 7, 'Bad', 1, 9)));
  assert.deepStrictEqual(checkPool(ok, facts), []);
});

// ---------------------------------------------------------------- style guide and the stranger test
test('style guide is compiled from the repo, every rule cited verbatim', () => {
  assert.ok(guide.rules.length >= 20, `${guide.rules.length} rules`);
  for (const r of guide.rules) {
    assert.ok(r.cites.length, `${r.id} has no citation`);
    for (const c of r.cites) {
      const line = fs.readFileSync(path.join(ROOT, c.file), 'utf8').replace(/\r\n/g, '\n').split('\n')[c.line - 1];
      assert.ok(line.includes(c.quote), `${r.id}: ${c.file}:${c.line} does not contain "${c.quote}"`);
    }
  }
  assert.deepStrictEqual(guide.captains.map((c) => c.key), facts.names.captains, 'captains come from the GDD and match CaptainTuning');
  assert.ok(guide.rules.find((r) => r.id === 'C02').rule.includes('Smoke and Fire'), 'captain rules carry the GDD abilities');
  assert.ok(guide.shipped.length >= 10, 'the shipped pool (Tuning/Events.json) is in the guide');
  assert.ok(styleMarkdown(guide).includes('`DeadwaterRun.md:'), 'the markdown shows citations');
});

test('stranger test: every rule names something only Deadwater Run has; a generic guide fails', () => {
  assert.deepStrictEqual(strangerTest(guide, ROOT), []);
  const genericGuide = {
    ...guide,
    rules: [
      { id: 'X1', rule: 'Write in a salty pirate voice with short, punchy sentences.', cites: guide.rules[0].cites },
      { id: 'X2', rule: 'Every choice should weigh gold against risk to the ship and crew.', cites: guide.rules[0].cites },
    ],
  };
  const fails = strangerTest(genericGuide, ROOT);
  assert.ok(fails.some((x) => /X1: names nothing only Deadwater Run has/.test(x)), fails.join('\n'));
  assert.ok(fails.some((x) => /X2: names nothing/.test(x)), 'genre words (gold, ship, crew) never count');
  assert.ok(fails.some((x) => /never mentions "Kraken"/.test(x)));
});

test('stranger test: a moved or edited citation fails the build', () => {
  const bad = { ...guide, rules: guide.rules.map((r, i) => (i ? r : { ...r, cites: [{ ...r.cites[0], line: r.cites[0].line + 1 }] })) };
  assert.ok(strangerTest(bad, ROOT).some((x) => /S01: citation .* is not verbatim/.test(x)));
});

test('specificity signals: generic event has none, Deadwater event has terms + mechanics', () => {
  const generic = readJson(path.join(__dirname, '..', 'calibration', 'generic-pirate.json')).events;
  for (const e of generic) assert.deepStrictEqual(specificitySignals(e, guide), { terms: [], mechanics: [] }, e.id);
  const s = specificitySignals(stubDraft('content-refiner').events[2], guide);
  assert.ok(s.terms.includes('plunder') && s.mechanics.some((m) => /ship vs crew/.test(m)), JSON.stringify(s));
  assert.deepStrictEqual(specificitySignals({ title: 'x', body: 'They fear the sea.', choices: [] }, guide).terms, [], '"fear" is not the Fear mechanic');
});

// ---------------------------------------------------------------- the score
test('score: weighted dimensions, code penalties, blocker cap, always a reason', () => {
  const dims = (n) => Object.fromEntries(['rules', 'text_data', 'design', 'voice', 'specificity'].map((k) => [k, { score: n, reason: `r-${k}` }]));
  const clean = scoreEvaluation({ dimensions: dims(90), findings: [], summary: 'fine' }, []);
  assert.strictEqual(clean.score, 90);
  assert.ok(/90\/100\..*specificity 90 - r-specificity.*Evaluator: fine/.test(clean.reason), clean.reason);
  const weighted = scoreEvaluation({ dimensions: { ...dims(100), specificity: { score: 80, reason: 'ok' } }, findings: [] }, []);
  assert.strictEqual(weighted.score, 95, 'specificity weighs 25 of 100');
  const generic = scoreEvaluation({ dimensions: { ...dims(100), specificity: { score: 50, reason: 'generic' } }, findings: [] }, []);
  assert.strictEqual(generic.base, 87.5);
  assert.strictEqual(generic.score, 69, 'polished but generic is capped by the stranger gate');
  assert.ok(generic.strangerCapped && /a stranger couldn't tell this is Deadwater Run/.test(generic.reason) && !accepted(generic, [], 85));
  const pen = scoreEvaluation({ dimensions: dims(90), findings: [] }, [{ severity: 'major', rule: 'E07', event: 'A' }, { severity: 'minor', rule: 'E02', event: 'A' }]);
  assert.strictEqual(pen.score, 83);
  assert.ok(/Code checks -7 \(1 major x6, 1 minor x1\)/.test(pen.reason), pen.reason);
  const capped = scoreEvaluation({ dimensions: dims(100), findings: [{ severity: 'blocker', rule: 'V07', event: 'B' }] }, []);
  assert.strictEqual(capped.score, BLOCKER_CAP);
  assert.ok(/Capped at 49: .*V07 on B/.test(capped.reason));
  assert.ok(!accepted(pen, [{ severity: 'major' }], 80), 'a code major blocks acceptance');
  assert.ok(accepted(clean, [], 85) && !accepted(clean, [], 95));
});

test('locks: code restores a locked event the refiner changed or dropped', () => {
  const a = { id: 'A', title: 'a' }; const b = { id: 'B', title: 'b' };
  const refined = { events: [{ id: 'A', title: 'changed' }] };
  const reverted = enforceLocks(refined, { draft: { events: [a, b] } }, ['A', 'B']);
  assert.deepStrictEqual(reverted, ['A', 'B (dropped)']);
  assert.deepStrictEqual(refined.events, [a, b]);
});

// ---------------------------------------------------------------- the loop, end to end with stub agents
test('loop: generator -> evaluator -> refiner -> evaluator -> accepted, no human step', () => {
  const out = tmp('loop');
  const log = path.join(out, 'calls.jsonl');
  const r = run(out, [], { STUB_LOG: log });
  assert.strictEqual(r.status, 0, r.stdout + r.stderr);
  const calls = fs.readFileSync(log, 'utf8').trim().split('\n').map((l) => JSON.parse(l));
  assert.deepStrictEqual(calls.map((c) => c.agent), ['event-writer', 'content-critic', 'content-refiner', 'content-critic']);
  for (const c of calls) {
    assert.ok(c.argv.includes('--json-schema') && c.argv.includes('--max-budget-usd'), 'every call is schema-bound and budgeted');
    assert.ok(c.prompt.includes('"goodEventChance": 0.6'), 'every call gets the repo facts');
    assert.ok(c.prompt.includes('- S02 [setting]') && c.prompt.includes('DA_Event_TheTallyRock'), 'every call gets the compiled style guide and shipped pool');
    assert.ok(c.prompt.includes('## Output format (required)'));
  }
  assert.ok(calls[1].prompt.includes('## Rubric') && calls[1].prompt.includes('## Specificity signals'), 'evaluator gets the rubric and signals');
  assert.ok(calls[2].prompt.startsWith('REFINE:') && /## Score \d+\/100 - how it was scored/.test(calls[2].prompt), 'refiner gets the score and its reason');
  const dir = path.join(out, 'early-waters');
  for (const f of ['draft-1.json', 'eval-1.json', 'draft-2.json', 'eval-2.json', 'final.json', 'report.md']) assert.ok(fs.existsSync(path.join(dir, f)), f);
  const e1 = readJson(path.join(dir, 'eval-1.json')); const e2 = readJson(path.join(dir, 'eval-2.json'));
  assert.ok(e1.score.score <= BLOCKER_CAP && e1.score.blockers.length >= 4, `round 1 has blockers: ${e1.score.score} ${e1.score.blockers}`);
  assert.ok(e2.score.score >= 85 && e2.score.reason.length > 40, `round 2 accepted with a reason: ${e2.score.reason}`);
  const final = readJson(path.join(dir, 'final.json'));
  assert.strictEqual(final.round, 2);
  assert.ok(/^accepted at \d+\/100$/.test(final.status), final.status);
  const report = fs.readFileSync(path.join(dir, 'report.md'), 'utf8');
  assert.ok(report.startsWith('# Batch `early-waters` - accepted at'), report.slice(0, 120));
  assert.ok(report.includes('## Score by round') && /\| 2 \| refiner \| \*\*\d+\*\*/.test(report));
  assert.ok(!/Brandon/.test(report), 'no step waits on a human');
  assert.ok(/LOOP DONE: 1 batch\(es\), 1 accepted/.test(r.stdout), r.stdout);
});

test('loop keeps the best draft: a refiner that regresses never wins, and the loop stops on a plateau', () => {
  const out = tmp('regress');
  const log = path.join(out, 'calls.jsonl');
  const r = run(out, ['--patience', '2'], { STUB_SCENARIO: 'regress', STUB_LOG: log });
  assert.strictEqual(r.status, 0, r.stdout + r.stderr);
  const final = readJson(path.join(out, 'early-waters', 'final.json'));
  assert.strictEqual(final.round, 1, 'round 1 stays the best');
  assert.ok(final.events.some((e) => e.id === 'DA_Event_FloatingBarrel'), 'final is the round-1 draft');
  assert.ok(/best effort \d+\/100 \(threshold 85; stopped: plateau/.test(final.status), final.status);
  const refines = fs.readFileSync(log, 'utf8').trim().split('\n').map((l) => JSON.parse(l)).filter((c) => c.agent === 'content-refiner');
  assert.strictEqual(refines.length, 2);
  for (const c of refines) assert.ok(c.prompt.includes('DA_Event_FloatingBarrel') && !/"id": "DA_Event_RumBarrels"/.test(c.prompt.split('## Draft to refine')[1].split('## Deadwater')[0]), 'every refine starts from the best draft, not the regression');
});

test('loop locks high-scoring events: the refiner\'s edit to one is reverted', () => {
  const out = tmp('lock');
  const r = run(out, [], { STUB_SCENARIO: 'lock' });
  assert.strictEqual(r.status, 0, r.stdout + r.stderr);
  assert.ok(/code restored locked event\(s\): DA_Event_ScarredSloop/.test(r.stdout), r.stdout);
  const final = readJson(path.join(out, 'early-waters', 'final.json'));
  assert.strictEqual(final.events.find((e) => e.id === 'DA_Event_ScarredSloop').title, 'ScarredSloop');
  assert.ok(!final.events.some((e) => e.id === 'DA_Event_FloatingBarrel'), 'the generic event was replaced');
  assert.ok(/^accepted/.test(final.status), final.status);
});

test('a reply that misses the schema is retried once, and raw replies are kept', () => {
  const out = tmp('shape');
  const r = run(out, [], { STUB_BAD_SHAPE_ONCE: path.join(out, 'flag') });
  assert.strictEqual(r.status, 0, r.stdout + r.stderr);
  assert.ok(/reply missed the schema \(\$\.dimensions is missing/.test(r.stdout), r.stdout);
  assert.strictEqual(fs.readdirSync(path.join(out, 'early-waters', 'calls')).length, 5);
});

test('budget: stops with the best draft so far; with no draft at all the run fails', () => {
  const out = tmp('budget');
  const r = run(out, ['--total-budget-usd', '0.025']);
  assert.strictEqual(r.status, 0, r.stdout + r.stderr);
  assert.ok(/stopped: budget/.test(readJson(path.join(out, 'early-waters', 'final.json')).status));
  const none = run(tmp('budget0'), ['--total-budget-usd', '0']);
  assert.strictEqual(none.status, 1);
  assert.ok(/total budget \$0 reached/.test(none.stderr), none.stderr);
});

test('calibrate: generic control scores low, shipped events score well above it', () => {
  const out = tmp('calib');
  const r = spawnSync(process.execPath, [CLI, 'calibrate', '--out', out], { cwd: ROOT, encoding: 'utf8', env: { ...process.env, CONTENT_PIPELINE_CLAUDE: STUB } });
  assert.strictEqual(r.status, 0, r.stdout + r.stderr);
  assert.ok(/CALIBRATION PASS/.test(r.stdout), r.stdout);
  const report = fs.readFileSync(path.join(out, 'calibration', 'report.md'), 'utf8');
  assert.ok(report.startsWith('# Evaluator calibration - PASS') && report.includes('| control |') && report.includes('| shipped |'));
});

test('run recalibrates when the evaluator\'s inputs changed, and skips it when they didn\'t', () => {
  const out = tmp('gate');
  const log = path.join(out, 'calls.jsonl');
  const go = () => spawnSync(process.execPath, [CLI, 'run', '--batch', 'early-waters', '--out', out], { cwd: ROOT, encoding: 'utf8', env: { ...process.env, CONTENT_PIPELINE_CLAUDE: STUB, STUB_LOG: log } });
  const first = go();
  assert.strictEqual(first.status, 0, first.stdout + first.stderr);
  assert.ok(/calibration: never calibrated - calibrating before the loop/.test(first.stdout) && /CALIBRATION PASS/.test(first.stdout), first.stdout);
  const second = go();
  assert.ok(/calibration: current \(/.test(second.stdout), second.stdout);
  const stamp = path.join(out, 'calibration', 'stamp.json');
  fs.writeFileSync(stamp, JSON.stringify({ ...readJson(stamp), key: 'stale' }));
  assert.ok(/the evaluator's inputs changed/.test(go().stdout));
  fs.writeFileSync(stamp, JSON.stringify({ ...readJson(stamp), pass: false, key: 'x' }));
  const failing = spawnSync(process.execPath, [CLI, 'run', '--batch', 'early-waters', '--out', out, '--threshold', '20'], { cwd: ROOT, encoding: 'utf8', env: { ...process.env, CONTENT_PIPELINE_CLAUDE: STUB } });
  assert.strictEqual(failing.status, 1, 'a failing calibration stops the loop');
  assert.ok(/evaluator calibration FAIL - not running the loop/.test(failing.stderr), failing.stderr);
});

console.log(process.exitCode ? `PIPELINE TEST FAIL (${passed} passed)` : `PIPELINE TEST PASS ${passed}/${passed}`);
