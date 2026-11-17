/*
 * Deterministic checks for generated events - the half of the critic that never needs an LLM.
 *
 * Everything here is checked against facts.js (the repo), so the rules can't drift from the game.
 * blocker = the game can't do it or the text contradicts the data; major = a guardrail, brief or voice
 * rule is broken; minor = format polish.
 */
'use strict';

/** Parameters of each outcome/requirement, mirroring the 11-Events.md tables. */
const OUTCOME_FIELDS = {
  Gold: { delta: 'int' },
  ShipHealth: { delta: 'int' },
  CrewHealth: { delta: 'int', selector: 'selector' },
  GiveItem: { item: 'item', count: 'posint' },
  AbilityChoice: { count: 'posint' },
  GrantXp: { amount: 'posint', selector: 'selector' },
  Recruit: { unit: 'unit' },
  LoseCrew: { selector: 'selector' },
  RandomOf: { options: 'options' },
};
const REQUIREMENT_FIELDS = {
  MinGold: { amount: 'posint' },
  CrewCountAtLeast: { count: 'posint' },
  HasItem: { item: 'item' },
  CaptainIs: { captain: 'captain' },
};

function assertSpecsMatchFacts(facts) {
  const miss = (spec, names, what) => names.filter((n) => !spec[n]).map((n) => `${what} ${n}`);
  const unknown = [...miss(OUTCOME_FIELDS, Object.keys(facts.event.outcomes), 'outcome'),
    ...miss(REQUIREMENT_FIELDS, facts.event.requirements, 'requirement')];
  if (unknown.length) throw new Error(`checks.js has no parameter spec for: ${unknown.join(', ')} (11-Events.md changed)`);
}

const sentences = (s) => (String(s).match(/[^.!?]+[.!?]+(?=\s|$)|[^.!?]+$/g) || []).filter((x) => x.trim()).length;
const isInt = (v) => Number.isInteger(v);

/** Every outcome in a choice, including the ones nested in RandomOf. */
function flatOutcomes(outcomes) {
  const out = [];
  for (const o of outcomes || []) {
    out.push(o);
    if (o && o.type === 'RandomOf') for (const opt of o.options || []) out.push(...flatOutcomes(opt.outcomes));
  }
  return out;
}

const isCost = (o) => (isInt(o.delta) && o.delta < 0) || o.type === 'LoseCrew';
const isGain = (o) => (isInt(o.delta) && o.delta > 0) || ['GiveItem', 'AbilityChoice', 'GrantXp', 'Recruit'].includes(o.type);

function checkEvents(events, facts, brief = {}, existingIds = []) {
  assertSpecsMatchFacts(facts);
  const F = [];
  const g = facts.guardrails;
  const add = (severity, event, field, rule, problem, fix) => F.push({ source: 'code', severity, event, field, rule, problem, fix });
  const removed = Object.fromEntries(facts.event.removedOutcomes.map((r) => [r.type, r.why]));
  const seen = new Set(existingIds);

  function checkParams(ev, at, obj, spec, kind) {
    for (const [field, type] of Object.entries(spec)) {
      const v = obj[field];
      const p = `${at}.${field}`;
      if (type === 'options') {
        if (!Array.isArray(v) || v.length < 2) { add('blocker', ev, p, 'E06 params', 'RandomOf needs at least 2 weighted options', 'Add options [{weight, outcomes}]'); continue; }
        v.forEach((opt, i) => {
          if (!(opt.weight > 0)) add('blocker', ev, `${p}[${i}].weight`, 'E06 params', `weight ${opt.weight} must be > 0`, 'Use a positive weight');
          checkOutcomes(ev, `${p}[${i}].outcomes`, opt.outcomes, true);
        });
        continue;
      }
      if (v === undefined) { add('blocker', ev, p, 'E06 params', `${kind} is missing "${field}"`, `Set ${field}`); continue; }
      const bad = (what, list) => add('blocker', ev, p, 'E06 params', `"${v}" is not ${what}`, `Use one of: ${list.join(', ')}`);
      if (type === 'int' && !isInt(v)) add('blocker', ev, p, 'E06 params', `${field} must be a whole number`, 'Use an integer');
      if (type === 'posint' && !(isInt(v) && v > 0)) add('blocker', ev, p, 'E06 params', `${field} must be a positive whole number`, 'Use an integer > 0');
      if (type === 'selector' && !facts.event.selectors.includes(v)) bad('an EDwCrewSelector', facts.event.selectors);
      if (type === 'item' && !facts.names.items.includes(v)) bad('an item in ItemTuning.json', facts.names.items);
      if (type === 'unit' && !facts.names.recruitableUnits.includes(v)) bad('a recruitable unit', facts.names.recruitableUnits);
      if (type === 'captain' && !facts.names.captains.includes(v)) bad('a captain', facts.names.captains);
    }
  }

  function checkOutcomes(ev, at, outcomes, nested) {
    if (!Array.isArray(outcomes)) { add('blocker', ev, at, 'E05 outcome', 'outcomes must be a list', 'Use []'); return; }
    outcomes.forEach((o, i) => {
      const p = `${at}[${i}]`;
      if (removed[o.type]) return add('blocker', ev, p, 'E05 outcome', `Outcome "${o.type}" was removed from the game: ${removed[o.type]}`, 'Use an outcome that exists');
      const spec = OUTCOME_FIELDS[o.type];
      if (!spec || !facts.event.outcomes[o.type]) return add('blocker', ev, p, 'E05 outcome', `No outcome class UDwEventOutcome_${o.type}`, `Use one of: ${Object.keys(facts.event.outcomes).join(', ')}`);
      if (nested && o.type === 'RandomOf') add('major', ev, p, 'E05 outcome', 'RandomOf nested in RandomOf is hard to read on the event screen', 'Flatten into one RandomOf');
      checkParams(ev, p, o, spec, `UDwEventOutcome_${o.type}`);
      // Guardrails (placeholders derived from tuning - balance-designer to confirm)
      const d = o.delta;
      const over = (lim, what) => add('major', ev, `${p}.delta`, 'E07 guardrail', `${o.type} ${d > 0 ? '+' : ''}${d} is beyond ${what} (${lim})`, `Keep it within ${lim}`);
      if (o.type === 'Gold' && d > g.goldGainMax) over(g.goldGainMax, 'a fight\'s gold');
      if (o.type === 'Gold' && -d > g.goldLossMax) over(g.goldLossMax, 'the starting gold');
      if (o.type === 'ShipHealth' && d > g.shipGainMax) over(g.shipGainMax, 'a shop repair');
      if (o.type === 'ShipHealth' && -d > g.shipLossMax) over(g.shipLossMax, 'one cannon hit');
      if (o.type === 'CrewHealth' && Math.abs(d) > g.crewDeltaMax) over(g.crewDeltaMax, 'half a crew member\'s HP');
      if (o.type === 'GrantXp' && o.amount > g.xpMax) add('major', ev, `${p}.amount`, 'E07 guardrail', `GrantXp ${o.amount} is beyond a cleared fight's XP (${g.xpMax})`, `Keep it within ${g.xpMax}`);
      if (o.type === 'GiveItem' && o.count > g.itemCountMax) add('major', ev, `${p}.count`, 'E07 guardrail', `GiveItem x${o.count} (max ${g.itemCountMax}; inventory holds ${facts.numbers.maxInventory})`, `Give at most ${g.itemCountMax}`);
      if (o.type === 'AbilityChoice' && o.count > g.abilityChoiceMax) add('major', ev, `${p}.count`, 'E07 guardrail', `AbilityChoice of ${o.count} (rewards offer ${g.abilityChoiceMax})`, `Offer at most ${g.abilityChoiceMax}`);
      if (o.type === 'LoseCrew' && o.selector === 'Captain') add('blocker', ev, `${p}.selector`, 'E06 params', 'LoseCrew can never remove the captain', 'Use Random or Chosen');
    });
  }

  /** "Pay 40 gold" must match a Gold outcome or MinGold requirement of 40 in the same choice. */
  function checkTextNumbers(ev, at, choice) {
    const flat = flatOutcomes(choice.outcomes);
    const kinds = [
      { re: /(\d+)\s*(?:gold|coins?)\b/gi, ok: (n) => flat.some((o) => o.type === 'Gold' && Math.abs(o.delta) === n) || (choice.requirements || []).some((r) => r.type === 'MinGold' && r.amount === n), what: 'Gold outcome or MinGold requirement' },
      { re: /(\d+)\s*(?:hp|health)\b/gi, ok: (n) => flat.some((o) => /Health$/.test(o.type) && Math.abs(o.delta) === n), what: 'health outcome' },
      { re: /(\d+)\s*xp\b/gi, ok: (n) => flat.some((o) => o.type === 'GrantXp' && o.amount === n), what: 'GrantXp outcome' },
    ];
    for (const field of ['label', 'resultText']) {
      for (const k of kinds) {
        for (const m of String(choice[field] || '').matchAll(k.re)) {
          if (!k.ok(+m[1])) add('blocker', ev, `${at}.${field}`, 'E08 text vs data', `Text says "${m[0]}" but no ${k.what} in this choice is ${m[1]}`, 'Make the number in the text match the data');
        }
      }
    }
  }

  function checkVoice(ev, at, text) {
    for (const w of facts.bannedWords) {
      if (new RegExp(`\\b${w.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}\\b`, 'i').test(text)) add('major', ev, at, 'E10 voice', `Uses "${w}" (banned by voice.md)`, 'Drop the stage-pirate word');
    }
  }

  (events || []).forEach((e, ei) => {
    const ev = e.id || `events[${ei}]`;
    if (!/^DA_Event_[A-Z][A-Za-z0-9]*$/.test(e.id || '')) add('blocker', ev, 'id', 'E12 id', `Id "${e.id}" is not DA_Event_<PascalCase>`, 'Rename');
    if (seen.has(e.id)) add('blocker', ev, 'id', 'E12 id', `Id "${e.id}" is used by another event`, 'Rename');
    seen.add(e.id);
    if (!facts.event.valences.includes(e.valence)) add('blocker', ev, 'valence', 'E03 valence', `Valence "${e.valence}" is not one of ${facts.event.valences.join('/')}`, 'Use Good or Bad');
    if (!(e.weight > 0)) add('blocker', ev, 'weight', 'E04 layers', 'Weight must be > 0', 'Use 1');
    if (!isInt(e.minLayer) || !isInt(e.maxLayer) || e.minLayer < facts.event.minLayer || e.maxLayer > facts.event.maxLayer || e.minLayer > e.maxLayer) {
      add('blocker', ev, 'minLayer/maxLayer', 'E04 layers', `Layers ${e.minLayer}-${e.maxLayer} are outside the map (${facts.event.minLayer}-${facts.event.maxLayer})`, 'Fix the range');
    }
    if (brief.layers && (e.minLayer < brief.layers[0] || e.maxLayer > brief.layers[1])) add('major', ev, 'minLayer/maxLayer', 'E13 brief', `Layers ${e.minLayer}-${e.maxLayer} are outside the brief's ${brief.layers.join('-')}`, 'Stay inside the brief');
    for (const f of ['title', 'body']) if (!String(e[f] || '').trim()) add('blocker', ev, f, 'E02 text', `${f} is empty`, 'Write it');
    if (String(e.title || '').length > 40) add('minor', ev, 'title', 'E02 text', `Title is ${e.title.length} chars (max 40)`, 'Shorten');
    const bs = sentences(e.body || '');
    if (e.body && (bs < 2 || bs > 4)) add('minor', ev, 'body', 'E02 text', `Body is ${bs} sentences (2-4)`, 'Trim or extend');
    checkVoice(ev, 'title', e.title || '');
    checkVoice(ev, 'body', e.body || '');

    const choices = Array.isArray(e.choices) ? e.choices : [];
    if (choices.length < facts.event.minChoices || choices.length > facts.event.maxChoices) add('blocker', ev, 'choices', 'E01 choices', `${choices.length} choices (UDwEventDefinition allows ${facts.event.minChoices}-${facts.event.maxChoices})`, 'Fix the choice count');
    if (choices.length && choices.every((c) => (c.requirements || []).length)) add('blocker', ev, 'choices', 'E11 never stuck', 'Every choice has a requirement - a player who meets none is stuck', 'Leave one choice with no requirements');

    choices.forEach((c, ci) => {
      const at = `choices[${ci}]`;
      for (const f of ['label', 'resultText']) if (!String(c[f] || '').trim()) add('blocker', ev, `${at}.${f}`, 'E02 text', `${f} is empty`, 'Write it');
      if (String(c.label || '').length > 40) add('minor', ev, `${at}.label`, 'E02 text', `Label is ${c.label.length} chars (max 40)`, 'Shorten');
      if (sentences(c.resultText || '') > 2) add('minor', ev, `${at}.resultText`, 'E02 text', 'Result text is more than 2 sentences', 'Trim');
      checkVoice(ev, `${at}.label`, c.label || '');
      checkVoice(ev, `${at}.resultText`, c.resultText || '');
      (c.requirements || []).forEach((r, ri) => {
        const p = `${at}.requirements[${ri}]`;
        const spec = REQUIREMENT_FIELDS[r.type];
        if (!spec || !facts.event.requirements.includes(r.type)) return add('blocker', ev, p, 'E09 requirement', `No requirement class UDwEventRequirement_${r.type}`, `Use one of: ${facts.event.requirements.join(', ')}`);
        checkParams(ev, p, r, spec, `UDwEventRequirement_${r.type}`);
      });
      checkOutcomes(ev, `${at}.outcomes`, c.outcomes, false);
      if (Array.isArray(c.outcomes) && !c.outcomes.length && !/nothing/i.test(c.resultText || '')) add('major', ev, `${at}.outcomes`, 'E05 outcome', 'No outcomes and the result text doesn\'t say nothing happens', 'Add an outcome or say nothing happens');
      checkTextNumbers(ev, at, c);
    });

    const all = choices.flatMap((c) => flatOutcomes(c.outcomes));
    if (e.valence === 'Bad' && !all.some(isCost)) add('major', ev, 'valence', 'E14 valence fit', 'Bad event with no cost in any choice', 'Give it a downside, or make it Good');
    if (e.valence === 'Good' && !choices.some((c) => flatOutcomes(c.outcomes).some(isGain))) add('major', ev, 'valence', 'E14 valence fit', 'Good event where no choice gains anything', 'Give one choice an upside, or make it Bad');
  });

  if (brief.count && (events || []).length !== brief.count) add('major', '(batch)', 'events', 'E13 brief', `${(events || []).length} events (brief asks for ${brief.count})`, `Write ${brief.count}`);
  for (const [val, n] of Object.entries(brief.valences || {})) {
    const got = (events || []).filter((e) => e.valence === val).length;
    if (got !== n) add('major', '(batch)', 'valence', 'E13 brief', `${got} ${val} events (brief asks for ${n})`, `Write ${n} ${val}`);
  }
  return F;
}

/** Checks across all batches: the full set should look like the v1.0 event pool. */
function checkPool(allEvents, facts, scopeRange = [10, 12]) {
  const F = [];
  const n = allEvents.length;
  const good = allEvents.filter((e) => e.valence === 'Good').length;
  if (n < scopeRange[0] || n > scopeRange[1]) F.push({ source: 'code', severity: 'major', event: '(pool)', field: 'events', rule: 'P1 scope', problem: `${n} events in total (Scope.md: ~${scopeRange[0]}-${scopeRange[1]})`, fix: 'Adjust the briefs' });
  const share = n ? good / n : 0;
  if (Math.abs(share - facts.event.goodEventChance) > 0.1) F.push({ source: 'code', severity: 'major', event: '(pool)', field: 'valence', rule: 'P2 60/40', problem: `${Math.round(share * 100)}% Good (target ${facts.event.goodEventChance * 100}%)`, fix: 'Rebalance Good/Bad' });
  const ids = allEvents.map((e) => e.id);
  const dup = ids.filter((id, i) => ids.indexOf(id) !== i);
  if (dup.length) F.push({ source: 'code', severity: 'blocker', event: '(pool)', field: 'id', rule: 'P3 ids', problem: `Duplicate ids: ${[...new Set(dup)].join(', ')}`, fix: 'Rename' });
  for (let layer = facts.event.minLayer; layer <= facts.event.maxLayer; layer++) {
    for (const v of facts.event.valences) {
      if (!allEvents.some((e) => e.valence === v && e.minLayer <= layer && e.maxLayer >= layer)) {
        F.push({ source: 'code', severity: 'minor', event: '(pool)', field: 'layers', rule: 'P4 coverage', problem: `No ${v} event can appear on layer ${layer} (the resolver falls back to the other valence)`, fix: 'Widen a layer range' });
      }
    }
  }
  return F;
}

module.exports = { checkEvents, checkPool, flatOutcomes, OUTCOME_FIELDS, REQUIREMENT_FIELDS };
