#!/usr/bin/env node
/*
 * Deadwater Run - self-correcting content loop (map events)
 *
 *   brief ─► GENERATOR (event-writer) ─► code checks (lib/checks.js) ─► EVALUATOR (content-critic) ─► score 0-100 + reason
 *                                                                                                       │
 *     ┌──── score >= threshold, no blocker, no code major ──► accepted ◄──────────────────────────────────┤
 *     │                                                                                                 ▼
 *     │     REFINER (content-refiner) rewrites only what scored low, starting from the BEST draft so far ──► re-evaluate
 *     │     stops on its own: accepted, plateau (no gain for --patience rounds), --max-rounds or the budget
 *     ▼
 *   final.json = the best-scoring draft, never a regression. No human in the loop.
 *
 *   All three agents get the same game facts (lib/facts.js) and style guide (lib/style.js), both compiled from the
 *   repo on every run: the GDD, voice sheet, store page, UI art direction, 11-Events.md, tuning and the shipped pool.
 *
 *   node Tools/ContentPipeline/content-pipeline.js facts                 # the game facts the agents get
 *   node Tools/ContentPipeline/content-pipeline.js style                 # rebuild Docs/ContentPipeline/style-guide.md (stranger test)
 *   node Tools/ContentPipeline/content-pipeline.js check <draft.json>    # code checks only (no agents)
 *   node Tools/ContentPipeline/content-pipeline.js run --batch <id>      # one brief through the loop
 *   node Tools/ContentPipeline/content-pipeline.js run --all             # every brief, then the pool summary
 *   node Tools/ContentPipeline/content-pipeline.js calibrate             # evaluator on a generic control vs the shipped pool
 *   node Tools/ContentPipeline/content-pipeline.js report                # rebuild <out>/summary.md
 *
 * Options: --root <repo>  --out <dir> (default Docs/ContentPipeline/runs)  --threshold N (default 85)
 *          --max-rounds N (evaluations per batch, default 5)  --patience N (default 2)
 *          --budget-usd N (per agent call, default 3)  --total-budget-usd N (default 25)
 *          --no-calibrate (skip the calibration gate - tests only)
 * `run` first checks <out>/calibration/stamp.json: if the evaluator's inputs changed since the last passing
 * calibration, it recalibrates, and it refuses to run the loop if calibration fails.
 * `run` and `calibrate` launch Claude Code agents (`claude -p --agent ...`) and cost money.
 * CONTENT_PIPELINE_CLAUDE=<stub.js> swaps `claude` for a stub (test/pipeline.test.js).
 *
 * Output is drafts. Nothing is written to Content/ or Tuning/.
 * Zero dependencies: Node 18+.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const crypto = require('crypto');
const { buildFacts, factsForPrompt } = require('./lib/facts');
const { checkEvents, checkPool, OUTCOME_FIELDS, REQUIREMENT_FIELDS } = require('./lib/checks');
const { buildStyleGuide, specificitySignals, styleMarkdown, styleForPrompt } = require('./lib/style');
const { DIMENSIONS, PENALTY, BLOCKER_CAP, STRANGER_FLOOR, STRANGER_CAP, scoreEvaluation, scoreEvents, accepted } = require('./lib/score');

const DEFAULT_ROOT = path.resolve(__dirname, '..', '..');
const BRIEFS = path.join(__dirname, 'briefs.json');
const CONTROL = path.join(__dirname, 'calibration', 'generic-pirate.json');
const GENERATOR = 'event-writer';
const EVALUATOR = 'content-critic';
const REFINER = 'content-refiner';

// ------------------------------------------------------------------------------------------------
// CLI
// ------------------------------------------------------------------------------------------------

function parseArgs(argv) {
  const a = { cmd: argv[2], file: null, root: DEFAULT_ROOT, out: null, batch: null, all: false,
    threshold: 85, maxRounds: 5, patience: 2, budgetUsd: 3, totalBudgetUsd: 25 };
  for (let i = 3; i < argv.length; i++) {
    const k = argv[i];
    const next = () => argv[++i];
    switch (k) {
      case '--root': a.root = path.resolve(next()); break;
      case '--out': a.out = path.resolve(next()); break;
      case '--batch': a.batch = next(); break;
      case '--all': a.all = true; break;
      case '--threshold': a.threshold = +next(); break;
      case '--max-rounds': a.maxRounds = +next(); break;
      case '--patience': a.patience = +next(); break;
      case '--budget-usd': a.budgetUsd = +next(); break;
      case '--total-budget-usd': a.totalBudgetUsd = +next(); break;
      case '--no-calibrate': a.noCalibrate = true; break;
      default:
        if (k.startsWith('--')) throw new Error(`Unknown option ${k}`);
        a.file = k;
    }
  }
  if (!(a.maxRounds >= 1) || !(a.patience >= 1) || !(a.threshold > 0 && a.threshold <= 100)) throw new Error('--max-rounds/--patience >= 1, 0 < --threshold <= 100');
  a.out = a.out || path.join(a.root, 'Docs', 'ContentPipeline', 'runs');
  return a;
}

// ------------------------------------------------------------------------------------------------
// Output schemas (shape only - game rules live in lib/checks.js, style rules in lib/style.js)
// ------------------------------------------------------------------------------------------------

const EVENT = {
  type: 'object',
  required: ['id', 'title', 'body', 'valence', 'weight', 'minLayer', 'maxLayer', 'choices'],
  properties: {
    id: { type: 'string' }, title: { type: 'string' }, body: { type: 'string' }, valence: { type: 'string' },
    weight: { type: 'number' }, minLayer: { type: 'integer' }, maxLayer: { type: 'integer' },
    choices: {
      type: 'array',
      items: {
        type: 'object',
        required: ['label', 'requirements', 'outcomes', 'resultText'],
        properties: {
          label: { type: 'string' }, resultText: { type: 'string' },
          requirements: { type: 'array', items: { type: 'object' } },
          outcomes: { type: 'array', items: { type: 'object' } },
        },
      },
    },
  },
};

const EVENT_SCHEMA = { type: 'object', required: ['events', 'notes'], properties: { events: { type: 'array', items: EVENT }, notes: { type: 'string' } } };

const SCORED = { type: 'object', required: ['score', 'reason'], properties: { score: { type: 'integer', minimum: 0, maximum: 100 }, reason: { type: 'string' } } };

const EVAL_SCHEMA = {
  type: 'object',
  required: ['dimensions', 'events', 'findings', 'code_check_disputes', 'summary'],
  properties: {
    dimensions: { type: 'object', required: DIMENSIONS.map((d) => d.key), properties: Object.fromEntries(DIMENSIONS.map((d) => [d.key, SCORED])) },
    events: {
      type: 'array',
      items: { type: 'object', required: ['id', 'score', 'reason', 'stranger_test'], properties: { id: { type: 'string' }, score: { type: 'integer', minimum: 0, maximum: 100 }, reason: { type: 'string' }, stranger_test: { type: 'string' } } },
    },
    findings: {
      type: 'array',
      items: {
        type: 'object',
        required: ['severity', 'event', 'field', 'rule', 'problem', 'fix'],
        properties: { severity: { type: 'string', enum: ['blocker', 'major', 'minor'] }, event: { type: 'string' }, field: { type: 'string' }, rule: { type: 'string' }, problem: { type: 'string' }, fix: { type: 'string' } },
      },
    },
    code_check_disputes: { type: 'array', items: { type: 'object', required: ['rule', 'event', 'why'], properties: { rule: { type: 'string' }, event: { type: 'string' }, why: { type: 'string' } } } },
    summary: { type: 'string' },
  },
};

const REFINE_SCHEMA = {
  type: 'object',
  required: ['events', 'changes', 'notes'],
  properties: {
    events: { type: 'array', items: EVENT },
    changes: { type: 'array', items: { type: 'object', required: ['event', 'field', 'finding', 'after'], properties: { event: { type: 'string' }, field: { type: 'string' }, finding: { type: 'string' }, after: { type: 'string' } } } },
    notes: { type: 'string' },
  },
};

// ------------------------------------------------------------------------------------------------
// Prompts
// ------------------------------------------------------------------------------------------------

function formatSpec() {
  const line = (name, spec) => `- ${name}: { "type": "${name}"${Object.entries(spec).map(([k, t]) => `, "${k}": <${t}>`).join('')} }`;
  return [
    'Each event:',
    '{ "id": "DA_Event_<PascalCase>", "title", "body", "valence": "Good"|"Bad", "weight": 1, "minLayer", "maxLayer",',
    '  "choices": [ { "label", "requirements": [...], "outcomes": [...], "resultText" } ] }',
    '',
    'Outcomes (applied in order; `delta` is signed; RandomOf options are { "weight": >0, "outcomes": [...] }):',
    ...Object.entries(OUTCOME_FIELDS).map(([n, s]) => line(n, s)),
    '',
    'Requirements (all must pass to enable the choice):',
    ...Object.entries(REQUIREMENT_FIELDS).map(([n, s]) => line(n, s)),
    '',
    'Types: <int> whole number, <posint> > 0, <selector> one of event.selectors, <item> one of names.items,',
    '<unit> one of names.recruitableUnits, <captain> one of names.captains.',
  ].join('\n');
}

const section = (title, body) => `## ${title}\n\n${body}\n`;
const json = (x) => '```json\n' + JSON.stringify(x, null, 2) + '\n```';

// `claude -p --agent` does not enforce --json-schema (no structured_output in the reply, seen 2026-11-17), so the
// schema is also stated in the prompt, and callAgentChecked() verifies the shape.
const outputSection = (schema, example) => section('Output format (required)', [
  'Reply with one JSON object and nothing else. Use exactly these top-level keys - no extra or renamed keys.',
  '', 'JSON Schema:', '```json\n' + JSON.stringify(schema) + '\n```',
  ...(example ? ['', 'Shape:', json(example)] : []),
].join('\n'));

function commonSections(ctx) {
  return [
    section('Deadwater Run style guide (compiled from the repo - every rule id is citable)', styleForPrompt(ctx.guide)),
    section('Game facts (from the repo - the only rules, names and numbers that exist)', '```json\n' + factsForPrompt(ctx.facts) + '\n```'),
    section('Event data format', formatSpec()),
    section('GDD - Game Mechanics (DeadwaterRun.md)', ctx.facts.gddMechanics),
  ];
}

function generatorPrompt(brief, ctx) {
  return [
    `GENERATE: write ${brief.count} Deadwater Run map events for this brief. Return only JSON in the given schema.`,
    'A stranger who reads one of your events must be able to tell it is Deadwater Run and no other pirate game.',
    '',
    section('Brief', JSON.stringify(brief, null, 2)),
    ...commonSections(ctx),
    section('Events already drafted in other batches (don\'t repeat their ideas or ids)', ctx.others.length ? ctx.others.map((e) => `- ${e.id}: ${e.title}`).join('\n') : '(none)'),
    outputSection(EVENT_SCHEMA),
  ].join('\n');
}

const EVAL_EXAMPLE = {
  dimensions: Object.fromEntries(DIMENSIONS.map((d) => [d.key, { score: 0, reason: '<why this number, citing rule ids and quoting lines>' }])),
  events: [{ id: 'DA_Event_X', score: 0, reason: '<why>', stranger_test: '<what ties it to Deadwater Run, or "nothing - works in any pirate game">' }],
  findings: [{ severity: 'major', event: 'DA_Event_X', field: 'choices[0].resultText', rule: 'S03', problem: '<what is wrong>', fix: '<concrete fix>' }],
  code_check_disputes: [],
  summary: '<one or two sentences: the biggest reason the score is not higher>',
};

function evaluatorPrompt(brief, ctx, draft, code, signals) {
  return [
    'EVALUATE: score this generated Deadwater Run content. Return only JSON in the given schema.',
    'You emit scores, not a pass/fail. Code turns your dimension scores into the final 0-100 score, so each number must be honest on its own.',
    '',
    section('Rubric (score every dimension 0-100 with a reason that cites rule ids and quotes the weakest line)', [
      ...DIMENSIONS.map((d) => `- \`${d.key}\` (weight ${d.weight}): ${d.what}`),
      '',
      'Anchors, the same for every dimension:',
      '- 95-100: could ship next to the shipped pool today; nothing to change',
      '- 85-94: sounds and plays like Deadwater Run; only polish left',
      '- 70-84: works, but a stranger might not place it, or a real design/voice problem remains',
      '- 50-69: generic pirate content, or a choice that is a trap/dominant, or text that misleads',
      '- 0-49: broken, contradicts the facts, or could be from any pirate game',
      '',
      '`specificity` is the stranger test. For each event ask: if I swapped the names, would it work unchanged in another',
      'pirate game? Code found the signature terms and mechanics below; terms alone don\'t pass it (name-dropping the',
      'Kraken in a generic scene is still generic). What passes is an event built on what only this game has: two health',
      'bars, sink vs plunder, a crew of 3-4 who die for good, the three captains, Aquadia, the Kraken at the end of the chart.',
      '',
      `Per event: a 0-100 score with a reason and a one-line stranger_test. Code subtracts ${PENALTY.blocker}/${PENALTY.major}/${PENALTY.minor} per code blocker/major/minor, caps any score with a blocker at ${BLOCKER_CAP},`,
      `and caps the batch at ${STRANGER_CAP} when specificity is under ${STRANGER_FLOOR}: generic content is wrong however polished it is.`,
    ].join('\n')),
    section('Content type', 'Map events (UDwEventDefinition, Docs/Architecture/11-Events.md)'),
    section('Brief it was written for', JSON.stringify(brief, null, 2)),
    section('Content', json(draft.events) + '\n\nAuthor\'s notes: ' + (draft.notes || '(none)')),
    section('Code-check results (already enforced and already deducted - don\'t repeat them; dispute any that are wrong)', json(code)),
    section('Specificity signals found by code (per event)', json(signals)),
    ...commonSections(ctx),
    section('Findings', 'Each finding cites the style-guide id (S01-S08, C01-C03, V01-V07, A01-A03) or names the dimension it breaks. blocker = the game can\'t do it, text contradicts data, or a run breaks; major = a real design, voice or stranger-test problem; minor = polish.'),
    outputSection(EVAL_SCHEMA, EVAL_EXAMPLE),
  ].join('\n');
}

function refinerPrompt(brief, ctx, best, locked, threshold) {
  const low = best.eventScores.filter((e) => !locked.includes(e.id));
  return [
    `REFINE: raise this batch's score to ${threshold}+ by fixing what the evaluator scored low. Return only JSON in the given schema.`,
    '',
    section('Rules for this pass', [
      '- Start from the draft below (the best-scoring draft so far). Return the whole batch.',
      `- Locked events scored ${threshold}+ with no blocker/major finding. Return them exactly as they are - code restores any edit to them.`,
      '- Fix every blocker and major finding, and lift every dimension scored under the threshold. Change only what a finding or a low score points at; keep lines that work.',
      '- Each change goes in `changes` with the finding or dimension it answers (e.g. "S03", "specificity", "E15 recruit trap").',
      '- If you think a finding is wrong, leave the line and say why in `notes`.',
    ].join('\n')),
    section('Brief', JSON.stringify(brief, null, 2)),
    section(`Score ${best.scored.score}/100 - how it was scored`, best.scored.reason),
    section('Per-event scores', best.eventScores.map((e) => `- ${e.id}: ${e.score}/100${locked.includes(e.id) ? ' (LOCKED)' : ''} - ${e.reason}`).join('\n')),
    section('Findings to fix (code + evaluator)', json(best.findings.filter((f) => f.severity !== 'minor' || low.some((e) => e.id === f.event)))),
    section('Draft to refine', json(best.draft.events)),
    ...commonSections(ctx),
    section('Events already drafted in other batches (don\'t repeat their ideas or ids)', ctx.others.length ? ctx.others.map((e) => `- ${e.id}: ${e.title}`).join('\n') : '(none)'),
    outputSection(REFINE_SCHEMA),
  ].join('\n');
}

// ------------------------------------------------------------------------------------------------
// Agents
// ------------------------------------------------------------------------------------------------

function extractJson(text) {
  const s = String(text || '').trim();
  try { return JSON.parse(s); } catch { /* fall through */ }
  const fence = /```(?:json)?\s*([\s\S]*?)```/.exec(s);
  if (fence) return JSON.parse(fence[1]);
  const a = s.indexOf('{'); const b = s.lastIndexOf('}');
  if (a >= 0 && b > a) return JSON.parse(s.slice(a, b + 1));
  throw new Error('agent returned no JSON');
}

class BudgetError extends Error {}

function callAgent(agent, prompt, schema, args, ledger) {
  if (ledger.cost >= args.totalBudgetUsd) throw new BudgetError(`total budget $${args.totalBudgetUsd} reached ($${ledger.cost.toFixed(2)})`);
  const cliArgs = ['-p', '--agent', agent, '--output-format', 'json', '--json-schema', JSON.stringify(schema),
    '--tools', 'Read,Glob,Grep', '--no-session-persistence', '--max-budget-usd', String(args.budgetUsd)];
  const override = process.env.CONTENT_PIPELINE_CLAUDE;
  const [exe, exeArgs] = !override ? ['claude', cliArgs] : override.endsWith('.js') ? [process.execPath, [override, ...cliArgs]] : [override, cliArgs];
  const t0 = Date.now();
  // The prompt goes in on stdin: it is multi-line markdown that a command line would mangle.
  const r = spawnSync(exe, exeArgs, { cwd: args.root, input: prompt, encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
  if (r.error) throw new Error(`could not start ${exe}: ${r.error.message}`);
  let j;
  try { j = JSON.parse(r.stdout); } catch { throw new Error(`${agent}: not JSON output (exit ${r.status}): ${(r.stdout || r.stderr || '').slice(0, 400)}`); }
  const cost = j.total_cost_usd || 0;
  ledger.cost += cost;
  ledger.calls.push({ agent, cost, turns: j.num_turns, seconds: Math.round((Date.now() - t0) / 1000) });
  if (ledger.rawDir) {
    fs.mkdirSync(ledger.rawDir, { recursive: true });
    const { credential_expires_at, ...kept } = j; // account detail, not part of the audit trail
    fs.writeFileSync(path.join(ledger.rawDir, `${String(ledger.calls.length).padStart(2, '0')}-${agent}.json`), JSON.stringify(kept, null, 2) + '\n');
  }
  if (j.is_error || r.status !== 0) throw new Error(`${agent} failed (exit ${r.status}): ${String(j.result || r.stderr).slice(0, 400)}`);
  const data = j.structured_output || extractJson(j.result);
  console.log(`   ${agent.padEnd(15)} $${cost.toFixed(2)}  ${j.num_turns} turns  ${Math.round((Date.now() - t0) / 1000)}s`);
  return data;
}

/** Required keys of the schema and its required object children, plus integer ranges on scores. */
function shapeErrors(data, schema, at = '$') {
  if (schema.type === 'object') {
    if (!data || typeof data !== 'object' || Array.isArray(data)) return [`${at} is not an object`];
    return (schema.required || []).flatMap((k) => (data[k] === undefined ? [`${at}.${k} is missing`] : shapeErrors(data[k], schema.properties[k], `${at}.${k}`)));
  }
  if (schema.type === 'array') {
    if (!Array.isArray(data)) return [`${at} is not an array`];
    return schema.items && schema.items.type === 'object' ? data.flatMap((x, i) => shapeErrors(x, schema.items, `${at}[${i}]`)) : [];
  }
  if (schema.type === 'integer' && !(Number.isInteger(data) && data >= (schema.minimum ?? -Infinity) && data <= (schema.maximum ?? Infinity))) return [`${at} must be an integer ${schema.minimum}-${schema.maximum}`];
  if (schema.type === 'string' && typeof data !== 'string') return [`${at} is not a string`];
  return [];
}

/** Calls the agent; if the reply misses the schema (or `extra` rejects it), asks once more with the errors. */
function callAgentChecked(agent, prompt, schema, args, ledger, extra = () => []) {
  const errors = (d) => { const e = shapeErrors(d, schema); return e.length ? e : extra(d); };
  let data = callAgent(agent, prompt, schema, args, ledger);
  let errs = errors(data);
  if (errs.length) {
    console.log(`   ${agent}: reply missed the schema (${errs.slice(0, 3).join('; ')}) - asking again`);
    data = callAgent(agent, `${prompt}\n\n## Your previous reply was rejected\n\nIt did not match the output schema: ${errs.join('; ')}. Return the complete JSON object with every required key at the top level.`, schema, args, ledger);
    errs = errors(data);
    if (errs.length) throw new Error(`${agent} reply still misses the schema: ${errs.join('; ')}`);
  }
  return data;
}

/** The evaluation must score every event it was given - a skipped event would silently score 0. */
const evalCovers = (events) => (d) => events.filter((e) => !d.events.some((x) => x.id === e.id)).map((e) => `$.events has no score for ${e.id}`);

// ------------------------------------------------------------------------------------------------
// Readable output (the Editor-Tools.md "writing in a doc first" format)
// ------------------------------------------------------------------------------------------------

const sign = (n) => (n > 0 ? `+${n}` : `${n}`);
function fmtOutcome(o) {
  switch (o.type) {
    case 'Gold': case 'ShipHealth': return `${o.type} ${sign(o.delta)}`;
    case 'CrewHealth': return `CrewHealth ${sign(o.delta)} (${o.selector})`;
    case 'GiveItem': return `GiveItem ${o.item} x${o.count}`;
    case 'AbilityChoice': return `AbilityChoice x${o.count}`;
    case 'GrantXp': return `GrantXp ${o.amount} (${o.selector})`;
    case 'Recruit': return `Recruit ${o.unit}`;
    case 'LoseCrew': return `LoseCrew (${o.selector})`;
    case 'Nothing': return 'Nothing';
    case 'RandomOf': return `RandomOf [ ${(o.options || []).map((x) => `${x.weight}: ${(x.outcomes || []).map(fmtOutcome).join(', ') || 'nothing'}`).join(' | ')} ]`;
    default: return `${o.type}?? ${JSON.stringify(o)}`;
  }
}
const fmtReq = (r) => `${r.type} ${r.amount ?? r.count ?? r.item ?? r.captain ?? ''}`.trim();

function eventsMarkdown(events, eventScores = []) {
  return events.map((e) => {
    const s = eventScores.find((x) => x.id === e.id);
    return [
      `### ${e.title}  \`${e.id}\`${s ? ` - ${s.score}/100` : ''}`,
      ...(s ? ['', `_${s.reason}_`, ''] : []),
      '```',
      `Title:    ${e.title}`,
      `Valence:  ${e.valence.padEnd(9)} Layers: ${e.minLayer}-${e.maxLayer}        Weight: ${e.weight}`,
      `Body:     ${e.body}`,
      ...e.choices.map((c, i) => `Choice ${i + 1}: ${c.label}  ->${c.requirements.length ? ` Requires: ${c.requirements.map(fmtReq).join('; ')} ->` : ''} Outcomes: ${c.outcomes.map(fmtOutcome).join('; ') || 'none'}  -> Result: ${c.resultText}`),
      '```',
    ].join('\n');
  }).join('\n\n');
}

const blocking = (f) => f.severity === 'blocker' || f.severity === 'major';
const cell = (s) => String(s).replace(/\|/g, '\\|').replace(/\n/g, ' ');

function findingsTable(rows) {
  if (!rows.length) return '_None._';
  return ['| Sev | From | Event | Field | Rule | Problem | Fix |', '|---|---|---|---|---|---|---|',
    ...rows.map((f) => `| ${f.severity} | ${f.source} | ${f.event} | \`${f.field}\` | ${f.rule} | ${cell(f.problem)} | ${cell(f.fix)} |`)].join('\n');
}

function batchReport(brief, rounds, best, status, local, args) {
  const dimHead = DIMENSIONS.map((d) => d.key).join(' | ');
  return [
    `# Batch \`${brief.id}\` - ${status}`,
    '',
    `Brief: ${brief.theme}`,
    `Layers ${brief.layers.join('-')}, ${brief.count} events (${Object.entries(brief.valences).map(([k, v]) => `${v} ${k}`).join(', ')}).`,
    `Threshold ${args.threshold}, max ${args.maxRounds} evaluations, patience ${args.patience}. Agent cost: $${local.calls.reduce((s, c) => s + c.cost, 0).toFixed(2)} over ${local.calls.length} calls.`,
    '',
    `**Final score: ${best.scored.score}/100** (round ${best.round}). ${best.scored.reason.replace(/^\d+\/100\. /, '')}`,
    '',
    '## Score by round',
    '',
    `| Round | Made by | Score | ${dimHead} | Code B/M/m | Kept |`,
    `|---|---|---|${DIMENSIONS.map(() => '---').join('|')}|---|---|`,
    ...rounds.map((r) => `| ${r.round} | ${r.by} | **${r.scored.score}** | ${r.scored.dims.map((d) => d.score).join(' | ')} | ${['blocker', 'major', 'minor'].map((k) => r.scored.counts[k] || 0).join('/')} | ${r.round === best.round ? 'best' : ''} |`),
    '',
    ...rounds.flatMap((r) => [
      `## Round ${r.round} (${r.by}) - ${r.scored.score}/100`,
      '',
      r.scored.reason,
      '',
      ...(r.reverted && r.reverted.length ? [`Code restored locked events the refiner edited: ${r.reverted.join(', ')}.`, ''] : []),
      ...(r.changes && r.changes.length ? ['Refiner changes:', ...r.changes.map((c) => `- ${c.event} \`${c.field}\` (${c.finding}): ${cell(c.after)}`), ''] : []),
      findingsTable(r.findings),
      ...(r.evaluation.code_check_disputes.length ? ['', 'Evaluator disputes of code checks:', ...r.evaluation.code_check_disputes.map((d) => `- ${d.rule} on ${d.event}: ${d.why}`)] : []),
      '',
    ]),
    '## Final events (best round)',
    '',
    eventsMarkdown(best.draft.events, best.eventScores),
    '',
  ].join('\n');
}

// ------------------------------------------------------------------------------------------------
// The loop
// ------------------------------------------------------------------------------------------------

function loadBriefs() { return JSON.parse(fs.readFileSync(BRIEFS, 'utf8')).briefs; }

function draftedElsewhere(out, exceptBatch) {
  if (!fs.existsSync(out)) return [];
  return fs.readdirSync(out).filter((d) => d !== exceptBatch && fs.existsSync(path.join(out, d, 'final.json')))
    .flatMap((d) => JSON.parse(fs.readFileSync(path.join(out, d, 'final.json'), 'utf8')).events);
}

/** Code checks, the evaluator and the score for one draft. */
function evaluate(brief, ctx, draft, call, by, round) {
  const code = checkEvents(draft.events, ctx.facts, brief, ctx.takenIds);
  const signals = Object.fromEntries(draft.events.map((e) => [e.id, specificitySignals(e, ctx.guide)]));
  const evaluation = call(EVALUATOR, evaluatorPrompt(brief, ctx, draft, code, signals), EVAL_SCHEMA, evalCovers(draft.events));
  const scored = scoreEvaluation(evaluation, code);
  const eventScores = scoreEvents(evaluation, code, draft.events);
  const findings = [...code, ...evaluation.findings.map((f) => ({ source: 'critic', ...f }))];
  return { round, by, draft, code, signals, evaluation, scored, eventScores, findings };
}

/** Events the refiner may not touch: high-scoring, no blocker/major of their own, and no batch-level problem. */
function lockedIds(r, threshold) {
  if (r.findings.some((f) => blocking(f) && /^\(/.test(f.event))) return [];
  return r.eventScores.filter((e) => e.score >= threshold && !e.findings.some(blocking)).map((e) => e.id);
}

/** Puts back any locked event the refiner changed or dropped. Returns the ids it restored. */
function enforceLocks(refined, best, locked) {
  const reverted = [];
  for (const id of locked) {
    const orig = best.draft.events.find((e) => e.id === id);
    const i = refined.events.findIndex((e) => e.id === id);
    if (i < 0) { refined.events.push(orig); reverted.push(`${id} (dropped)`); } else if (JSON.stringify(refined.events[i]) !== JSON.stringify(orig)) { refined.events[i] = orig; reverted.push(id); }
  }
  return reverted;
}

function runBatch(brief, ctx, args, ledger) {
  console.log(`\n=== batch ${brief.id}: ${brief.count} events, layers ${brief.layers.join('-')} ===`);
  const dir = path.join(args.out, brief.id);
  fs.rmSync(dir, { recursive: true, force: true });
  fs.mkdirSync(dir, { recursive: true });
  const save = (name, obj) => fs.writeFileSync(path.join(dir, name), typeof obj === 'string' ? obj : JSON.stringify(obj, null, 2) + '\n');
  const bctx = { ...ctx, others: [...ctx.guide.shipped, ...draftedElsewhere(args.out, brief.id)] };
  bctx.takenIds = bctx.others.map((e) => e.id);
  const local = { calls: [] };
  ledger.rawDir = path.join(dir, 'calls');
  const call = (agent, prompt, schema, extra) => {
    const before = ledger.calls.length;
    try { return callAgentChecked(agent, prompt, schema, args, ledger, extra); } finally { local.calls.push(...ledger.calls.slice(before)); }
  };

  const rounds = [];
  let best = null;
  let stop = 'max rounds';
  let draft = call(GENERATOR, generatorPrompt(brief, bctx), EVENT_SCHEMA);
  let by = 'generator';
  let extra = {};
  for (let round = 1; round <= args.maxRounds; round++) {
    save(`draft-${round}.json`, draft);
    let r;
    try { r = evaluate(brief, bctx, draft, call, by, round); } catch (e) { if (e instanceof BudgetError && best) { stop = 'budget'; break; } throw e; }
    Object.assign(r, extra);
    save(`eval-${round}.json`, { by, score: r.scored, eventScores: r.eventScores, code: r.code, signals: r.signals, evaluation: r.evaluation, ...extra });
    rounds.push(r);
    const improved = !best || r.scored.score > best.scored.score;
    if (improved) best = r;
    console.log(`   round ${round} (${by}): ${r.scored.score}/100${improved ? ' (best)' : ` (best is ${best.scored.score} from round ${best.round})`}`);
    if (accepted(best.scored, best.code, args.threshold)) { stop = 'accepted'; break; }
    if (round - best.round >= args.patience) { stop = `plateau (no gain for ${args.patience} rounds)`; break; }
    if (round === args.maxRounds) break;
    // Refine the best draft so far - a regression is never built on.
    const locked = lockedIds(best, args.threshold);
    let refined;
    try { refined = call(REFINER, refinerPrompt(brief, bctx, best, locked, args.threshold), REFINE_SCHEMA); } catch (e) { if (e instanceof BudgetError) { stop = 'budget'; break; } throw e; }
    const reverted = enforceLocks(refined, best, locked);
    if (reverted.length) console.log(`   code restored locked event(s): ${reverted.join(', ')}`);
    draft = { events: refined.events, notes: refined.notes };
    by = 'refiner';
    extra = { changes: refined.changes, reverted, locked };
  }
  const ok = accepted(best.scored, best.code, args.threshold);
  const status = ok ? `accepted at ${best.scored.score}/100` : `best effort ${best.scored.score}/100 (threshold ${args.threshold}; stopped: ${stop})`;
  save('final.json', { ...best.draft, score: best.scored.score, round: best.round, status });
  save('report.md', batchReport(brief, rounds, best, status, local, args));
  console.log(`   -> ${status}; ${path.relative(args.root, path.join(dir, 'report.md'))}`);
  return { brief, rounds, best, status, accepted: ok, cost: local.calls.reduce((s, c) => s + c.cost, 0) };
}

function writeSummary(args, facts) {
  const briefs = loadBriefs();
  const rows = [];
  const all = [];
  for (const b of briefs) {
    const dir = path.join(args.out, b.id);
    if (!fs.existsSync(path.join(dir, 'final.json'))) { rows.push(`| \`${b.id}\` | not run | | | |`); continue; }
    const final = JSON.parse(fs.readFileSync(path.join(dir, 'final.json'), 'utf8'));
    const evals = fs.readdirSync(dir).filter((f) => /^eval-\d+\.json$/.test(f)).sort((x, y) => parseInt(x.slice(5), 10) - parseInt(y.slice(5), 10))
      .map((f) => JSON.parse(fs.readFileSync(path.join(dir, f), 'utf8')));
    all.push(...final.events);
    rows.push(`| [\`${b.id}\`](${b.id}/report.md) | ${final.events.length} | ${evals.map((e) => e.score.score).join(' -> ')} | **${final.score}** | ${final.status} |`);
  }
  const pool = checkPool(all, facts);
  const md = [
    '# Content loop - event pool summary',
    '',
    'Generated by `node Tools/ContentPipeline/content-pipeline.js report`. Drafts, not in the game.',
    '',
    '| Batch | Events | Score by round | Final | Status |',
    '|---|---|---|---|---|',
    ...rows,
    '',
    `Pool: ${all.length} events, ${all.filter((e) => e.valence === 'Good').length} Good / ${all.filter((e) => e.valence === 'Bad').length} Bad.`,
    '',
    '## Pool checks (across batches)',
    '',
    findingsTable(pool),
    '',
  ].join('\n');
  fs.mkdirSync(args.out, { recursive: true });
  fs.writeFileSync(path.join(args.out, 'summary.md'), md);
  console.log(`\nsummary: ${path.relative(args.root, path.join(args.out, 'summary.md'))} (${all.length} events, ${pool.length} pool findings)`);
  return { all, pool };
}

/**
 * Proves the evaluator can tell Deadwater Run from generic pirate content before anyone trusts its scores:
 * a generic control batch must score low (and lowest on specificity), shipped events must score well above it.
 */
function calibrate(ctx, args, ledger) {
  const dir = path.join(args.out, 'calibration');
  fs.rmSync(dir, { recursive: true, force: true });
  ledger.rawDir = path.join(dir, 'calls');
  const control = JSON.parse(fs.readFileSync(CONTROL, 'utf8'));
  const shipped = { notes: 'Shipped events from Tuning/Events.json (in the game).', events: ctx.guide.shipped.filter((e) => control.compare.includes(e.id)) };
  const brief = { id: 'calibration', layers: [1, 9], count: 4, valences: { Good: 2, Bad: 2 }, theme: 'Calibration: score as written.' };
  const call = (agent, prompt, schema, extra) => callAgentChecked(agent, prompt, schema, args, ledger, extra);
  const cctx = { ...ctx, others: [], takenIds: [] };
  const g = evaluate(brief, cctx, { events: control.events, notes: control.notes }, call, 'control', 1);
  const s = evaluate(brief, cctx, shipped, call, 'shipped', 2);
  const spec = (r) => r.scored.dims.find((d) => d.key === 'specificity').score;
  // The control is valid, competent writing, so only the stranger test should sink it.
  const checks = [
    [`generic control can't be accepted: ${g.scored.score} < ${args.threshold} and held under the stranger cap (${STRANGER_CAP})`, !accepted(g.scored, g.code, args.threshold) && g.scored.score <= STRANGER_CAP],
    [`generic control fails the stranger test: specificity ${spec(g)} < 40`, spec(g) < 40],
    [`shipped specificity beats the control by 30+ (${spec(s)} vs ${spec(g)})`, spec(s) - spec(g) >= 30],
    [`shipped events outscore the control (${s.scored.score} vs ${g.scored.score})`, s.scored.score > g.scored.score],
  ];
  const pass = checks.every(([, ok]) => ok);
  fs.mkdirSync(dir, { recursive: true });
  const save = (name, r) => fs.writeFileSync(path.join(dir, name), JSON.stringify({ score: r.scored, eventScores: r.eventScores, code: r.code, signals: r.signals, evaluation: r.evaluation }, null, 2) + '\n');
  save('control.json', g);
  save('shipped.json', s);
  fs.writeFileSync(path.join(dir, 'report.md'), [
    `# Evaluator calibration - ${pass ? 'PASS' : 'FAIL'}`,
    '',
    `Generic pirate control (\`Tools/ContentPipeline/calibration/generic-pirate.json\`) vs ${shipped.events.length} shipped events (\`Tuning/Events.json\`). Agent cost $${ledger.calls.reduce((a, c) => a + c.cost, 0).toFixed(2)}.`,
    '',
    ...checks.map(([what, ok]) => `- ${ok ? 'PASS' : 'FAIL'} ${what}`),
    '',
    `| Set | Score | ${DIMENSIONS.map((d) => d.key).join(' | ')} |`, `|---|---|${DIMENSIONS.map(() => '---').join('|')}|`,
    ...[['control', g], ['shipped', s]].map(([n, r]) => `| ${n} | **${r.scored.score}** | ${r.scored.dims.map((d) => d.score).join(' | ')} |`),
    '',
    '## Control', '', g.scored.reason, '', eventsMarkdown(control.events, g.eventScores), '',
    '## Shipped', '', s.scored.reason, '', eventsMarkdown(shipped.events, s.eventScores), '',
  ].join('\n'));
  fs.writeFileSync(path.join(dir, 'stamp.json'), JSON.stringify({ key: calibrationKey(ctx, args), pass, control: g.scored.score, shipped: s.scored.score, checks: checks.map(([what, ok]) => ({ what, ok })) }, null, 2) + '\n');
  for (const [what, ok] of checks) console.log(`   ${ok ? 'PASS' : 'FAIL'} ${what}`);
  console.log(`CALIBRATION ${pass ? 'PASS' : 'FAIL'}: ${path.relative(args.root, path.join(dir, 'report.md'))}`);
  return pass;
}

/**
 * Everything the evaluator's judgement depends on: the prompt it is given (style guide, facts, GDD, rubric code), its
 * agent definition, the scoring code, the control set and the threshold. If any of it changes, the last calibration
 * no longer proves anything and `run` recalibrates first.
 */
function calibrationKey(ctx, args) {
  const h = crypto.createHash('sha256');
  const read = (p) => fs.readFileSync(p, 'utf8').replace(/\r\n/g, '\n');
  for (const part of [styleForPrompt(ctx.guide), factsForPrompt(ctx.facts), ctx.facts.gddMechanics,
    read(path.join(args.root, '.claude', 'agents', 'content-critic.md')), read(path.join(__dirname, 'lib', 'score.js')), read(CONTROL),
    evaluatorPrompt.toString(), commonSections.toString(), formatSpec.toString(), JSON.stringify(EVAL_SCHEMA), String(args.threshold)]) {
    h.update(part).update('|');
  }
  return h.digest('hex').slice(0, 16);
}

/** The loop only trusts an evaluator whose current calibration passed. Returns why it (re)calibrated, or null. */
function ensureCalibrated(ctx, args, ledger) {
  const stampPath = path.join(args.out, 'calibration', 'stamp.json');
  const stamp = fs.existsSync(stampPath) ? JSON.parse(fs.readFileSync(stampPath, 'utf8')) : null;
  const key = calibrationKey(ctx, args);
  if (stamp && stamp.key === key && stamp.pass) { console.log(`calibration: current (${key}), control ${stamp.control} vs shipped ${stamp.shipped}`); return null; }
  const why = !stamp ? 'never calibrated' : stamp.key !== key ? "the evaluator's inputs changed" : 'last calibration failed';
  console.log(`calibration: ${why} - calibrating before the loop`);
  if (!calibrate(ctx, args, ledger)) throw new Error(`evaluator calibration FAIL - not running the loop on scores that can't tell Deadwater Run from generic content. See ${path.relative(args.root, path.join(args.out, 'calibration', 'report.md'))}`);
  return why;
}

function main() {
  const args = parseArgs(process.argv);
  const facts = buildFacts(args.root);
  const guide = buildStyleGuide(args.root, facts);
  const ctx = { facts, guide };
  switch (args.cmd) {
    case 'facts':
      console.log(factsForPrompt(facts));
      return;
    case 'style': {
      const p = path.join(args.root, 'Docs', 'ContentPipeline', 'style-guide.md');
      fs.writeFileSync(p, styleMarkdown(guide));
      console.log(`style guide: ${guide.rules.length} rules from ${guide.sources.length} sources, stranger test PASS -> ${path.relative(args.root, p)}`);
      return;
    }
    case 'check': {
      if (!args.file) throw new Error('check <draft.json>');
      const draft = JSON.parse(fs.readFileSync(args.file, 'utf8'));
      const f = checkEvents(draft.events, facts);
      for (const x of f) console.log(`${x.severity.padEnd(7)} ${x.event} ${x.field}: ${x.rule} - ${x.problem}`);
      console.log(f.some(blocking) ? 'CHECK FAIL' : 'CHECK PASS');
      process.exitCode = f.some(blocking) ? 1 : 0;
      return;
    }
    case 'run': {
      const briefs = loadBriefs();
      const todo = args.all ? briefs : briefs.filter((b) => b.id === args.batch);
      if (!todo.length) throw new Error(`run --all or --batch <${briefs.map((b) => b.id).join('|')}>`);
      const ledger = { cost: 0, calls: [] };
      if (!args.noCalibrate) ensureCalibrated(ctx, args, ledger);
      const results = todo.map((b) => runBatch(b, ctx, args, ledger));
      writeSummary(args, facts);
      console.log(`\nLOOP DONE: ${results.length} batch(es), ${results.filter((r) => r.accepted).length} accepted, scores ${results.map((r) => r.best.scored.score).join('/')}, ${ledger.calls.length} agent calls, $${ledger.cost.toFixed(2)}`);
      return;
    }
    case 'calibrate': {
      const ledger = { cost: 0, calls: [] };
      process.exitCode = calibrate(ctx, args, ledger) ? 0 : 1;
      return;
    }
    case 'report':
      writeSummary(args, facts);
      return;
    default:
      console.log('usage: content-pipeline.js facts | style | check <draft.json> | run --batch <id> | run --all | calibrate | report');
      process.exitCode = 2;
  }
}

if (require.main === module) {
  try { main(); } catch (e) { console.error(`ERROR: ${e.message}`); process.exit(1); }
}

module.exports = { generatorPrompt, evaluatorPrompt, refinerPrompt, eventsMarkdown, extractJson, shapeErrors, enforceLocks, lockedIds, EVENT_SCHEMA, EVAL_SCHEMA, REFINE_SCHEMA };
