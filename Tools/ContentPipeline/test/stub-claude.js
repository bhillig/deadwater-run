// Stand-in for `claude -p --agent <name> ...` used by pipeline.test.js - no agent is launched, no API cost.
//
// event-writer (Generator), content-refiner (Refiner): return a fixed draft per STUB_SCENARIO
//   default  generator: planted mistakes + generic text      refiner: a clean, Deadwater-specific batch
//   lock     generator: 3 strong events + 1 generic one       refiner: fixes the generic one AND edits a locked one
//   regress  generator: same as lock                          refiner: makes it worse every time
// content-critic (Evaluator): scores from what the pipeline put in its prompt (code findings + specificity signals),
//   so the score really moves with the content.
'use strict';
const fs = require('fs');

const argv = process.argv.slice(2);
const agent = argv[argv.indexOf('--agent') + 1];
const prompt = fs.readFileSync(0, 'utf8');
if (process.env.STUB_LOG) fs.appendFileSync(process.env.STUB_LOG, JSON.stringify({ agent, argv, prompt }) + '\n');
const scenario = process.env.STUB_SCENARIO || 'default';

const ev = (id, valence, body, choices, extra = {}) => ({ id, title: id.replace('DA_Event_', ''), body, valence, weight: 1, minLayer: 1, maxLayer: 3, choices, ...extra });
const ch = (label, outcomes, resultText = 'The crew gets back to work.', requirements = []) => ({ label, requirements, outcomes, resultText });

// ---- Deadwater-specific, valid events
const SCARRED = ev('DA_Event_ScarredSloop', 'Good', 'A rival crew\'s sloop drifts past with a tentacle scar across her hull. Nobody is aboard.', [
  ch('Strip her powder', [{ type: 'Gold', delta: 60 }], 'Dry powder and a purse. Whoever sailed her met the Kraken first.'),
  ch('Take her planks', [{ type: 'ShipHealth', delta: 40 }], 'Your hull is the better for it.'),
]);
const HIRE = ev('DA_Event_SwordForHire', 'Good', 'A lone Swordsman waves from a spar. His crew went down for good last night.', [
  ch('Haul him aboard', [{ type: 'Recruit', unit: 'Swordsman' }], 'He takes the empty bunk without a word.', [{ type: 'CrewCountAtMost', count: 3 }]),
  ch('Take his sword', [{ type: 'Gold', delta: 40 }], 'He keeps the spar. You keep the sword.'),
]);
const HULLORCREW = ev('DA_Event_HullOrCrew', 'Good', 'A rival crew\'s cache sits on a shoal. Plunder it fast under their guns, or sink their lookout boat first.', [
  ch('Run in under fire', [{ type: 'ShipHealth', delta: -30 }, { type: 'Gold', delta: 100 }], 'Shot rakes the hull. The chest is worth it.'),
  ch('Send the crew ashore', [{ type: 'CrewHealth', delta: -20, selector: 'Random' }, { type: 'Gold', delta: 100 }], 'They come back bloodied and rich.'),
]);
const REEF = ev('DA_Event_AquadiaReef', 'Bad', 'The reefs of Aquadia rise under the keel at low tide.', [
  ch('Scrape over', [{ type: 'ShipHealth', delta: -40 }], 'The hull groans. You make it.'),
  ch('Everyone on the ropes', [{ type: 'CrewHealth', delta: -20, selector: 'All' }], 'Hands bleed on the lines.'),
]);
// ---- Valid but generic (no signature term, no Deadwater mechanic)
const BARREL = ev('DA_Event_FloatingBarrel', 'Good', 'A barrel bobs in the swell. It is sealed tight.', [
  ch('Fish it out', [{ type: 'Gold', delta: 30 }], 'Coins, packed in straw.'),
  ch('Sail on', [{ type: 'Nothing' }], 'Nothing happens.'),
]);
// ---- Planted mistakes
const BAD = [
  ev('DA_Event_RumBarrels', 'Good', 'Fog sits on the water. Something knocks against your hull.', [
    ch('Haul them aboard, matey', [{ type: 'GiveItem', item: 'Rum', count: 1 }]),                   // unknown item + banned word
    ch('Drink to the captain', [{ type: 'CrewMana', delta: 10, selector: 'All' }]),                   // removed outcome
  ]),
  ev('DA_Event_TollBoat', 'Bad', 'Fog sits on the water. Something knocks against your hull.', [
    ch('Pay 40 gold', [{ type: 'Gold', delta: -30 }], 'They take it and row off.', [{ type: 'MinGold', amount: 30 }]), // text vs data
    ch('Hand over a crewmate', [{ type: 'LoseCrew', selector: 'Captain' }], 'Gone.', [{ type: 'CrewCountAtLeast', count: 2 }]), // captain + stuck
  ]),
  ev('DA_Event_DriftingChest', 'Good', 'Fog sits on the water. Something knocks against your hull.', [ch('Open it', [{ type: 'Gold', delta: 500 }], 'Gold, and plenty of it.')]), // guardrail
  ev('DA_Event_Gull', 'Good', 'Fog sits on the water. Something knocks against your hull.', [ch('Feed it', [{ type: 'GrantXp', amount: 20, selector: 'All' }], 'It follows you for a day.')]),
];

const draft = (events, notes) => ({ events: JSON.parse(JSON.stringify(events)), notes });

function generator() {
  if (scenario === 'default') return draft(BAD, 'first draft');
  return draft([SCARRED, HIRE, REEF, BARREL], 'first draft');
}

function refiner() {
  if (scenario === 'default') return { ...draft([SCARRED, HIRE, HULLORCREW, REEF], 'Rebuilt on the two health bars.'), changes: [{ event: 'DA_Event_RumBarrels', field: 'events', finding: 'E05 outcome', after: 'replaced' }] };
  if (scenario === 'lock') {
    const edited = { ...SCARRED, title: 'Retitled By The Refiner' };
    return { ...draft([edited, HIRE, REEF, HULLORCREW], 'Replaced the generic barrel; also retitled a locked event.'), changes: [{ event: 'DA_Event_FloatingBarrel', field: 'events', finding: 'specificity', after: 'DA_Event_HullOrCrew' }] };
  }
  return { ...draft(BAD, 'worse'), changes: [] }; // regress
}

const block = (title) => {
  const m = new RegExp(`## ${title}[^\\n]*\\n\\n\`\`\`json\\n([\\s\\S]*?)\\n\`\`\``).exec(prompt);
  return m ? JSON.parse(m[1]) : null;
};

function evaluator() {
  const events = block('Content') || [];
  const code = block('Code-check results') || [];
  const signals = block('Specificity signals') || {};
  const specific = (id) => { const s = signals[id] || { terms: [], mechanics: [] }; return s.terms.length + s.mechanics.length > 0; };
  const f = events.length ? events.filter((e) => specific(e.id)).length / events.length : 0;
  const blockers = code.filter((x) => x.severity === 'blocker').length;
  const banned = code.some((x) => x.rule === 'E10 voice');
  const dim = (score, reason) => ({ score: Math.max(0, Math.min(100, Math.round(score))), reason });
  return {
    dimensions: {
      rules: dim(100 - 25 * blockers, `${blockers} code blocker(s)`),
      text_data: dim(60 + 30 * f, 'stub'),
      design: dim(40 + 50 * f, 'stub'),
      voice: dim(40 + 50 * f - (banned ? 20 : 0), banned ? 'V04 stage-pirate word' : 'stub'),
      specificity: dim(90 * f, `${Math.round(f * 100)}% of events are built on Deadwater Run`),
    },
    events: events.map((e) => ({ id: e.id, score: specific(e.id) ? 92 : 40, reason: specific(e.id) ? 'Deadwater Run' : 'generic', stranger_test: specific(e.id) ? 'two health bars' : 'nothing - works in any pirate game' })),
    findings: events.filter((e) => !specific(e.id)).map((e) => ({ severity: 'major', event: e.id, field: 'body', rule: 'specificity', problem: 'Works unchanged in any pirate game', fix: 'Build it on the ship vs crew health bars' })),
    code_check_disputes: [],
    summary: f === 1 ? 'Reads as Deadwater Run.' : 'Generic events drag the batch down.',
  };
}

let out = agent === 'event-writer' ? generator() : agent === 'content-refiner' ? refiner() : evaluator();
// STUB_BAD_SHAPE_ONCE=<flag file>: the first evaluator reply drops required keys (seen from a real run on 2026-11-17).
const flag = process.env.STUB_BAD_SHAPE_ONCE;
if (agent === 'content-critic' && flag && !fs.existsSync(flag)) { fs.writeFileSync(flag, '1'); out = { findings: out.findings }; }
process.stdout.write(JSON.stringify({ type: 'result', is_error: false, result: JSON.stringify(out), structured_output: out, total_cost_usd: 0.01, num_turns: 1 }));
