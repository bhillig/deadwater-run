/*
 * The Deadwater Run style guide, compiled from the repo on every run - never hand-copied.
 *
 * Every rule quotes the line it comes from (file:line, checked verbatim), so a change to the GDD, the voice sheet,
 * the store page, the UI art direction or the shipped event pool reaches the generator, evaluator and refiner on
 * the next run. If a quoted line moves or changes, the build stops instead of enforcing a stale rule.
 *
 * The stranger test: every rule must name something only Deadwater Run has (a term in the signature lexicon,
 * itself pulled from the sources). A rule a stranger could read as "any pirate game" fails the build.
 */
'use strict';

const fs = require('fs');
const path = require('path');

const SOURCES = {
  gdd: 'DeadwaterRun.md',
  voice: 'Docs/ContentPipeline/voice.md',
  store: 'Docs/Release/StorePage.md',
  ui: 'Docs/Architecture/14-UI.md',
  events: 'Docs/Architecture/11-Events.md',
  pool: 'Tuning/Events.json',
};

/** Pirate-genre words a stranger would expect from any pirate game. They never count toward the stranger test. */
const GENERIC = ['pirate', 'pirates', 'ship', 'ships', 'sea', 'seas', 'gold', 'hull', 'crew', 'captain', 'treasure', 'rum',
  'cannon', 'cannons', 'deck', 'sword', 'plank', 'parrot', 'island', 'storm', 'map', 'sail', 'sails', 'port'];

function load(root) {
  const files = {};
  for (const [k, rel] of Object.entries(SOURCES)) {
    const p = path.join(root, rel);
    if (!fs.existsSync(p)) throw new Error(`style: missing ${rel}`);
    files[k] = fs.readFileSync(p, 'utf8').replace(/\r\n/g, '\n');
  }
  return files;
}

/** Finds `re` in a source and returns a citation: the verbatim match and its 1-based line. */
function citer(files) {
  return (key, re) => {
    const lines = files[key].split('\n');
    for (let i = 0; i < lines.length; i++) {
      const m = re.exec(lines[i]);
      if (m) return { file: SOURCES[key], line: i + 1, quote: m[0].trim() };
    }
    throw new Error(`style: ${SOURCES[key]} no longer says ${re} - the source changed; update lib/style.js`);
  };
}

/** The three captains from the GDD's "Captain Choices": name, identity line, abilities, run rule. */
function parseCaptains(gdd, cite, tuningNames) {
  const block = (/### Captain Choices\n([\s\S]*?)\n### Your Ship/.exec(gdd) || [])[1];
  if (!block) throw new Error('style: DeadwaterRun.md "Captain Choices" section not found');
  const parts = block.split(/^#### /m).slice(1);
  if (parts.length !== 3) throw new Error(`style: expected 3 captains in the GDD, found ${parts.length}`);
  return parts.map((p) => {
    const display = p.split('\n')[0].trim().replace(/^The /, '');
    const key = tuningNames.find((t) => t.toLowerCase() === display.replace(/\s+/g, '').toLowerCase());
    if (!key) throw new Error(`style: GDD captain "${display}" has no CaptainTuning row`);
    // "- What's different:" / "- Combat Abilities:" / "- Run rule" headers, each with nested "    - ..." bullets.
    const sections = {};
    let current = null;
    for (const line of p.split('\n')) {
      const head = /^\s{0,2}- ([^:\n]+?):?\s*$/.exec(line);
      const item = /^\s{3,}- (.+)$/.exec(line);
      if (item && current) sections[current].push(item[1].trim());
      else if (head) sections[current = head[1].trim().toLowerCase()] = [];
    }
    const need = (k) => {
      if (!(sections[k] || []).length) throw new Error(`style: GDD captain "${display}" has no "${k}" bullets`);
      return sections[k];
    };
    const named = (b) => { const m = /^([A-Z][A-Za-z' ]+?):[ \t]*(.+)$/.exec(b); return m ? { name: m[1].trim(), what: m[2].trim() } : { name: null, what: b }; };
    const identity = need("what's different")[0];
    return {
      display, key, identity, traits: need("what's different"),
      abilities: need('combat abilities').map(named),
      runRule: named(need('run rule')[0]),
      cite: cite('gdd', new RegExp(identity.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'))),
    };
  });
}

function signatureLexicon(files, captains) {
  const gdd = files.gdd;
  const kraken = [...((/### At The Final Boss Node\n([\s\S]*?)(?:\n## |\n### |$)/.exec(gdd) || [])[1] || '').matchAll(/^\s+- ([A-Z][A-Za-z ]+?) \(/gm)].map((m) => m[1]);
  if (!kraken.length) throw new Error('style: Kraken moves not found in the GDD');
  const fixed = [
    ['gdd', 'Aquadia'], ['gdd', 'Kraken'], ['gdd', 'Fear'], ['gdd', 'plunder'], ['gdd', 'sink'], ['gdd', 'Swordsman'], ['gdd', 'Sniper'],
    ['gdd', "sniper's nest"], ['gdd', 'gone for good'], ['gdd', '250 HP'], ['gdd', 'rival crew'], ['gdd', 'tentacle'],
    ['ui', "Ship's Log"], ['ui', 'parchment'], ['ui', 'Grenze'], ['ui', 'Texturina'], ['ui', 'Black Lantern'], ['ui', 'wax'],
    ['events', 'Describe'], ['events', 'CaptainIs'], ['events', 'LoseCrew'], ['events', 'RandomOf'],
  ];
  for (const [k, t] of fixed) if (!files[k].toLowerCase().includes(t.toLowerCase())) throw new Error(`style: lexicon term "${t}" is no longer in ${SOURCES[k]}`);
  const terms = [
    ...fixed.map(([, t]) => t), 'Deadwater Run', ...kraken,
    // Ability and run-rule names, when they're more than one everyday word ("Charge" alone would match any text).
    ...captains.flatMap((c) => [c.display, c.key, ...[...c.abilities, c.runRule].map((a) => a.name).filter((x) => x && /\s/.test(x))]),
  ];
  return [...new Set(terms)].filter((t) => !GENERIC.includes(t.toLowerCase()));
}

/** Mechanic names that are also everyday words: they count only as written in the GDD ("Fear", not "fear"). */
const CASE_SENSITIVE = new Set(['Fear']);
const hasTerm = (text, t) => new RegExp(`(^|[^A-Za-z])${t.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}`, CASE_SENSITIVE.has(t) ? '' : 'i').test(text);
const termsIn = (text, lexicon) => lexicon.filter((t) => hasTerm(text, t));

function buildStyleGuide(root, facts) {
  const files = load(root);
  const cite = citer(files);
  const pool = JSON.parse(files.pool);
  const shipped = Array.isArray(pool) ? pool : pool.events;
  if (!Array.isArray(shipped) || !shipped.length) throw new Error('style: Tuning/Events.json has no events');
  const captains = parseCaptains(files.gdd, cite, facts.names.captains);
  const lexicon = signatureLexicon(files, captains);
  const by = (k) => captains.find((c) => c.key === k) || captains[0];
  const n = facts.numbers;

  const rules = [
    // ---- Setting and story
    { id: 'S01', area: 'setting', cites: [cite('gdd', /legendary seas of Aquadia/), cite('voice', /The legendary seas of Aquadia\./)],
      rule: 'Every event happens on the seas of Aquadia, somewhere on the chart between port and the Kraken. Places belong to Aquadia (reefs, shoals, wrecks, rival crews); no real-world ports, nations, navies or history.' },
    { id: 'S02', area: 'setting', cites: [cite('gdd', /sink your enemy.s ship allowing you to progress through the fight quicker or slay all enemies for the opportunity to plunder\?/), cite('gdd', /grants you 1\.5x the loot/), cite('store', /Quick and safe, or slow and rich - your call, every fight\./)],
      rule: 'Each event restages the game\'s hook in miniature: a quick, safe option against a slow, rich one, the same call as sinking the ship (1x loot) or killing the crew to plunder (1.5x). If no choice carries that sink-or-plunder tension, the event is not Deadwater Run.' },
    { id: 'S03', area: 'setting', cites: [cite('gdd', /Ship starts with 250 HP and two cannons/), cite('gdd', /Start with a pirate crew of three \(100 HP\/20 MP\)/), cite('gdd', /Each pirate ship has a health bar too\./)],
      rule: `Costs land on Deadwater Run's two health bars: the ship (${n.shipMaxHealth} HP, two ${n.cannonDamage}-damage cannons, ShipHealth) and the crew (${n.crewMaxHealth} HP each, CrewHealth). The strongest events make you spend one bar to save the other: the hull or the crew.` },
    { id: 'S04', area: 'setting', cites: [cite('gdd', /Once a crew member dies.+they are gone for good!/), cite('store', /Permadeath crew: a fallen pirate is gone for good/), cite('gdd', /Maximum crew of 4 members \(including the captain\)/)],
      rule: `Crew lives are permanent. A pirate lost to LoseCrew is gone for good, from a crew of ${n.maxCrew} at most. That loss is never a throwaway line: the result text names who goes (the Swordsman, the Sniper) and lets it land.` },
    { id: 'S05', area: 'setting', cites: [cite('store', /You have a ship, a captain and two crewmates\. That will have to do\./), cite('gdd', /Swordsman \(Close range\)/), cite('gdd', /Sniper \(Long range\)/)],
      rule: 'You are a small, tough crew: a captain, a Swordsman (close range) and a Sniper (long range), up to four. Not a fleet, not a navy, no first mate, quartermaster or cabin boy the game doesn\'t have. Crowds of your own sailors don\'t exist.' },
    { id: 'S06', area: 'setting', cites: [cite('gdd', /Players must fight a Kraken to beat the game/), cite('store', /the Kraken has never been beaten/), cite('gdd', /Tentacle slam/)],
      rule: 'The Kraken waits at the end of the chart and has never been beaten. Events can foreshadow it (tentacle scars, hulls it broke, crews that sailed on before you) but never fight it, wound it or hand you its weakness. It is the only monster in Aquadia.' },
    { id: 'S07', area: 'setting', cites: [cite('gdd', /Random event occurs with 60% good \/ 40% bad odds/), cite('voice', /The sea is mostly kind \(60% good events\) but never safe/)],
      rule: `Aquadia's sea is mostly kind but never safe: ${Math.round(facts.event.goodEventChance * 100)}% of events are Good, the rest Bad. Good events can still cost something; Bad events always offer a choice that limits the damage.` },
    { id: 'S08', area: 'setting', cites: [cite('gdd', /Obtain gold and new abilities upon successfully defeating a rival crew/), cite('voice', /Magic systems, gods, curses with rules, anything the game can.t show or doesn.t have/)],
      rule: 'The other people on the water are rival pirate crews making their own run at the Kraken: what they left, what they want, what they fear. No magic, gods, ghost ships or curses with rules, because nothing in the game can show them.' },
    // ---- The captains (generated from the GDD)
    ...captains.map((c, i) => ({
      id: `C0${i + 1}`, area: 'captain', cites: [c.cite],
      rule: `A CaptainIs ${c.key} choice plays to the ${c.display}: ${c.traits.map((t) => `"${t.replace(/[,.]$/, '')}"`).join('; ')}. `
        + `Draw on what the GDD gives them: ${c.abilities.map((a) => (a.name ? `${a.name} (${a.what.replace(/\.$/, '')})` : a.what)).join('; ')}; `
        + `run rule ${c.runRule.name ? `${c.runRule.name} (${c.runRule.what.replace(/\.$/, '')})` : `"${c.runRule.what}"`}. Text gated to another captain never borrows this one's traits.`,
    })),
    // ---- Voice
    { id: 'V01', area: 'voice', cites: [cite('voice', /\*\*Second person, present tense\.\*\* "A longboat bumps against your hull\."/)],
      rule: `Write as "you", the captain, in present tense ("A longboat bumps against your hull."). Untagged text never assumes which captain you are: it must read true for the ${captains.map((c) => c.display).join(', ')} alike.` },
    { id: 'V02', area: 'voice', cites: [cite('voice', /\*\*Plain and punchy\.\*\* Short sentences\. Concrete nouns: hull, powder, rope, rum, gold, deck, rigging, fog/), cite('gdd', /The sniper's nest of a ship is available/)],
      rule: 'Short sentences built from what is on a Deadwater Run deck: hull, powder, rope, rum, gold, rigging, fog, the cannons, the sniper\'s nest. No abstractions where a thing would do.' },
    { id: 'V03', area: 'voice', cites: [cite('voice', /The tone is a small, tough crew against a cruel sea\. Dangerous and tense, with dry gallows humour\. It isn.t a cartoon and it isn.t grimdark/), cite('store', /That will have to do\./)],
      rule: 'Dry gallows humour, the store page\'s "That will have to do." Understatement over punchlines. Aquadia is dangerous and tense: not a cartoon, not grimdark.' },
    { id: 'V04', area: 'voice', cites: [cite('voice', /\*\*Light pirate flavour\.\*\* One salty word per line at most\. No "arr", "matey", "ye" or "shiver me timbers"/)],
      rule: `One salty word per line at most. Deadwater Run never says ${facts.bannedWords.map((w) => `"${w}"`).join(', ')}: those are stage pirates, and the code checks ban them.` },
    { id: 'V05', area: 'voice', cites: [cite('voice', /\*\*Choices read as decisions\*\*, not as menu items\. "Cut the line" rather than "Option A: lose 20 gold"/), cite('events', /shows title, body and one button per choice with its outcomes' "you will" lines/)],
      rule: 'Labels are short imperatives ("Cut the line"). The event screen already prints each outcome\'s Describe() "you will" line under its button, so a label never restates the numbers and never reads like "Option A: lose 20 gold".' },
    { id: 'V06', area: 'voice', cites: [cite('voice', /Long lore dumps\. A body is 2-4 sentences/)],
      rule: 'A body is 2-4 sentences, a result 1-2. Aquadia is shown through one scene, never explained: no history of the seas, no backstory of the Kraken.' },
    { id: 'V07', area: 'voice', cites: [cite('voice', /Promising effects the outcomes don.t apply/), cite('events', /`TArray<TObjectPtr<UDwEventOutcome>> Outcomes` \(applied in order\)/)],
      rule: 'Every promise in a body, label or result maps to an outcome in that choice: "two medicine kits" only with GiveItem HealthRestore x2. A RandomOf choice shows one ResultText for every branch, so it must read true whichever branch lands; never describe both.' },
    // ---- Aesthetic (UI art direction, U6)
    { id: 'A01', area: 'aesthetic', cites: [cite('ui', /\*\*"Ship's Log"\*\* everywhere - parchment pages and cards/), cite('ui', /fonts Grenze \(headings, `F_DwHeading`\) and Texturina \(body/)],
      rule: 'Every screen is the Ship\'s Log: an event is ink on a parchment page, its title set in Grenze and its body in Texturina. Write what a captain would ink into the log of the run. No UI talk ("click", "button", "menu", "screen"), no emoji, no markdown.' },
    { id: 'A02', area: 'aesthetic', cites: [cite('ui', /"HULL IS SOUND", "SOLD OUT"/), cite('ui', /WE ARE HERE over the current one/)],
      rule: 'ALL CAPS belongs to the Ship\'s Log ink stamps ("HULL IS SOUND", "SOLD OUT", "WE ARE HERE"). Event text is sentence case and titles are short headings ("The Tally Rock"). No caps-lock shouting, no exclamation marks.' },
    { id: 'A03', area: 'aesthetic', cites: [cite('ui', /sword = fight, coin = shop, \? = event/)],
      rule: 'On the parchment chart an event is only a "?" stamp, so the title is the reveal: it names the thing you sailed into ("The Drifting Wreck", "Squall Line"), never the outcome ("Free Gold").' },
  ];

  const references = [
    cite('store', /The seas of Aquadia have never been kind, and the Kraken has never been beaten\./),
    cite('store', /You have a ship, a captain and two crewmates\. That will have to do\./),
    cite('store', /Quick and safe, or slow and rich - your call, every fight\./),
    cite('store', /Lose a pirate and they're gone for good\./),
  ];
  const guide = { sources: Object.values(SOURCES), lexicon, generic: GENERIC, captains, rules, references, shipped };
  const failures = strangerTest(guide, root);
  if (failures.length) throw new Error(`style guide fails the stranger test:\n  ${failures.join('\n  ')}`);
  return guide;
}

/**
 * Fails a guide a stranger couldn't place: a rule with no Deadwater Run term, a citation that isn't verbatim at its
 * line, or a guide that never names the pillars of the game. Run on every build and against a generic control in tests.
 */
function strangerTest(guide, root) {
  const fails = [];
  const lex = guide.lexicon.filter((t) => !GENERIC.includes(t.toLowerCase()));
  for (const r of guide.rules) {
    const hits = termsIn(r.rule, lex);
    if (!hits.length) fails.push(`${r.id}: names nothing only Deadwater Run has ("${r.rule.slice(0, 70)}...")`);
    if (!r.cites || !r.cites.length) fails.push(`${r.id}: no source citation`);
    for (const c of r.cites || []) {
      const p = root && path.join(root, c.file);
      const line = p && fs.existsSync(p) ? fs.readFileSync(p, 'utf8').replace(/\r\n/g, '\n').split('\n')[c.line - 1] : null;
      if (!line || !line.includes(c.quote)) fails.push(`${r.id}: citation ${c.file}:${c.line} is not verbatim`);
    }
  }
  const all = guide.rules.map((r) => r.rule).join(' ');
  for (const pillar of ['Aquadia', 'Kraken', 'plunder', 'gone for good', "Ship's Log", ...guide.captains.map((c) => c.display)]) {
    if (!hasTerm(all, pillar)) fails.push(`guide: never mentions "${pillar}"`);
  }
  return fails;
}

/** Signature terms and Deadwater Run mechanics an event uses - the code half of the evaluator's stranger test. */
function specificitySignals(event, guide) {
  const text = [event.title, event.body, ...(event.choices || []).flatMap((c) => [c.label, c.resultText])].join(' ');
  const contentLex = guide.lexicon.filter((t) => !['Grenze', 'Texturina', "Ship's Log", 'parchment', 'wax', 'Describe', 'RandomOf'].includes(t));
  const terms = termsIn(text, contentLex);
  const outcomes = (event.choices || []).flatMap((c) => flat(c.outcomes));
  const mechanics = [];
  if ((event.choices || []).some((c) => (c.requirements || []).some((r) => r.type === 'CaptainIs'))) mechanics.push('CaptainIs gate (S-captain)');
  if (outcomes.some((o) => o.type === 'LoseCrew')) mechanics.push('LoseCrew: permadeath (S04)');
  if (outcomes.some((o) => o.type === 'Recruit')) mechanics.push('Recruit: the 4-pirate roster (S05)');
  if (outcomes.some((o) => o.type === 'ShipHealth') && outcomes.some((o) => o.type === 'CrewHealth')) mechanics.push('ship vs crew health bars (S03)');
  return { terms, mechanics };
}
function flat(outs) {
  return (outs || []).flatMap((o) => (o && o.type === 'RandomOf' ? [o, ...(o.options || []).flatMap((x) => flat(x.outcomes))] : [o]));
}

function styleMarkdown(guide) {
  const c = (x) => `\`${x.file}:${x.line}\``;
  const group = (area, title) => [`## ${title}`, '', ...guide.rules.filter((r) => r.area === area).flatMap((r) => [
    `- **${r.id}** ${r.rule}`, ...r.cites.map((x) => `  - ${c(x)}: "${x.quote}"`),
  ]), ''];
  return [
    '# Deadwater Run - Style Guide (generated)',
    '',
    '> Built by `node Tools/ContentPipeline/content-pipeline.js style` from the files below. Don\'t edit this file: edit the source and rebuild. Every rule quotes its source line, and the build fails if a quote moves or a rule stops naming something only Deadwater Run has (the stranger test).',
    '',
    `Sources: ${guide.sources.map((s) => `\`${s}\``).join(', ')}.`,
    '',
    ...group('setting', 'Setting and story'),
    ...group('captain', 'The three captains'),
    ...group('voice', 'Voice'),
    ...group('aesthetic', 'Aesthetic (Ship\'s Log art direction)'),
    '## Reference lines',
    '',
    ...guide.references.map((x) => `- "${x.quote}" (${c(x)})`),
    '',
    `## The shipped event pool (\`Tuning/Events.json\`, in the game)`,
    '',
    'How Deadwater Run events already sound. New content matches this voice and never repeats these ideas.',
    '',
    ...guide.shipped.map((e) => `- **${e.title}** (${e.valence}, layers ${e.minLayer}-${e.maxLayer}): ${e.body}`),
    '',
    '## Signature lexicon (the stranger test)',
    '',
    'Terms only Deadwater Run has, pulled from the sources. Every rule above names at least one. Genre words never count: ' + guide.generic.join(', ') + '.',
    '',
    guide.lexicon.map((t) => `\`${t}\``).join(', '),
    '',
  ].join('\n');
}

/** Compact guide for prompts: rules with ids (findings cite them) plus the reference lines and shipped pool. */
function styleForPrompt(guide) {
  return [
    ...guide.rules.map((r) => `- ${r.id} [${r.area}] ${r.rule}`),
    '',
    'Reference lines (store page): ' + guide.references.map((x) => `"${x.quote}"`).join(' '),
    '',
    'Shipped events (Tuning/Events.json - the voice to match; never repeat these ideas or ids):',
    ...guide.shipped.map((e) => `- ${e.id} "${e.title}": ${e.body} | results: ${e.choices.map((c) => `"${c.resultText}"`).join(' ')}`),
  ].join('\n');
}

module.exports = { buildStyleGuide, strangerTest, specificitySignals, styleMarkdown, styleForPrompt, termsIn, GENERIC };
