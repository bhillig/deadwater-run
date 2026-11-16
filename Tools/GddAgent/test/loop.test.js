// End-to-end test of gdd-agent.js on a throwaway fixture repo with a stub writer (no agent is launched, no API cost).
//   node Tools/GddAgent/test/loop.test.js      -> prints "LOOP TEST PASS" or "LOOP TEST FAIL ...", exit code 0 on pass
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const assert = require('assert');
const { spawnSync, execSync } = require('child_process');

const agent = path.join(__dirname, '..', 'gdd-agent.js');
const { priority_score, extractProbes } = require(agent);
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'gdd-agent-'));
const put = (rel, text) => { fs.mkdirSync(path.join(dir, path.dirname(rel)), { recursive: true }); fs.writeFileSync(path.join(dir, rel), text); };

try {
  // --- unit checks: probe extraction and the score gates ---
  const probes = extractProbes('UDwRun with phase guards, DA_MapRules, Tools/Simulate.ps1, DeadwaterRun.Sim.Smoke tests, UDwX (after Brandon confirms UDwY)');
  assert.deepStrictEqual(probes.map((p) => p.kind + ':' + p.name),
    ['symbol:UDwRun', 'symbol:UDwX', 'asset:DA_MapRules', 'file:Tools/Simulate.ps1', 'test:DeadwaterRun.Sim.Smoke']);
  const ctx = { today: new Date('2026-11-10T12:00:00Z'), status: new Map([['DW-1', 'missing']]), dependents: new Map([['DW-2', new Set()]]), milestone: null };
  const t = { id: 'DW-2', status: 'missing', writable: true, deps: ['DW-1'], gdd: [], critical: false, days: 1, coverage: 0, end: null };
  assert.strictEqual(priority_score(t, ctx).score, 0, 'blocked ticket must score 0');
  ctx.status.set('DW-1', 'done');
  assert.ok(priority_score(t, ctx).score > 0, 'unblocked ticket must score > 0');
  assert.strictEqual(priority_score({ ...t, writable: false }, ctx).score, 0, 'non-code owner must score 0');

  // --- fixture repo: GDD, scope, roadmap with two tickets (DW-902 after DW-901) ---
  put('DeadwaterRun.md', '# Fixture\n\n## Game Mechanics\n\n### At The Final Boss Node\n- Players must fight a Kraken to beat the game\n- Kraken has 500 HP\n\n## AI Architecture\n');
  put('Docs/Scope.md', '# Scope\n\n## Must ship (v1.0)\n- **Run loop:** Kraken boss\n\n## Timeline\n| Week | Dates | Exit criteria |\n|---|---|---|\n| 1 | Nov 1-7 | Kraken boss fight playable |\n');
  put('Docs/Architecture/Roadmap.md', [
    '# Roadmap', '', '```mermaid', 'gantt', '    section Week 1',
    '    DW-901 Kraken boss fight                :crit, w901, 2026-11-01, 1d',
    '    DW-902 Kraken boss HUD                  :w902, after w901, 1d', '```', '',
    '```', 'Agent:       gameplay-engineer', 'Goal:        DW-901 Kraken boss fight', 'Context:     fixture',
    'Acceptance:  UDwKrakenBoss, DA_Kraken_Test', 'Proof:       test', 'Out of scope: HUD', '```', '',
    '```', 'Agent:       ui-engineer', 'Goal:        DW-902 Kraken boss HUD', 'Context:     fixture',
    'Acceptance:  UDwBossHud', 'Proof:       screenshot', 'Out of scope: logic', '```', ''].join('\n'));
  put('Source/DeadwaterRun/DeadwaterRun.h', '#pragma once\n');
  execSync('git init -q && git add -A && git -c user.name=t -c user.email=t@t commit -q -m fixture', { cwd: dir, stdio: 'ignore' });
  execSync('git config user.name t && git config user.email t@t', { cwd: dir });

  const run = spawnSync(process.execPath, [agent, '--root', dir, '--today', '2026-11-10', '--run', '--retries', '1'],
    { encoding: 'utf8', env: { ...process.env, GDD_AGENT_CLAUDE: path.join(__dirname, 'stub-writer.js') } });
  const out = run.stdout + run.stderr;
  assert.strictEqual(run.status, 0, out);
  assert.match(out, /iteration 1: DW-901/, 'highest-priority unblocked ticket first');
  assert.match(out, /Priority queue[\s\S]*DW-901[\s\S]*Blocked: DW-902 \(DW-901\)/, 'DW-902 starts blocked');
  assert.match(out, /re-scan: 1 requirement\(s\) still missing/, 're-scan after the incomplete write');
  assert.match(out, /DW-901 requirements met/, 'retry completes DW-901');
  assert.match(out, /iteration 2: DW-902/, 'DW-902 unblocked by the re-scan');
  assert.match(out, /Build order: DW-901 -> DW-902/);
  assert.match(out, /All code requirements from the GDD are met\./, 'stops when everything is met');
  const branches = execSync('git branch', { cwd: dir, encoding: 'utf8' });
  assert.match(branches, /gameplay-engineer\/dw-901-kraken-boss-fight/);
  assert.match(branches, /ui-engineer\/dw-902-kraken-boss\b/);
  console.log('LOOP TEST PASS');
  if (process.argv.includes('-v')) console.log(out);
} catch (e) {
  console.log('LOOP TEST FAIL ' + e.message);
  process.exitCode = 1;
} finally {
  fs.rmSync(dir, { recursive: true, force: true });
}
