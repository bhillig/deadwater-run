// Stand-in for `claude -p --agent <name> ...` used by pipeline.test.js - no agent is launched.
// event-writer: the first draft has planted mistakes; the revision (prompt has "## Revision") fixes them.
// content-critic: asks for a revision while the code checks still report blocker/major findings, then passes.
'use strict';
const fs = require('fs');

const argv = process.argv.slice(2);
const agent = argv[argv.indexOf('--agent') + 1];
const prompt = fs.readFileSync(0, 'utf8');
if (process.env.STUB_LOG) fs.appendFileSync(process.env.STUB_LOG, JSON.stringify({ agent, argv, prompt }) + '\n');

const ev = (id, valence, choices, extra = {}) => ({
  id, title: id.replace('DA_Event_', ''), body: 'Fog sits on the water. Something knocks against your hull.', valence, weight: 1, minLayer: 1, maxLayer: 3, choices, ...extra,
});
const ch = (label, outcomes, resultText = 'The crew gets back to work.', requirements = []) => ({ label, requirements, outcomes, resultText });

function badDraft() {
  return {
    notes: 'first draft',
    events: [
      ev('DA_Event_RumBarrels', 'Good', [
        ch('Haul them aboard, matey', [{ type: 'GiveItem', item: 'Rum', count: 1 }]),                   // unknown item + banned word
        ch('Drink to the captain', [{ type: 'CrewMana', delta: 10, selector: 'All' }]),                   // removed outcome
      ]),
      ev('DA_Event_TollBoat', 'Bad', [
        ch('Pay 40 gold', [{ type: 'Gold', delta: -30 }], 'They take it and row off.', [{ type: 'MinGold', amount: 30 }]), // text vs data
        ch('Hand over a crewmate', [{ type: 'LoseCrew', selector: 'Captain' }], 'Gone.', [{ type: 'CrewCountAtLeast', count: 2 }]), // captain + stuck
      ]),
      ev('DA_Event_DriftingChest', 'Good', [ch('Open it', [{ type: 'Gold', delta: 500 }], 'Gold, and plenty of it.')]), // guardrail
      ev('DA_Event_Gull', 'Good', [ch('Feed it', [{ type: 'GrantXp', amount: 20, selector: 'All' }], 'It follows you for a day.')]),
    ],
  };
}

function goodDraft() {
  return {
    notes: 'Fixed: Rum -> HealthRestore; CrewMana -> CrewHealth; toll text matches -40; LoseCrew Random + a free choice; chest 120.',
    events: [
      ev('DA_Event_RumBarrels', 'Good', [
        ch('Haul them aboard', [{ type: 'GiveItem', item: 'HealthRestore', count: 1 }]),
        ch('Drink to the captain', [{ type: 'CrewHealth', delta: 10, selector: 'All' }]),
      ]),
      ev('DA_Event_TollBoat', 'Bad', [
        ch('Pay 40 gold', [{ type: 'Gold', delta: -40 }], 'They take it and row off.', [{ type: 'MinGold', amount: 40 }]),
        ch('Run the blockade', [{ type: 'ShipHealth', delta: -30 }], 'Shot rakes the hull as you pass.'),
      ]),
      ev('DA_Event_DriftingChest', 'Good', [ch('Open it', [{ type: 'Gold', delta: 120 }], 'Gold, and plenty of it.')]),
      ev('DA_Event_Gull', 'Good', [ch('Feed it', [{ type: 'GrantXp', amount: 20, selector: 'All' }], 'It follows you for a day.')]),
    ],
  };
}

function review() {
  const m = /## Code-check results[^\n]*\n\n```json\n([\s\S]*?)\n```/.exec(prompt);
  const code = m ? JSON.parse(m[1]) : [];
  const open = code.some((f) => f.severity !== 'minor');
  return {
    verdict: open ? 'revise' : 'pass',
    voice: { score: open ? 2 : 4, sounds_like_deadwater_run: !open, why: open ? 'Stage-pirate wording.' : 'Plain and dry.', weakest_lines: open ? ['Haul them aboard, matey'] : [] },
    findings: open ? [{ severity: 'major', event: 'DA_Event_RumBarrels', field: 'choices[1].label', rule: 'design', problem: 'Toasting with no cost is a free choice', fix: 'Give it a cost' }] : [],
    code_check_disputes: [],
    open_questions: ['Should events ever cost crew?'],
  };
}

const isWriter = agent === 'event-writer';
let out = isWriter ? (prompt.includes('## Revision') ? goodDraft() : badDraft()) : review();
// STUB_BAD_SHAPE_ONCE=<flag file>: the first critic reply drops required keys (seen from a real run on 2026-11-17).
const flag = process.env.STUB_BAD_SHAPE_ONCE;
if (!isWriter && flag && !fs.existsSync(flag)) { fs.writeFileSync(flag, '1'); out = { verdict: out.verdict, findings: out.findings }; }
process.stdout.write(JSON.stringify({ type: 'result', is_error: false, result: JSON.stringify(out), structured_output: out, total_cost_usd: 0.01, num_turns: 1 }));
