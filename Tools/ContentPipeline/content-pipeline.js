#!/usr/bin/env node
/*
 * Deadwater Run - dynamic content pipeline (events)
 *
 *   brief ─► event-writer agent ─► code checks (lib/checks.js) + content-critic agent ─► revise ─► final draft
 *              ▲                          (both read game facts from the repo: lib/facts.js)            │
 *              └──────────────── blocker/major findings go back to the writer ◄──────────────────────────┘
 *
 *   node Tools/ContentPipeline/content-pipeline.js facts                  # print the facts the agents get
 *   node Tools/ContentPipeline/content-pipeline.js check <draft.json>     # code checks only (no agents)
 *   node Tools/ContentPipeline/content-pipeline.js run --batch early-waters
 *   node Tools/ContentPipeline/content-pipeline.js run --all              # every brief in briefs.json, then the pool report
 *   node Tools/ContentPipeline/content-pipeline.js report                 # rebuild out/summary.md from out/
 *
 * Options: --root <repo>  --out <dir> (default Docs/ContentPipeline/out)  --revisions N (default 1)
 *          --budget-usd N (per agent call, default 3)  --total-budget-usd N (default 20)
 * `run` launches Claude Code agents (`claude -p --agent ...`) and costs money.
 * CONTENT_PIPELINE_CLAUDE=<stub.js> swaps `claude` for a stub (test/pipeline.test.js).
 *
 * Output is drafts only. Nothing goes into Content/ - Brandon approves and authors every event (N4).
 * Zero dependencies: Node 18+.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const { buildFacts, factsForPrompt } = require('./lib/facts');
const { checkEvents, checkPool, OUTCOME_FIELDS, REQUIREMENT_FIELDS } = require('./lib/checks');

const DEFAULT_ROOT = path.resolve(__dirname, '..', '..');
const BRIEFS = path.join(__dirname, 'briefs.json');
const WRITER = 'event-writer';
const CRITIC = 'content-critic';

// ------------------------------------------------------------------------------------------------
// CLI
// ------------------------------------------------------------------------------------------------

function parseArgs(argv) {
  const a = { cmd: argv[2], file: null, root: DEFAULT_ROOT, out: null, batch: null, all: false, revisions: 1, budgetUsd: 3, totalBudgetUsd: 20 };
  for (let i = 3; i < argv.length; i++) {
    const k = argv[i];
    const next = () => argv[++i];
    switch (k) {
      case '--root': a.root = path.resolve(next()); break;
      case '--out': a.out = path.resolve(next()); break;
      case '--batch': a.batch = next(); break;
      case '--all': a.all = true; break;
      case '--revisions': a.revisions = +next(); break;
      case '--budget-usd': a.budgetUsd = +next(); break;
      case '--total-budget-usd': a.totalBudgetUsd = +next(); break;
      default:
        if (k.startsWith('--')) throw new Error(`Unknown option ${k}`);
        a.file = k;
    }
  }
  a.out = a.out || path.join(a.root, 'Docs', 'ContentPipeline', 'out');
  return a;
}

// ------------------------------------------------------------------------------------------------
// Output schemas (shape only - game rules are enforced by lib/checks.js so they have one source)
// ------------------------------------------------------------------------------------------------

const EVENT_SCHEMA = {
  type: 'object',
  required: ['events', 'notes'],
  properties: {
    events: {
      type: 'array',
      items: {
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
      },
    },
    notes: { type: 'string' },
  },
};

const FINDING = {
  type: 'object',
  required: ['severity', 'event', 'field', 'rule', 'problem', 'fix'],
  properties: {
    severity: { type: 'string', enum: ['blocker', 'major', 'minor'] },
    event: { type: 'string' }, field: { type: 'string' }, rule: { type: 'string' }, problem: { type: 'string' }, fix: { type: 'string' },
  },
};

const REVIEW_SCHEMA = {
  type: 'object',
  required: ['verdict', 'voice', 'findings', 'code_check_disputes', 'open_questions'],
  properties: {
    verdict: { type: 'string', enum: ['pass', 'revise'] },
    voice: {
      type: 'object',
      required: ['score', 'sounds_like_deadwater_run', 'why', 'weakest_lines'],
      properties: {
        score: { type: 'integer', minimum: 1, maximum: 5 },
        sounds_like_deadwater_run: { type: 'boolean' },
        why: { type: 'string' },
        weakest_lines: { type: 'array', items: { type: 'string' } },
      },
    },
    findings: { type: 'array', items: FINDING },
    code_check_disputes: { type: 'array', items: { type: 'object', required: ['rule', 'event', 'why'], properties: { rule: { type: 'string' }, event: { type: 'string' }, why: { type: 'string' } } } },
    open_questions: { type: 'array', items: { type: 'string' } },
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

// `claude -p --agent` does not enforce --json-schema (no structured_output in the reply, seen 2026-11-17), so the
// schema is also stated in the prompt, and callAgentChecked() verifies the shape.
const outputSection = (schema, example) => section('Output format (required)', [
  'Reply with one JSON object and nothing else. Use exactly these top-level keys - no extra or renamed keys.',
  '', 'JSON Schema:', '```json\n' + JSON.stringify(schema) + '\n```',
  ...(example ? ['', 'Shape:', '```json\n' + JSON.stringify(example, null, 2) + '\n```'] : []),
].join('\n'));

const REVIEW_EXAMPLE = {
  verdict: 'revise',
  voice: { score: 4, sounds_like_deadwater_run: true, why: '<one or two sentences>', weakest_lines: ['<quoted line>'] },
  findings: [{ severity: 'major', event: 'DA_Event_X', field: 'choices[0].outcomes[1]', rule: '<short rule name>', problem: '<what is wrong>', fix: '<concrete fix>' }],
  code_check_disputes: [],
  open_questions: ['<question for Brandon>'],
};

function writerPrompt(brief, facts, others, revision) {
  return [
    `Write ${brief.count} Deadwater Run map events for this brief. Return only JSON in the given schema.`,
    '',
    section('Brief', JSON.stringify(brief, null, 2)),
    section('Game facts (from the repo - the only rules, names and numbers that exist)', '```json\n' + factsForPrompt(facts) + '\n```'),
    section('Event data format', formatSpec()),
    section('Voice sheet (Docs/ContentPipeline/voice.md)', facts.voice),
    section('GDD - Game Mechanics (DeadwaterRun.md)', facts.gddMechanics),
    section('Events already written in other batches (don\'t repeat their ideas or ids)', others.length ? others.map((e) => `- ${e.id}: ${e.title}`).join('\n') : '(none)'),
    revision ? section('Revision - your previous draft and the review', [
      'Fix every blocker and major finding. Keep what the critic didn\'t flag. Explain each fix (or why you disagree) in `notes`.',
      '', '### Previous draft', '```json\n' + JSON.stringify(revision.draft, null, 2) + '\n```',
      '', '### Findings to fix', '```json\n' + JSON.stringify(revision.findings, null, 2) + '\n```',
    ].join('\n')) : '',
    outputSection(EVENT_SCHEMA),
  ].join('\n');
}

function criticPrompt(brief, facts, draft, codeFindings) {
  return [
    'Review this generated Deadwater Run content. Return only JSON in the given schema.',
    '',
    section('Content type', 'Map events (UDwEventDefinition, Docs/Architecture/11-Events.md)'),
    section('Brief it was written for', JSON.stringify(brief, null, 2)),
    section('Content', '```json\n' + JSON.stringify(draft.events, null, 2) + '\n```\n\nWriter\'s notes: ' + (draft.notes || '(none)')),
    section('Code-check results (already enforced - don\'t repeat them; dispute any that are wrong)', '```json\n' + JSON.stringify(codeFindings, null, 2) + '\n```'),
    section('Game facts (from the repo - ground truth)', '```json\n' + factsForPrompt(facts) + '\n```'),
    section('Event data format', formatSpec()),
    section('Voice sheet (Docs/ContentPipeline/voice.md)', facts.voice),
    section('GDD - Game Mechanics (DeadwaterRun.md)', facts.gddMechanics),
    outputSection(REVIEW_SCHEMA, REVIEW_EXAMPLE),
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

function callAgent(agent, prompt, schema, args, ledger) {
  if (ledger.cost >= args.totalBudgetUsd) throw new Error(`total budget $${args.totalBudgetUsd} reached ($${ledger.cost.toFixed(2)})`);
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

/** Top-level shape check: required keys of the schema and its required object children. */
function shapeErrors(data, schema, at = '$') {
  if (schema.type === 'object') {
    if (!data || typeof data !== 'object' || Array.isArray(data)) return [`${at} is not an object`];
    return (schema.required || []).flatMap((k) => (data[k] === undefined ? [`${at}.${k} is missing`] : shapeErrors(data[k], schema.properties[k], `${at}.${k}`)));
  }
  if (schema.type === 'array' && !Array.isArray(data)) return [`${at} is not an array`];
  return [];
}

/** Calls the agent; if the reply misses the schema's shape, asks once more with the errors. */
function callAgentChecked(agent, prompt, schema, args, ledger) {
  let data = callAgent(agent, prompt, schema, args, ledger);
  let errs = shapeErrors(data, schema);
  if (errs.length) {
    console.log(`   ${agent}: reply missed the schema (${errs.slice(0, 3).join('; ')}) - asking again`);
    data = callAgent(agent, `${prompt}\n\n## Your previous reply was rejected\n\nIt did not match the output schema: ${errs.join('; ')}. Return the complete JSON object with every required key at the top level.`, schema, args, ledger);
    errs = shapeErrors(data, schema);
    if (errs.length) throw new Error(`${agent} reply still misses the schema: ${errs.join('; ')}`);
  }
  return data;
}

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
    case 'RandomOf': return `RandomOf [ ${(o.options || []).map((x) => `${x.weight}: ${(x.outcomes || []).map(fmtOutcome).join(', ') || 'nothing'}`).join(' | ')} ]`;
    default: return `${o.type}?? ${JSON.stringify(o)}`;
  }
}
const fmtReq = (r) => `${r.type} ${r.amount ?? r.count ?? r.item ?? r.captain ?? ''}`.trim();

function eventsMarkdown(events) {
  return events.map((e) => [
    `### ${e.title}  \`${e.id}\``,
    '```',
    `Title:    ${e.title}`,
    `Valence:  ${e.valence.padEnd(9)} Layers: ${e.minLayer}-${e.maxLayer}        Weight: ${e.weight}`,
    `Body:     ${e.body}`,
    ...e.choices.map((c, i) => `Choice ${i + 1}: ${c.label}  ->${c.requirements.length ? ` Requires: ${c.requirements.map(fmtReq).join('; ')} ->` : ''} Outcomes: ${c.outcomes.map(fmtOutcome).join('; ') || 'none'}  -> Result: ${c.resultText}`),
    '```',
  ].join('\n')).join('\n\n');
}

const blocking = (f) => f.severity === 'blocker' || f.severity === 'major';

function findingsTable(rows) {
  if (!rows.length) return '_None._';
  return ['| Sev | From | Event | Field | Rule | Problem | Fix |', '|---|---|---|---|---|---|---|',
    ...rows.map((f) => `| ${f.severity} | ${f.source} | ${f.event} | \`${f.field}\` | ${f.rule} | ${String(f.problem).replace(/\|/g, '\\|').replace(/\n/g, ' ')} | ${String(f.fix).replace(/\|/g, '\\|').replace(/\n/g, ' ')} |`)].join('\n');
}

function batchReport(brief, rounds, status, ledger) {
  const last = rounds[rounds.length - 1];
  const fixed = rounds.length > 1 ? rounds[0].findings.filter(blocking) : [];
  return [
    `# Batch \`${brief.id}\` - ${status}`,
    '',
    `Brief: ${brief.theme}`,
    `Layers ${brief.layers.join('-')}, ${brief.count} events (${Object.entries(brief.valences).map(([k, v]) => `${v} ${k}`).join(', ')}).`,
    `Rounds: ${rounds.length}. Agent cost: $${ledger.calls.reduce((s, c) => s + c.cost, 0).toFixed(2)} over ${ledger.calls.length} calls.`,
    '',
    `**Sounds like Deadwater Run?** ${last.review.voice.sounds_like_deadwater_run ? 'Yes' : 'No'} - ${last.review.voice.score}/5 (final round). ${last.review.voice.why}`,
    ...(last.review.voice.weakest_lines.length ? ['', 'Weakest lines (critic):', ...last.review.voice.weakest_lines.map((l) => `- ${l}`)] : []),
    '',
    ...rounds.flatMap((r, i) => [
      `## Round ${i + 1} review - critic verdict: ${r.review.verdict}, voice ${r.review.voice.score}/5`,
      '',
      findingsTable(r.findings),
      ...(r.review.code_check_disputes.length ? ['', 'Critic disputes of code checks:', ...r.review.code_check_disputes.map((d) => `- ${d.rule} on ${d.event}: ${d.why}`)] : []),
      '',
    ]),
    ...(fixed.length ? ['## Writer\'s notes on the revision', '', rounds[rounds.length - 1].draft.notes || '(none)', ''] : []),
    '## Open questions for Brandon',
    '',
    ...(rounds.flatMap((r) => r.review.open_questions).filter((q, i, a) => a.indexOf(q) === i).map((q) => `- ${q}`)),
    '',
    '## Final events',
    '',
    eventsMarkdown(last.draft.events),
    '',
  ].join('\n');
}

// ------------------------------------------------------------------------------------------------
// Pipeline
// ------------------------------------------------------------------------------------------------

function loadBriefs() { return JSON.parse(fs.readFileSync(BRIEFS, 'utf8')).briefs; }

function finalEvents(out, exceptBatch) {
  if (!fs.existsSync(out)) return [];
  return fs.readdirSync(out).filter((d) => d !== exceptBatch && fs.existsSync(path.join(out, d, 'final.json')))
    .flatMap((d) => JSON.parse(fs.readFileSync(path.join(out, d, 'final.json'), 'utf8')).events);
}

function runBatch(brief, facts, args, ledger) {
  console.log(`\n=== batch ${brief.id}: ${brief.count} events, layers ${brief.layers.join('-')} ===`);
  const dir = path.join(args.out, brief.id);
  fs.rmSync(dir, { recursive: true, force: true });
  fs.mkdirSync(dir, { recursive: true });
  const save = (name, obj) => fs.writeFileSync(path.join(dir, name), typeof obj === 'string' ? obj : JSON.stringify(obj, null, 2) + '\n');
  const others = finalEvents(args.out, brief.id);
  const local = { cost: 0, calls: [] };
  ledger.rawDir = path.join(dir, 'calls');
  const call = (agent, prompt, schema) => {
    const before = ledger.calls.length;
    const data = callAgentChecked(agent, prompt, schema, args, ledger);
    local.calls.push(...ledger.calls.slice(before));
    return data;
  };

  const rounds = [];
  let draft = call(WRITER, writerPrompt(brief, facts, others, null), EVENT_SCHEMA);
  for (let round = 1; ; round++) {
    save(`draft-${round}.json`, draft);
    const code = checkEvents(draft.events, facts, brief, others.map((e) => e.id));
    const review = call(CRITIC, criticPrompt(brief, facts, draft, code), REVIEW_SCHEMA);
    const findings = [...code, ...review.findings.map((f) => ({ source: 'critic', ...f }))];
    save(`review-${round}.json`, { code, critic: review });
    rounds.push({ draft, review, findings });
    const open = findings.filter(blocking);
    console.log(`   round ${round}: ${code.filter(blocking).length} code + ${review.findings.filter(blocking).length} critic blocker/major, voice ${review.voice.score}/5`);
    if (!open.length || round > args.revisions) break;
    draft = call(WRITER, writerPrompt(brief, facts, others, { draft, findings: open }), EVENT_SCHEMA);
  }
  const last = rounds[rounds.length - 1];
  const status = last.findings.some(blocking) ? 'needs Brandon (blocker/major left after revisions)' : 'ready for Brandon\'s review';
  save('final.json', last.draft);
  save('report.md', batchReport(brief, rounds, status, local));
  console.log(`   -> ${status}; ${path.relative(args.root, path.join(dir, 'report.md'))}`);
  return { brief, rounds, status, cost: local.calls.reduce((s, c) => s + c.cost, 0) };
}

function writeSummary(args, facts) {
  const briefs = loadBriefs();
  const rows = [];
  const all = [];
  for (const b of briefs) {
    const dir = path.join(args.out, b.id);
    if (!fs.existsSync(path.join(dir, 'final.json'))) { rows.push(`| \`${b.id}\` | not run | | | | |`); continue; }
    const final = JSON.parse(fs.readFileSync(path.join(dir, 'final.json'), 'utf8'));
    const reviews = fs.readdirSync(dir).filter((f) => /^review-\d+\.json$/.test(f)).sort()
      .map((f) => JSON.parse(fs.readFileSync(path.join(dir, f), 'utf8')));
    const first = reviews[0]; const last = reviews[reviews.length - 1];
    const n = (r) => [...r.code, ...r.critic.findings].filter(blocking).length;
    all.push(...final.events);
    rows.push(`| [\`${b.id}\`](${b.id}/report.md) | ${final.events.length} | ${reviews.length} | ${n(first)} -> ${n(last)} | ${first.critic.voice.score} -> ${last.critic.voice.score} | ${n(last) ? 'needs Brandon' : 'ready for review'} |`);
  }
  const pool = checkPool(all, facts);
  const md = [
    '# Content pipeline - event pool summary',
    '',
    'Generated by `node Tools/ContentPipeline/content-pipeline.js report`. Drafts only, not in the game.',
    '',
    '| Batch | Events | Rounds | Blocker+major (first -> last) | Voice /5 (first -> last) | Status |',
    '|---|---|---|---|---|---|',
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

function main() {
  const args = parseArgs(process.argv);
  const facts = buildFacts(args.root);
  switch (args.cmd) {
    case 'facts':
      console.log(factsForPrompt(facts));
      return;
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
      const results = todo.map((b) => runBatch(b, facts, args, ledger));
      writeSummary(args, facts);
      console.log(`\nPIPELINE DONE: ${results.length} batch(es), ${ledger.calls.length} agent calls, $${ledger.cost.toFixed(2)}`);
      return;
    }
    case 'report':
      writeSummary(args, facts);
      return;
    default:
      console.log('usage: content-pipeline.js facts | check <draft.json> | run --batch <id> | run --all | report');
      process.exitCode = 2;
  }
}

if (require.main === module) {
  try { main(); } catch (e) { console.error(`ERROR: ${e.message}`); process.exit(1); }
}

module.exports = { writerPrompt, criticPrompt, eventsMarkdown, extractJson, EVENT_SCHEMA, REVIEW_SCHEMA };
