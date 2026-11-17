// Content pipeline tests. Runs the real `run` path with test/stub-claude.js in place of `claude` - no agent is launched.
//   node Tools/ContentPipeline/test/pipeline.test.js   ->   PIPELINE TEST PASS
'use strict';
const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');
const { buildFacts } = require('../lib/facts');
const { checkEvents, checkPool } = require('../lib/checks');

const ROOT = path.resolve(__dirname, '..', '..', '..');
const CLI = path.join(__dirname, '..', 'content-pipeline.js');
const STUB = path.join(__dirname, 'stub-claude.js');
let passed = 0;
function test(name, fn) {
  try { fn(); passed++; console.log(`  ok   ${name}`); } catch (e) { console.log(`  FAIL ${name}\n       ${e.message}`); process.exitCode = 1; }
}
const tmp = (p) => fs.mkdtempSync(path.join(os.tmpdir(), `dw-content-${p}-`));
const rules = (f) => f.filter((x) => x.severity !== 'minor').map((x) => x.rule);

const facts = buildFacts(ROOT);

test('facts come from the repo (11-Events.md, tuning, C++ headers)', () => {
  assert.deepStrictEqual(Object.keys(facts.event.outcomes).sort(), ['AbilityChoice', 'CrewHealth', 'Gold', 'GiveItem', 'GrantXp', 'LoseCrew', 'Recruit', 'RandomOf', 'ShipHealth'].sort());
  assert.deepStrictEqual(facts.event.removedOutcomes.map((r) => r.type), ['CrewMana']);
  assert.deepStrictEqual([facts.event.minChoices, facts.event.maxChoices, facts.event.maxLayer], [1, 3, 9]);
  assert.deepStrictEqual(facts.event.valences, ['Good', 'Bad']);
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
  buildFacts(root); // unchanged copy parses
  const doc = path.join(root, 'Docs/Architecture/11-Events.md');
  fs.writeFileSync(doc, fs.readFileSync(doc, 'utf8').replace(/^`UDwEventRequirement`.*$/m, 'Requirements: TBD'));
  assert.throws(() => buildFacts(root), /requirement/);
});

test('code checks catch each planted mistake', () => {
  process.env.STUB_LOG = '';
  const r = spawnSync(process.execPath, [STUB, '--agent', 'event-writer'], { input: 'first', encoding: 'utf8' });
  const draft = JSON.parse(r.stdout).structured_output;
  const f = checkEvents(draft.events, facts, { count: 4, valences: { Good: 3, Bad: 1 }, layers: [1, 3] });
  const has = (rule, re) => assert.ok(f.some((x) => x.rule === rule && re.test(x.problem)), `expected ${rule} ${re}\n${JSON.stringify(f, null, 1)}`);
  has('E06 params', /"Rum" is not an item/);
  has('E05 outcome', /"CrewMana" was removed/);
  has('E08 text vs data', /"40 gold"/);
  has('E06 params', /never remove the captain/);
  has('E11 never stuck', /stuck/);
  has('E07 guardrail', /Gold \+500/);
  has('E10 voice', /"matey"/);
});

test('code checks pass the fixed draft', () => {
  const r = spawnSync(process.execPath, [STUB, '--agent', 'event-writer'], { input: '## Revision', encoding: 'utf8' });
  const draft = JSON.parse(r.stdout).structured_output;
  assert.deepStrictEqual(rules(checkEvents(draft.events, facts, { count: 4, valences: { Good: 3, Bad: 1 }, layers: [1, 3] })), []);
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
  const pool = [...Array(8)].map((_, i) => mk(i, 'Good', 1, 9));
  const f = checkPool(pool, facts);
  assert.ok(f.some((x) => x.rule === 'P1 scope'));
  assert.ok(f.some((x) => x.rule === 'P2 60/40'));
  assert.ok(f.some((x) => x.rule === 'P4 coverage' && /No Bad event/.test(x.problem)));
  const ok = [...Array(7)].map((_, i) => mk(i, 'Good', 1, 9)).concat([...Array(5)].map((_, i) => mk(i + 7, 'Bad', 1, 9)));
  assert.deepStrictEqual(checkPool(ok, facts), []);
});

test('loop: draft -> checks + critic -> revision -> pass (stub agents)', () => {
  const out = tmp('out');
  const log = path.join(out, 'calls.jsonl');
  const r = spawnSync(process.execPath, [CLI, 'run', '--batch', 'early-waters', '--out', out],
    { cwd: ROOT, encoding: 'utf8', env: { ...process.env, CONTENT_PIPELINE_CLAUDE: STUB, STUB_LOG: log } });
  assert.strictEqual(r.status, 0, r.stdout + r.stderr);
  const calls = fs.readFileSync(log, 'utf8').trim().split('\n').map((l) => JSON.parse(l));
  assert.deepStrictEqual(calls.map((c) => c.agent), ['event-writer', 'content-critic', 'event-writer', 'content-critic']);
  for (const c of calls) {
    assert.ok(c.argv.includes('--json-schema') && c.argv.includes('--max-budget-usd'), 'every call is schema-bound and budgeted');
    assert.ok(c.prompt.includes('"goodEventChance": 0.6'), 'every call gets the repo facts');
    assert.ok(c.prompt.includes('## Output format (required)'), 'the schema is in the prompt (--agent ignores --json-schema)');
  }
  assert.ok(calls[1].prompt.includes('CrewMana\\" was removed from the game'), 'critic sees the code-check results');
  assert.ok(calls[2].prompt.includes('## Revision') && calls[2].prompt.includes('Toasting with no cost'), 'writer gets code + critic findings');
  const dir = path.join(out, 'early-waters');
  for (const f of ['draft-1.json', 'review-1.json', 'draft-2.json', 'review-2.json', 'final.json', 'report.md']) assert.ok(fs.existsSync(path.join(dir, f)), f);
  const report = fs.readFileSync(path.join(dir, 'report.md'), 'utf8');
  assert.ok(report.startsWith("# Batch `early-waters` - ready for Brandon's review"), report.slice(0, 120));
  assert.ok(report.includes('Choice 1: Pay 40 gold  -> Requires: MinGold 40 -> Outcomes: Gold -40'));
  assert.ok(fs.existsSync(path.join(out, 'summary.md')));
  assert.ok(/PIPELINE DONE: 1 batch\(es\), 4 agent calls, \$0\.04/.test(r.stdout), r.stdout);
});

test('loop stops at --revisions and marks the batch for Brandon', () => {
  const out = tmp('norev');
  const r = spawnSync(process.execPath, [CLI, 'run', '--batch', 'early-waters', '--out', out, '--revisions', '0'],
    { cwd: ROOT, encoding: 'utf8', env: { ...process.env, CONTENT_PIPELINE_CLAUDE: STUB } });
  assert.strictEqual(r.status, 0, r.stderr);
  assert.ok(fs.readFileSync(path.join(out, 'early-waters', 'report.md'), 'utf8').startsWith('# Batch `early-waters` - needs Brandon'));
});

test('a reply that misses the schema is retried once, and raw replies are kept', () => {
  const out = tmp('shape');
  const r = spawnSync(process.execPath, [CLI, 'run', '--batch', 'early-waters', '--out', out],
    { cwd: ROOT, encoding: 'utf8', env: { ...process.env, CONTENT_PIPELINE_CLAUDE: STUB, STUB_BAD_SHAPE_ONCE: path.join(out, 'flag') } });
  assert.strictEqual(r.status, 0, r.stdout + r.stderr);
  assert.ok(/reply missed the schema \(\$\.voice is missing/.test(r.stdout), r.stdout);
  assert.ok(/5 agent calls/.test(r.stdout), r.stdout);
  assert.strictEqual(fs.readdirSync(path.join(out, 'early-waters', 'calls')).length, 5);
});

test('total budget cap stops the run', () => {
  const out = tmp('budget');
  const r = spawnSync(process.execPath, [CLI, 'run', '--batch', 'early-waters', '--out', out, '--total-budget-usd', '0.015'],
    { cwd: ROOT, encoding: 'utf8', env: { ...process.env, CONTENT_PIPELINE_CLAUDE: STUB } });
  assert.strictEqual(r.status, 1);
  assert.ok(/total budget \$0\.015 reached/.test(r.stderr), r.stderr);
});

console.log(process.exitCode ? `PIPELINE TEST FAIL (${passed} passed)` : `PIPELINE TEST PASS ${passed}/${passed}`);
