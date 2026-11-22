/*
 * Game facts for the content pipeline, read from the repo on every run - never hand-copied.
 *
 * The writer is told these facts and the critic checks against them, so a tuning or architecture change
 * reaches both agents without anyone editing a prompt. If a source can't be parsed the run stops: a critic
 * that checks against nothing would pass everything.
 */
'use strict';

const fs = require('fs');
const path = require('path');

function readText(root, rel) {
  const p = path.join(root, rel);
  if (!fs.existsSync(p)) throw new Error(`facts: missing ${rel}`);
  return fs.readFileSync(p, 'utf8').replace(/\r\n/g, '\n');
}

const readTuning = (root, name) => JSON.parse(readText(root, `Tuning/${name}.json`));

function must(value, what) {
  if (value === undefined || value === null || (Array.isArray(value) && value.length === 0) || Number.isNaN(value)) {
    throw new Error(`facts: could not read ${what} - the source format changed; update lib/facts.js`);
  }
  return value;
}

function row(rows, name, file) {
  return must(rows.find((r) => r.Name === name), `${file} row "${name}"`);
}

/** `UDwEventOutcome_X` rows of the outcome table in 11-Events.md: { X: "parameter text" }. */
function parseOutcomes(doc) {
  const out = {};
  for (const m of doc.matchAll(/^\|\s*`UDwEventOutcome_(\w+)`\s*\|\s*(.+?)\s*\|\s*$/gm)) out[m[1]] = m[2];
  return must(Object.keys(out).length ? out : null, '11-Events.md outcome table');
}

function buildFacts(root) {
  const events = readText(root, 'Docs/Architecture/11-Events.md');
  const runMap = readText(root, 'Docs/Architecture/09-Run-Map.md');
  const defH = readText(root, 'Source/DeadwaterRun/Data/DwEventDefinition.h');
  const typesH = readText(root, 'Source/DeadwaterRun/Data/DwDataTypes.h');
  const gdd = readText(root, 'DeadwaterRun.md');
  const voice = readText(root, 'Docs/ContentPipeline/voice.md');

  const outcomeParams = parseOutcomes(events);
  const removedOutcomes = [...events.matchAll(/`UDwEventOutcome_(\w+)` is removed([^\n]*)/g)].map((m) => ({ type: m[1], why: m[2].replace(/^[,.]\s*/, '').trim() }));
  for (const r of removedOutcomes) delete outcomeParams[r.type];

  const reqLine = must((/^`UDwEventRequirement`[^:\n]*:([^\n]+)/m.exec(events) || [])[1], '11-Events.md requirement list');
  const requirements = must([...reqLine.matchAll(/`_(\w+)`/g)].map((m) => m[1]), 'requirement classes');
  const selectors = must([...((/`EDwCrewSelector`\s*\(([^)]*)\)/.exec(events) || [])[1] || '').matchAll(/`(\w+)`/g)].map((m) => m[1]), 'EDwCrewSelector values');

  const valenceBody = must((/enum class EDwEventValence[^{]*\{([^}]*)\}/.exec(typesH) || [])[1], 'EDwEventValence in DwDataTypes.h');
  const valences = must(valenceBody.split(',').map((s) => s.replace(/\/\/.*$|\/\*.*?\*\//g, '').trim()).filter(Boolean), 'valence values');

  const minChoices = +must((/MinChoices\s*=\s*(\d+)/.exec(defH) || [])[1], 'UDwEventDefinition::MinChoices');
  const maxChoices = +must((/MaxChoices\s*=\s*(\d+)/.exec(defH) || [])[1], 'UDwEventDefinition::MaxChoices');
  const pathNodes = must(/`MinPathNodes`,\s*`MaxPathNodes`\s*\|\s*(\d+),\s*(\d+)/.exec(runMap), '09-Run-Map.md Min/MaxPathNodes');
  const maxLayer = +pathNodes[2];

  const run = readTuning(root, 'RunTuning')[0];
  const econ = readTuning(root, 'EconomyTuning')[0];
  const fight = row(readTuning(root, 'EncounterTuning'), 'Placeholder', 'EncounterTuning');
  const ship = row(readTuning(root, 'ShipTuning'), 'PlayerShip', 'ShipTuning');
  const units = readTuning(root, 'UnitTuning');
  const crew = row(units, 'Swordsman', 'UnitTuning');
  const items = readTuning(root, 'ItemTuning');
  const captains = readTuning(root, 'CaptainTuning').map((c) => c.Name);
  const level1 = row(readTuning(root, 'ProgressionTuning'), '1', 'ProgressionTuning');

  const gddMechanics = must((/## Game Mechanics\n([\s\S]*?)\n## AI Architecture/.exec(gdd) || [])[1], 'DeadwaterRun.md "Game Mechanics"');
  // Words the voice sheet bans outright: the quoted words on its 'No "..."' line.
  const bannedLine = (/^- .*\bNo\s+"[^\n]*$/m.exec(voice) || [''])[0];
  const bannedWords = [...bannedLine.matchAll(/"([^"]+)"/g)].map((m) => m[1].toLowerCase());

  return {
    sources: ['DeadwaterRun.md', 'Docs/Architecture/11-Events.md', 'Docs/Architecture/09-Run-Map.md',
      'Source/DeadwaterRun/Data/DwEventDefinition.h', 'Source/DeadwaterRun/Data/DwDataTypes.h', 'Tuning/*.json', 'Docs/ContentPipeline/voice.md'],
    event: {
      valences, minChoices, maxChoices, minLayer: 1, maxLayer,
      goodEventChance: must(run.GoodEventChance, 'RunTuning.GoodEventChance'),
      outcomes: outcomeParams, removedOutcomes, requirements, selectors,
      rules: [
        'Event health loss floors at 1 for crew and ship (C10a placeholder) - events never kill',
        'LoseCrew never removes the captain',
        'MP refills at every fight start, so there is no mana outcome (C9)',
        `Recruit does nothing when the crew is full (max ${econ.MaxCrew} including the captain) - gate it with CrewCountAtMost ${econ.MaxCrew - 1}`,
        `Inventory holds at most ${econ.MaxInventory} items in total`,
        'A choice with no outcomes imports as the Nothing outcome, so its result text must say nothing happens',
      ],
    },
    names: {
      items: items.map((i) => i.Name),
      recruitableUnits: units.filter((u) => u.RecruitCost > 0).map((u) => u.Name),
      captains,
    },
    numbers: {
      startingGold: econ.StartingGold, fightGold: fight.BaseGold, fightXp: fight.ClearXp, repairCost: econ.RepairCost,
      repairAmount: econ.RepairAmount, itemPrice: items[0].Price, recruitCost: crew.RecruitCost,
      shipMaxHealth: ship.MaxHealth, cannonDamage: ship.CannonDamage, crewMaxHealth: crew.MaxHealth, xpToLevel2: level1.XpToNext,
      maxCrew: econ.MaxCrew, maxInventory: econ.MaxInventory,
    },
    // Placeholder guardrails derived from the tuning above (balance-designer to confirm): an event shouldn't pay
    // more than a fight, hurt more than a cannon hit, or out-level a cleared encounter.
    guardrails: {
      goldGainMax: fight.BaseGold, goldLossMax: econ.StartingGold,
      shipGainMax: econ.RepairAmount, shipLossMax: ship.CannonDamage,
      crewDeltaMax: Math.round(crew.MaxHealth / 2), xpMax: fight.ClearXp, itemCountMax: 2, abilityChoiceMax: econ.RewardAbilityChoices,
    },
    bannedWords,
    gddMechanics,
    voice,
  };
}

/** Compact facts for prompts (no GDD/voice text - those are added as their own sections). */
function factsForPrompt(f) {
  const { gddMechanics, voice, ...rest } = f;
  return JSON.stringify(rest, null, 2);
}

module.exports = { buildFacts, factsForPrompt };
