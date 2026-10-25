# Orchestration

The main Claude Code session is the orchestrator. Subagents can't spawn other subagents, so routing happens here.

## Orchestrator's job
1. Break `Docs/Scope.md` into tickets (one agent, one outcome each)
2. Route each ticket to the agent that owns the area
3. Run independent tickets in parallel; each agent works in its own git worktree
4. Collect reports, verify the proof is real, and batch them for Brandon's daily review
5. Never decide creative questions - collect them and ask Brandon

## Agents
The roster is defined in `DeadwaterRun.md` → "AI Agents". Definitions live in `.claude/agents/`.

| Agent | Owns |
|---|---|
| `technical-architect` | High-level architecture: how systems connect, module and data layout, `Docs/Architecture/` (Uses Fable 5.1)|
| `gameplay-engineer` | Combat, turn manager, GAS attributes/abilities/effects/tags, map, shop, events, captains, economy, run state, data layout, Niagara VFX (Uses Opus 5.5) |
| `tools-engineer` | Editor tools, automation test harness, headless run simulator. (Uses Opus 5.5) | 
| `ai-engineer` | Enemy crew decision making, Kraken behavior (Uses Opus 5.5) |
| `ui-engineer` | UMG widgets and screens (Uses Opus 5.5) |
| `ui-juice-engineer` | UI animations and UI sound triggers (Uses Opus 5.5) |
| `balance-designer` | Tuning values in data, balance reports (Uses Opus 5.5) |
| `qa-tester` | Breaking the game, bug reports, verifying deliveries (Uses Sonnet 5.5)|
| `sound-wrangler` | Sound .uassets, mixes, combat sound cues (Uses Sonnet 5.5) |
| `release-engineer` | Packaging, Steamworks, store page and release checklists (Uses Sonnet 5.5) |
| `asset-scraper` | Third-party asset options (read-only, never imports) (Uses Sonnet 5.5)|

## Working together
- Deadwater Run is one game, not a set of features. No system is built in a silo: every system uses the shared tags, attributes, data assets and run state, and every report lists the other systems a change touches
- The Technical Architect sets the high-level architecture. For work that adds or changes how systems connect, route a `technical-architect` ticket first; other agents build from its document in `Docs/Architecture/`
- If an agent finds the architecture missing or wrong for its task, it stops and reports. The orchestrator routes the question to the Technical Architect

## Ticket format
```
Agent:       <agent name>
Goal:        <one outcome>
Context:     <design doc section / prior PRs>
Acceptance:  <testable criteria>
Proof:       <tests, screenshot, clip>
Out of scope: <what not to touch>
```

## Delivery rules
- Every delivery is a local branch, never a commit to `master`. Agents and the orchestrator never push or open PRs - Brandon pushes and opens the PR
- Every PR includes: build result, test output, and a screenshot or clip from the game
- Creative questions go in the report as 2-3 options; agents never pick for Brandon
- AI-generated art needs Brandon's explicit approval. AI code, VFX and UI are fine
- Anything outside `Docs/Scope.md` goes on the post-launch list

## Tracking agent changes
- Branches are named `<agent>/<short-topic>` (e.g. `gameplay-engineer/turn-manager`)
- Every commit message ends with an `Agent: <agent name>` line, above the co-author line
- Every PR gets the label `agent:<agent name>`
- A hook (`.claude/settings.json`) logs every Edit/Write/Unreal MCP call to `.claude/logs/agent-log.jsonl` in the main repo, including from worktrees. It is local and gitignored, and costs no tokens
- Daily review:
  - `node .claude/hooks/agent-log-view.js` - today's changes, grouped by agent
  - `node .claude/hooks/agent-log-view.js --since 2026-10-20 --agent gameplay-engineer` - one agent over a range
  - `git log --all --grep "Agent: gameplay-engineer"` - commits by one agent
  - `gh pr list --label agent:gameplay-engineer` - PRs by one agent
