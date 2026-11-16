#!/usr/bin/env node
/*
 * Deadwater Run - GDD coding agent
 *
 * Reads the Game Design Document (DeadwaterRun.md), the v1.0 scope (Docs/Scope.md) and the ticket plan
 * (Docs/Architecture/Roadmap.md), scans the codebase, finds which features are missing, ranks them with
 * priority_score(), hands the top one to the agent that owns it (Claude Code, `claude -p --agent <owner>`),
 * re-scans, and repeats until every code requirement is met.
 *
 *   node Tools/GddAgent/gdd-agent.js                      # plan: gap report + ranked queue (no writes)
 *   node Tools/GddAgent/gdd-agent.js --simulate           # plan the whole build order (marks picks done in memory)
 *   node Tools/GddAgent/gdd-agent.js --run --verify       # write code: loop until done (launches agents - ask Brandon first)
 *
 * Options: --root <repo>  --today YYYY-MM-DD  --top N  --max-iterations N  --retries N  --budget-usd N
 *          --verify (Build.ps1 + Test.ps1 after each write)  --json <file> (write the full report)  --explain DW-xxx
 *
 * Zero dependencies: Node 18+.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

// ------------------------------------------------------------------------------------------------
// Config
// ------------------------------------------------------------------------------------------------

const DEFAULT_ROOT = path.resolve(__dirname, '..', '..');
const REQUIREMENTS_FILE = path.join(__dirname, 'requirements.json');

/** Agents whose deliveries are code/content the agent can verify by scanning. The rest produce reports,
 *  builds, playtests or purchases: they are tracked but never picked by the code-writing loop. */
const CODE_AGENTS = new Set(['gameplay-engineer', 'tools-engineer', 'ai-engineer', 'ui-engineer', 'ui-juice-engineer']);

/** Folders scanned for evidence. Content is scanned by file name only (binary assets). */
const SCAN_DIRS = ['Source', 'Tools', 'Docs', 'Tuning', 'Config', 'Build', 'Content/DeadwaterRun'];
const SKIP_DIRS = new Set(['.git', 'Binaries', 'Intermediate', 'Saved', 'DerivedDataCache', 'node_modules', '__ExternalActors__', '__ExternalObjects__']);
const TEXT_EXT = new Set(['.h', '.cpp', '.cs', '.ps1', '.py', '.json', '.ini', '.md', '.js', '.vdf', '.txt']);

/** priority_score weights - every term is printed by --explain so a pick can be audited. */
const W = {
  gddFeature: 1.0,      // ticket implements a GDD section (player-facing)
  toolingOnly: 0.6,     // tooling with no GDD section
  critical: 1.5,        // on the Roadmap's critical path (gantt :crit)
  milestone: 2.0,       // needed by the earliest unmet weekly exit criterion (Scope.md timeline)
  perDependent: 0.25,   // each ticket (transitively) waiting on this one
  latePerWeek: 1.0,     // urgency grows by this per week past the planned end, capped at maxUrgency
  maxUrgency: 2.0,
  soonUrgency: 1.2,     // planned end within the next 7 days
  momentum: 0.5,        // bonus x coverage for half-finished work
};

const DONE_COVERAGE = 1.0;   // every probe must pass
const STOPWORDS = new Set(('the and for with from into onto that this then than each every all one two three full v1 its '
  + 'placeholder placeholders playable start finish first final pass add new real only per shared core basic content '
  + 'based week until after before same also more most none plus both').split(' '));

// ------------------------------------------------------------------------------------------------
// CLI
// ------------------------------------------------------------------------------------------------

function parseArgs(argv) {
  const a = { root: DEFAULT_ROOT, today: null, top: 10, maxIterations: 20, retries: 2, budgetUsd: 15,
    mode: 'plan', verify: false, json: null, explain: null };
  for (let i = 2; i < argv.length; i++) {
    const k = argv[i];
    const next = () => argv[++i];
    switch (k) {
      case '--root': a.root = path.resolve(next()); break;
      case '--today': a.today = next(); break;
      case '--top': a.top = +next(); break;
      case '--max-iterations': a.maxIterations = +next(); break;
      case '--retries': a.retries = +next(); break;
      case '--budget-usd': a.budgetUsd = +next(); break;
      case '--simulate': a.mode = 'simulate'; break;
      case '--run': a.mode = 'run'; break;
      case '--verify': a.verify = true; break;
      case '--json': a.json = next(); break;
      case '--explain': a.explain = next().toUpperCase(); break;
      case '-h': case '--help': console.log(fs.readFileSync(__filename, 'utf8').split('*/')[0]); process.exit(0);
      default: throw new Error(`Unknown option ${k}`);
    }
  }
  const local = new Date();
  a.today = new Date((a.today || `${local.getFullYear()}-${String(local.getMonth() + 1).padStart(2, '0')}-${String(local.getDate()).padStart(2, '0')}`) + 'T12:00:00Z');
  return a;
}

const read = (root, rel) => fs.readFileSync(path.join(root, rel), 'utf8').replace(/\r\n/g, '\n');
const day = 86400000;
const iso = (d) => d.toISOString().slice(0, 10);
const words = (s) => (s.toLowerCase().match(/[a-z][a-z0-9]+/g) || []).filter((w) => w.length >= 4 && !STOPWORDS.has(w));

// ------------------------------------------------------------------------------------------------
// 1. Read the GDD, the scope and the ticket plan
// ------------------------------------------------------------------------------------------------

/** DeadwaterRun.md -> [{ heading, path, text }] for every heading under "Game Mechanics". */
function readGdd(root) {
  const md = read(root, 'DeadwaterRun.md');
  const sections = [];
  const stack = [];
  let cur = null;
  let inMechanics = false;
  for (const line of md.split('\n')) {
    const h = /^(#{2,4})\s+(.*?)\s*:?\s*$/.exec(line);
    if (h) {
      const level = h[1].length;
      if (level === 2) inMechanics = /game mechanics/i.test(h[2]);
      while (stack.length && stack[stack.length - 1].level >= level) stack.pop();
      stack.push({ level, title: h[2] });
      cur = inMechanics && level > 2 ? { heading: h[2], path: stack.map((s) => s.title).join(' > '), text: '' } : null;
      if (cur) sections.push(cur);
      continue;
    }
    if (cur) cur.text += line + '\n';
  }
  // A parent heading also owns its children's text (e.g. "Captain Choices" > "Team Leader").
  for (const s of sections) {
    s.fullText = sections.filter((c) => c.path.startsWith(s.path)).map((c) => c.heading + '\n' + c.text).join('\n');
  }
  return sections;
}

/** Docs/Scope.md -> must-ship bullets and the weekly exit criteria. */
function readScope(root) {
  const md = read(root, 'Docs/Scope.md');
  const mustShip = [];
  const weeks = [];
  let section = '';
  for (const line of md.split('\n')) {
    if (line.startsWith('## ')) { section = line.slice(3).trim(); continue; }
    if (/^must ship/i.test(section)) {
      const m = /^- \*\*(.+?):\*\*\s*(.*)$/.exec(line);
      if (m) mustShip.push({ area: m[1], text: m[2] });
    }
    if (/^timeline/i.test(section)) {
      const m = /^\|\s*(\w+)\s*\|\s*([^|]+)\|\s*([^|]+)\|/.exec(line);
      if (m && /^\d/.test(m[1])) weeks.push({ week: m[1], dates: m[2].trim(), exit: m[3].replace(/\*\*/g, '').trim() });
    }
  }
  return { mustShip, weeks };
}

/** Docs/Architecture/Roadmap.md -> tickets (gantt: week, dates, deps, critical; ticket blocks: owner, acceptance). */
function readRoadmap(root) {
  const md = read(root, 'Docs/Architecture/Roadmap.md');
  const tickets = new Map();
  const ensure = (id) => {
    if (!tickets.has(id)) tickets.set(id, { id, goal: '', owner: '', context: '', acceptance: '', proof: '', outOfScope: '',
      week: null, start: null, end: null, days: 1, deps: [], critical: false });
    return tickets.get(id);
  };

  // Gantt rows: "DW-209 Placeholder Kraken   :w209, after w201, 1d"  /  ":crit, w101, after w004, 2d"
  const gantt = /```mermaid\s*\ngantt([\s\S]*?)```/.exec(md);
  if (!gantt) throw new Error('Roadmap.md has no gantt chart');
  const alias = new Map();     // w209 -> DW-209
  const rows = [];
  let week = null;
  for (const line of gantt[1].split('\n')) {
    const sec = /^\s*section\s+Week\s+(\w+)/.exec(line);
    if (sec) { week = sec[1]; continue; }
    const row = /^\s*(DW-\d+)\s+(.+?)\s*:(.+)$/.exec(line);
    if (!row) continue;
    const parts = row[3].split(',').map((s) => s.trim());
    const t = ensure(row[1]);
    t.goal = t.goal || row[2];
    t.week = week;
    t.critical = parts.includes('crit');
    const key = parts.find((p) => /^w\d+$/.test(p));
    if (key) alias.set(key, t.id);
    rows.push({ t, parts });
  }
  const byId = new Map();
  for (const { t, parts } of rows) {
    const date = parts.find((p) => /^\d{4}-\d\d-\d\d$/.test(p));
    const after = parts.filter((p) => p.startsWith('after ')).flatMap((p) => p.slice(6).split(/\s+/));
    const dur = parts.find((p) => /^\d+d$/.test(p));
    t.days = dur ? parseInt(dur, 10) : 1;
    t.deps = after.map((w) => alias.get(w)).filter(Boolean);
    t.start = date ? new Date(date + 'T12:00:00Z') : null;
    byId.set(t.id, t);
  }
  // Resolve "after" start dates (gantt semantics: start = latest end of the dependencies).
  const resolve = (t, seen = new Set()) => {
    if (t.start) return t.start;
    if (seen.has(t.id)) return null;
    seen.add(t.id);
    let latest = null;
    for (const d of t.deps) {
      const dt = byId.get(d);
      const s = dt && resolve(dt, seen);
      const e = s && new Date(+s + dt.days * day);
      if (e && (!latest || e > latest)) latest = e;
    }
    t.start = latest;
    return latest;
  };
  for (const t of byId.values()) { resolve(t); t.end = t.start ? new Date(+t.start + t.days * day) : null; }

  // Ticket blocks (Orchestration.md format).
  const blockRe = /```\n(Agent:[\s\S]*?)```/g;
  let b;
  while ((b = blockRe.exec(md))) {
    const fields = {};
    let key = null;
    for (const line of b[1].split('\n')) {
      const m = /^(Agent|Goal|Context|Acceptance|Proof|Out of scope):\s*(.*)$/.exec(line);
      if (m) { key = m[1]; fields[key] = m[2]; } else if (key && line.trim()) fields[key] += '\n' + line.trim();
    }
    const id = /DW-\d+/.exec(fields.Goal || '');
    if (!id) continue;
    const t = ensure(id[0]);
    Object.assign(t, { owner: fields.Agent.trim(), goal: fields.Goal.replace(id[0], '').trim(), context: fields.Context || '',
      acceptance: fields.Acceptance || '', proof: fields.Proof || '', outOfScope: fields['Out of scope'] || '' });
  }
  // Coarse weeks 3-6 table: | ID | Agent | Goal | Context | Acceptance / Proof | Out of scope |
  for (const line of md.split('\n')) {
    const m = /^\|\s*(DW-\d+)\s*\|([^|]+)\|([^|]+)\|([^|]+)\|([^|]+)\|([^|]+)\|/.exec(line);
    if (!m) continue;
    const t = ensure(m[1]);
    if (t.acceptance) continue;
    Object.assign(t, { owner: m[2].trim(), goal: m[3].trim(), context: m[4].trim(), acceptance: m[5].trim(), outOfScope: m[6].trim() });
  }
  return [...tickets.values()].sort((x, y) => x.id.localeCompare(y.id, undefined, { numeric: true }));
}

// ------------------------------------------------------------------------------------------------
// 2. Requirements: what must exist for each ticket to count as built
// ------------------------------------------------------------------------------------------------

/** Pull identifiers out of a ticket's acceptance text: C++ types, assets, files, test names. */
function extractProbes(text) {
  const probes = [];
  const add = (p) => { if (!probes.some((q) => q.kind === p.kind && q.name === p.name)) probes.push(p); };
  const clean = text.replace(/\(after [^)]*\)/gi, '');
  for (const m of clean.matchAll(/\b([UAFEI]Dw[A-Z]\w*)\b/g)) add({ kind: 'symbol', name: m[1] });
  for (const m of clean.matchAll(/\b((?:DA|GA|GE|WBP|DT|IMC|IA|EUW|SC|SM|SCON|GC|L|BP)_[A-Za-z0-9_*]+)/g)) {
    if (!/^(SC|SM|L)_$/.test(m[1])) add({ kind: 'asset', name: m[1].replace(/_+$/, '') });
  }
  for (const m of clean.matchAll(/\b((?:Tools|Docs|Build|Tuning|Config)\/[A-Za-z0-9_\-./*]+[A-Za-z0-9*])/g)) add({ kind: 'file', name: m[1] });
  for (const m of clean.matchAll(/\b(DeadwaterRun\.[A-Z][\w.]+)/g)) add({ kind: 'test', name: m[1] });
  return probes;
}

function loadRequirements() {
  return fs.existsSync(REQUIREMENTS_FILE) ? JSON.parse(fs.readFileSync(REQUIREMENTS_FILE, 'utf8')) : { tickets: {} };
}

/** Link a ticket to the GDD section that describes it (explicit in requirements.json, else best keyword overlap). */
function linkGdd(ticket, gdd, req) {
  if (req.gdd) return req.gdd === 'none' ? [] : [].concat(req.gdd).map((h) => {
    const s = gdd.find((g) => g.heading.toLowerCase() === h.toLowerCase());
    if (!s) throw new Error(`requirements.json: ${ticket.id} names GDD section "${h}" which is not in DeadwaterRun.md`);
    return s;
  });
  const kw = new Set(words(ticket.goal));
  let best = null;
  for (const s of gdd) {
    const head = new Set(words(s.heading));
    const body = new Set(words(s.fullText));
    let score = 0;
    for (const w of kw) score += head.has(w) ? 2 : body.has(w) ? 1 : 0;
    if (score >= 3 && (!best || score > best.score || (score === best.score && s.path.length > best.s.path.length))) best = { s, score };
  }
  return best ? [best.s] : [];
}

function buildRequirements(tickets, gdd) {
  const reqFile = loadRequirements();
  for (const t of tickets) {
    const r = reqFile.tickets[t.id] || {};
    const auto = r.replaceProbes ? [] : extractProbes(t.acceptance);
    const ignore = new Set(r.ignore || []);
    const rename = r.rename || {};
    t.probes = [...auto.filter((p) => !ignore.has(p.name)).map((p) => (rename[p.name] ? { ...p, name: rename[p.name], renamedFrom: p.name } : p)),
      ...(r.probes || [])];
    t.owner = t.owner || r.owner || '';
    t.writable = (r.writable !== undefined) ? r.writable : t.owner.split(/\s*\/\s*/).some((o) => CODE_AGENTS.has(o.replace(/\s*\(.*\)/, '')));
    t.gdd = linkGdd(t, gdd, r);
    t.note = r.note || '';
  }
  return tickets;
}

// ------------------------------------------------------------------------------------------------
// 3. Scan the codebase
// ------------------------------------------------------------------------------------------------

function scan(root) {
  const files = [];
  const walk = (rel) => {
    const abs = path.join(root, rel);
    if (!fs.existsSync(abs)) return;
    for (const e of fs.readdirSync(abs, { withFileTypes: true })) {
      if (SKIP_DIRS.has(e.name)) continue;
      const r = rel ? rel + '/' + e.name : e.name;
      if (e.isDirectory()) walk(r); else files.push(r);
    }
  };
  for (const d of SCAN_DIRS) walk(d);

  const symbols = new Map();   // name -> file
  const assets = new Map();    // basename -> file
  let sourceText = '';
  for (const f of files) {
    const ext = path.extname(f).toLowerCase();
    if (ext === '.uasset' || ext === '.umap') { assets.set(path.basename(f, ext), f); continue; }
    if (!f.startsWith('Source/') || !TEXT_EXT.has(ext)) continue;
    const text = fs.readFileSync(path.join(root, f), 'utf8');
    sourceText += '\n' + text;
    for (const m of text.matchAll(/^\s*(?:class|struct|enum class|enum)\s+(?:[A-Z_]+_API\s+)?([A-Za-z_]\w*)\s*(?:final\s*)?[:{\n]/gm)) {
      if (!symbols.has(m[1])) symbols.set(m[1], f);
    }
    // UE automation tests: IMPLEMENT_SIMPLE_AUTOMATION_TEST(..., "DeadwaterRun.Run.Kraken.Foo", ...) / BEGIN_DEFINE_SPEC(..., "DeadwaterRun.X", ...)
  }
  // Data assets created by content scripts but referenced by name in C++ count too (generated content lives in Content/).
  return { files, fileSet: new Set(files), symbols, assets, sourceText };
}

function globToRe(g) {
  return new RegExp('^' + g.split('*').map((s) => s.replace(/[.+?^${}()|[\]\\]/g, '\\$&')).join('.*') + '$');
}

function checkProbe(p, idx) {
  switch (p.kind) {
    case 'symbol': return idx.symbols.get(p.name) || null;
    case 'asset': {
      if (!p.name.includes('*')) return idx.assets.get(p.name) || null;
      const re = globToRe(p.name);
      const hits = [...idx.assets.keys()].filter((n) => re.test(n) && !(p.exclude && new RegExp(p.exclude).test(n)));
      return hits.length >= (p.min || 1) ? `${hits.length} match${hits.length > 1 ? 'es' : ''}` : null;
    }
    case 'file': {
      if (!p.name.includes('*')) return idx.fileSet.has(p.name) || idx.files.some((f) => f.startsWith(p.name + '/')) ? p.name : null;
      const re = globToRe(p.name);
      return idx.files.find((f) => re.test(f)) || null;
    }
    case 'test': return idx.sourceText.includes('"' + p.name) ? 'Source/**/Tests' : null;
    case 'text': return new RegExp(p.name).test(idx.sourceText) ? 'Source/**' : null;
    default: throw new Error('Unknown probe kind ' + p.kind);
  }
}

// ------------------------------------------------------------------------------------------------
// 4. Gap detection
// ------------------------------------------------------------------------------------------------

function detectGaps(tickets, idx, assumedDone = new Set()) {
  for (const t of tickets) {
    t.results = t.probes.map((p) => ({ ...p, found: checkProbe(p, idx) }));
    const hit = t.results.filter((r) => r.found).length;
    t.coverage = t.probes.length ? hit / t.probes.length : 0;
    t.missing = t.results.filter((r) => !r.found);
    if (assumedDone.has(t.id)) t.status = 'done';
    else if (!t.probes.length) t.status = 'unverifiable';
    else if (t.coverage >= DONE_COVERAGE) t.status = 'done';
    else if (t.coverage > 0) t.status = 'partial';
    else t.status = 'missing';
  }
  return tickets.filter((t) => t.status !== 'done');
}

/** GDD sections no ticket covers - gaps in the plan itself, reported to the orchestrator. */
function uncoveredGdd(gdd, tickets) {
  const covered = new Set(tickets.flatMap((t) => t.gdd.map((s) => s.path)));
  return gdd.filter((s) => s.text.trim() && ![...covered].some((c) => c === s.path || c.startsWith(s.path + ' >') || s.path.startsWith(c + ' >')));
}

// ------------------------------------------------------------------------------------------------
// 5. priority_score
// ------------------------------------------------------------------------------------------------

function dependents(tickets) {
  const direct = new Map(tickets.map((t) => [t.id, []]));
  for (const t of tickets) for (const d of t.deps) direct.get(d) && direct.get(d).push(t.id);
  const all = new Map();
  for (const t of tickets) {
    const seen = new Set();
    const stack = [...direct.get(t.id)];
    while (stack.length) { const x = stack.pop(); if (!seen.has(x)) { seen.add(x); stack.push(...direct.get(x)); } }
    all.set(t.id, seen);
  }
  return all;
}

/** The earliest week whose exit criterion still has unbuilt code tickets behind it. A ticket serves a week's exit
 *  criterion when it is on the critical path or its goal shares a keyword with the criterion text. */
function currentMilestone(tickets, scope) {
  for (const w of scope.weeks) {
    const weekNum = w.week.replace(/\D.*$/, '');
    const kw = new Set(words(w.exit));
    const serving = tickets.filter((t) => t.week === weekNum && t.writable
      && (t.critical || words(t.goal).some((x) => kw.has(x))));
    const open = serving.filter((t) => t.status !== 'done');
    if (open.length) return { ...w, tickets: new Set(open.map((t) => t.id)) };
  }
  return null;
}

/**
 * priority_score(ticket) -> number. 0 means "not buildable now".
 *
 *   score = 10 * value * critical * milestone * unblocks * urgency * momentum / sqrt(cost)
 *
 *   value     GDD feature 1.0, tooling-only 0.6       (player-facing first)
 *   critical  1.5 on the Roadmap critical path
 *   milestone 2.0 if the earliest unmet weekly exit criterion needs it
 *   unblocks  1 + 0.25 per ticket transitively waiting on it
 *   urgency   1 + 1 per week past its planned end (max 2); 1.2 if due within 7 days
 *   momentum  1 + 0.5 * coverage                      (finish half-built work before starting new work)
 *   cost      planned days; sqrt so small tickets win ties without starving big critical ones
 *
 * Gates (score 0): already done, owner does not write code, a dependency is not built yet, no probes to verify against.
 */
function priority_score(t, ctx) {
  const terms = {};
  const gate = (why) => ({ score: 0, gate: why, terms });
  if (t.status === 'done') return gate('done');
  if (!t.writable) return gate(`owner ${t.owner || '?'} does not write code`);
  if (t.status === 'unverifiable') return gate('no requirement probes (cannot tell when it is done)');
  const blockers = t.deps.filter((d) => ctx.status.get(d) !== 'done');
  if (blockers.length) return gate('blocked by ' + blockers.join(', '));

  terms.value = t.gdd.length ? W.gddFeature : W.toolingOnly;
  terms.critical = t.critical ? W.critical : 1;
  terms.milestone = ctx.milestone && ctx.milestone.tickets.has(t.id) ? W.milestone : 1;
  terms.unblocks = 1 + W.perDependent * ctx.dependents.get(t.id).size;
  if (t.end) {
    const late = (ctx.today - t.end) / day;
    terms.urgency = late > 0 ? Math.min(W.maxUrgency, 1 + W.latePerWeek * late / 7) : late > -7 ? W.soonUrgency : 1;
  } else terms.urgency = 1;
  terms.momentum = 1 + W.momentum * t.coverage;
  terms.cost = Math.max(1, t.days);
  const score = 10 * terms.value * terms.critical * terms.milestone * terms.unblocks * terms.urgency * terms.momentum / Math.sqrt(terms.cost);
  return { score: Math.round(score * 100) / 100, gate: null, terms };
}

function rank(tickets, scope, today) {
  const ctx = { today, status: new Map(tickets.map((t) => [t.id, t.status])), dependents: dependents(tickets), milestone: null };
  ctx.milestone = currentMilestone(tickets, scope);
  for (const t of tickets) Object.assign(t, { priority: priority_score(t, ctx) });
  const queue = tickets.filter((t) => t.priority.score > 0).sort((a, b) => b.priority.score - a.priority.score || a.id.localeCompare(b.id));
  return { queue, milestone: ctx.milestone };
}

// ------------------------------------------------------------------------------------------------
// 6. Code-writing step: hand the ticket to the owning Claude Code agent
// ------------------------------------------------------------------------------------------------

function ticketPrompt(t, retryMissing) {
  const gddText = t.gdd.map((s) => `### ${s.path}\n${s.fullText.trim()}`).join('\n\n') || '(tooling ticket - no GDD section)';
  const probes = t.probes.map((p) => `- ${p.kind}: ${p.name}`).join('\n');
  return [
    `You are picking up ticket ${t.id} from the GDD coding agent (Tools/GddAgent). Follow your agent definition, Docs/Orchestration.md`,
    `delivery rules and CLAUDE.md. Work on branch ${branchName(t)} (already checked out). Commit when done; never push.`,
    '',
    '```',
    `Agent:       ${t.owner}`,
    `Goal:        ${t.id} ${t.goal}`,
    `Context:     ${t.context}`,
    `Acceptance:  ${t.acceptance}`,
    `Proof:       ${t.proof || 'Tools/Build.ps1 and Tools/Test.ps1 output'}`,
    `Out of scope: ${t.outOfScope}`,
    '```',
    '',
    '## GDD (DeadwaterRun.md) - source of truth for rules and numbers',
    gddText,
    '',
    '## Requirements the agent will re-scan for after you finish (all must exist)',
    probes,
    retryMissing ? `\n## Previous attempt is incomplete. Still missing:\n${retryMissing.map((p) => `- ${p.kind}: ${p.name}`).join('\n')}` : '',
    '',
    'Finish with: files changed, Build.ps1 result, Test.ps1 result, other systems touched, open questions for Brandon (2-3 options each).',
  ].join('\n');
}

const branchName = (t) => `${t.owner.split(/\s*\/\s*/)[0].replace(/\s*\(.*\)/, '')}/${t.id.toLowerCase()}-${words(t.goal).slice(0, 3).join('-')}`;

function git(root, ...args) {
  const r = spawnSync('git', args, { cwd: root, encoding: 'utf8' });
  if (r.status !== 0) throw new Error(`git ${args.join(' ')}: ${r.stderr.trim()}`);
  return r.stdout.trim();
}

function writeCode(t, args, retryMissing) {
  const owner = t.owner.split(/\s*\/\s*/)[0].replace(/\s*\(.*\)/, '');
  const prompt = ticketPrompt(t, retryMissing);
  console.log(`\n>> code-writing: ${t.id} -> claude -p --agent ${owner} (budget $${args.budgetUsd})`);
  // The prompt goes in on stdin: it is multi-line markdown, which a shell command line would mangle.
  // GDD_AGENT_CLAUDE swaps the writer (e.g. a .js stub to test the loop without launching an agent).
  const cliArgs = ['-p', '--agent', owner, '--permission-mode', 'acceptEdits', '--output-format', 'json', '--max-budget-usd', String(args.budgetUsd)];
  const override = process.env.GDD_AGENT_CLAUDE;
  const [exe, exeArgs] = !override ? ['claude', cliArgs] : override.endsWith('.js') ? [process.execPath, [override, ...cliArgs]] : [override, cliArgs];
  const r = spawnSync(exe, exeArgs, { cwd: args.root, input: prompt, encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
  if (r.error) throw new Error(`could not start the writer (${exe}): ${r.error.message}`);
  let report = r.stdout;
  try { const j = JSON.parse(r.stdout); report = j.result; console.log(`   cost $${(j.total_cost_usd || 0).toFixed(2)}, ${j.num_turns} turns`); } catch { /* plain text */ }
  if (r.status !== 0) console.log('   agent exited with', r.status, (r.stderr || '').slice(0, 500));
  return report || '';
}

function verify(root) {
  const ps = (script) => spawnSync('powershell', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', path.join(root, 'Tools', script)],
    { cwd: root, encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
  for (const s of ['Build.ps1', 'Test.ps1']) {
    const r = ps(s);
    const first = (r.stdout || '').split('\n')[0].trim();
    console.log(`   ${s}: ${first}`);
    if (r.status !== 0) return { ok: false, why: `${s}: ${first}` };
  }
  return { ok: true };
}

// ------------------------------------------------------------------------------------------------
// 7. Iteration loop
// ------------------------------------------------------------------------------------------------

function analyse(args, assumedDone) {
  const gdd = readGdd(args.root);
  const scope = readScope(args.root);
  const tickets = buildRequirements(readRoadmap(args.root), gdd);
  const idx = scan(args.root);
  const gaps = detectGaps(tickets, idx, assumedDone);
  const { queue, milestone } = rank(tickets, scope, args.today);
  return { gdd, scope, tickets, idx, gaps, queue, milestone };
}

function printReport(a, args, header) {
  const { tickets, gaps, queue, milestone, idx } = a;
  const code = tickets.filter((t) => t.writable);
  const codeDone = code.filter((t) => t.status === 'done').length;
  console.log(`\n=== ${header} - ${iso(args.today)} ===`);
  console.log(`Scanned ${idx.files.length} files, ${idx.symbols.size} C++ types, ${idx.assets.size} assets.`);
  console.log(`Tickets: ${tickets.length} (${code.length} code). Code requirements met: ${codeDone}/${code.length}.`);
  console.log(`Current milestone: ${milestone ? `week ${milestone.week} - "${milestone.exit}" (open: ${[...milestone.tickets].join(', ')})` : 'all met'}`);
  console.log('\nGaps (code tickets not done):');
  for (const t of gaps.filter((g) => g.writable)) {
    const miss = t.missing.slice(0, 4).map((m) => m.name).join(', ') + (t.missing.length > 4 ? ` +${t.missing.length - 4}` : '');
    console.log(`  ${t.id.padEnd(7)} ${t.status.padEnd(12)} ${(Math.round(t.coverage * 100) + '%').padStart(4)}  ${t.goal.slice(0, 48).padEnd(48)}  ${t.status === 'unverifiable' ? '' : 'missing: ' + miss}`);
  }
  console.log(`\nPriority queue (top ${args.top}):`);
  console.log('  rank id      score  value crit mile  unbl  urg   mom  cost  goal');
  queue.slice(0, args.top).forEach((t, i) => {
    const x = t.priority.terms;
    console.log(`  ${String(i + 1).padStart(4)} ${t.id.padEnd(7)} ${String(t.priority.score).padStart(6)}  ${x.value.toFixed(1)}  ${x.critical.toFixed(1)}  ${x.milestone.toFixed(1)}  ${x.unblocks.toFixed(2)} ${x.urgency.toFixed(2)} ${x.momentum.toFixed(2)}  ${String(x.cost).padStart(3)}d  ${t.goal.slice(0, 50)}`);
  });
  const blocked = tickets.filter((t) => t.priority.gate && t.priority.gate.startsWith('blocked'));
  if (blocked.length) console.log(`\nBlocked: ${blocked.map((t) => `${t.id} (${t.priority.gate.slice(11)})`).join('; ')}`);
  const unc = uncoveredGdd(a.gdd, tickets);
  if (unc.length) console.log(`\nGDD sections with no ticket: ${unc.map((s) => s.path).join('; ')}`);
}

function explain(a, id) {
  const t = a.tickets.find((x) => x.id === id);
  if (!t) throw new Error(`No ticket ${id}`);
  console.log(`\n=== ${t.id} ${t.goal} ===`);
  console.log(`Owner ${t.owner}; week ${t.week}; planned ${t.start ? iso(t.start) : '?'} -> ${t.end ? iso(t.end) : '?'} (${t.days}d); deps ${t.deps.join(', ') || 'none'}; critical ${t.critical}`);
  console.log(`GDD: ${t.gdd.map((s) => s.path).join('; ') || 'none (tooling)'}`);
  console.log(`Status ${t.status}, coverage ${Math.round(t.coverage * 100)}%`);
  for (const r of t.results) console.log(`  [${r.found ? 'x' : ' '}] ${r.kind.padEnd(6)} ${r.name}${r.min ? ` (at least ${r.min})` : ''}${r.renamedFrom ? ` (ticket said ${r.renamedFrom})` : ''}${r.found ? '  <- ' + r.found : ''}`);
  const p = t.priority;
  console.log(p.gate ? `priority_score = 0 (${p.gate})` : `priority_score = ${p.score}  ${JSON.stringify(p.terms)}`);
  const pos = a.queue.indexOf(t);
  if (pos >= 0) console.log(`Queue position ${pos + 1} of ${a.queue.length}`);
}

function main() {
  const args = parseArgs(process.argv);
  let a = analyse(args);
  printReport(a, args, args.mode === 'run' ? 'Initial scan' : 'GDD agent plan');
  if (args.explain) explain(a, args.explain);
  if (args.json) fs.writeFileSync(args.json, JSON.stringify({ today: iso(args.today), milestone: a.milestone && { week: a.milestone.week, exit: a.milestone.exit },
    tickets: a.tickets.map((t) => ({ id: t.id, goal: t.goal, owner: t.owner, status: t.status, coverage: t.coverage, missing: t.missing.map((m) => m.name),
      gdd: t.gdd.map((s) => s.path), score: t.priority.score, gate: t.priority.gate, terms: t.priority.terms })) }, null, 2));
  if (args.mode === 'plan') return;

  // Iteration loop: pick -> write -> re-scan -> verify -> repeat, until every code requirement is met or nothing is buildable.
  const assumed = new Set();        // --simulate only: picks treated as built
  const failed = new Set();         // tickets that did not reach their requirements after all retries
  const order = [];
  for (let it = 1; it <= args.maxIterations; it++) {
    const next = a.queue.find((t) => !failed.has(t.id));
    if (!next) break;
    order.push(next.id);
    console.log(`\n--- iteration ${it}: ${next.id} (score ${next.priority.score}) ${next.goal}`);

    if (args.mode === 'simulate') {
      assumed.add(next.id);
      a = analyse(args, assumed);
      continue;
    }

    if (git(args.root, 'status', '--porcelain', '--untracked-files=no')) throw new Error('Working tree has uncommitted changes - commit or stash first');
    const base = git(args.root, 'rev-parse', '--abbrev-ref', 'HEAD');
    git(args.root, 'switch', '-c', branchName(next));

    let retryMissing = null;
    let ok = false;
    for (let attempt = 0; attempt <= args.retries && !ok; attempt++) {
      writeCode(next, args, retryMissing);
      a = analyse(args, assumed);                                   // re-scan after every write
      const t = a.tickets.find((x) => x.id === next.id);
      if (t.status !== 'done') { retryMissing = t.missing; console.log(`   re-scan: ${t.missing.length} requirement(s) still missing`); continue; }
      const v = args.verify ? verify(args.root) : { ok: true };
      if (!v.ok) { retryMissing = [{ kind: 'build/test', name: v.why }]; continue; }
      ok = true;
    }
    console.log(ok ? `   ${next.id} requirements met on ${branchName(next)}` : `   ${next.id} FAILED after ${args.retries + 1} attempts - left for Brandon`);
    if (!ok) failed.add(next.id);
    console.log(`   back to ${base} is Brandon's call (merge or review the branch); continuing from ${branchName(next)}`);
  }

  const code = a.tickets.filter((t) => t.writable);
  const open = code.filter((t) => t.status !== 'done');
  console.log(`\n=== Stopped. Build order: ${order.join(' -> ') || '(nothing to do)'}`);
  if (!open.length) console.log('All code requirements from the GDD are met.');
  else console.log(`Not met (${open.length}): ${open.map((t) => `${t.id} [${t.priority.gate || (failed.has(t.id) ? 'failed' : 'iteration cap')}]`).join(', ')}`);
}

if (require.main === module) {
  try { main(); } catch (e) { console.error('GDD AGENT FAIL:', e.message); process.exit(1); }
}

module.exports = { readGdd, readScope, readRoadmap, extractProbes, buildRequirements, scan, detectGaps, priority_score, rank, currentMilestone, ticketPrompt };
