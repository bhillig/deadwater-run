# Open Questions for Brandon

- Every creative or design call the architecture needs. Agents never pick; they build the recommended default behind a data flag, clearly marked as a placeholder, until you answer.
- **Where it lives** = the data field that holds your answer. Almost all answers are data, not code. Questions marked **(code)** change behaviour that data can't express.
- **Needed by** follows the critical path in `Roadmap.md`.

## Top 5 (they shape the most work)
1. **K1** - What is the Conqueror's "one extra move"?
2. **C7** - How do cannons work (stand at the cannon? once per turn? uses the action?)
3. **B1-B3** - Kraken tentacles: how many, how much HP, what destroying one does, does it hurt the Kraken.
4. **K4** - What are the Team Leader's synergy moves?
5. **M2 + P2** - "5 fights guaranteed" exact or minimum per path; the perk pools (shared or per archetype, 2 or 3 choices).

## Combat and board (needed by Oct 27)

| ID | Question | Options | Recommendation | Where it lives |
|---|---|---|---|---|
| C1 | Grid movement and range: 4-way or 8-way? Range counted how? | a) 4-way, Manhattan range b) 8-way, Chebyshev range c) 8-way move, Manhattan range | a) - easiest to read on a ship deck, classic tactics feel | `DT_CombatTuning.Adjacency`, `RangeMetric` |
| C2 | Can units walk through other units? | a) Through allies, not enemies b) Nobody c) Everybody | a) | `bAllowPassThroughAllies` |
| C3 | Can a unit split its move (move 2, attack, move 3)? | a) Yes b) No, one move per turn | a) - matches "move or act in any order" | `bAllowSplitMovement` |
| C4 | Boarding seam: gap tiles or decks touching? Extra cost? Who dies on the seam when a ship sinks? | a) Decks touch along the boarding edge, cost 1, nobody stands "between" b) 1 neutral gap tile (gangplank), cost 1, survives sinking c) 1 gap tile, cost 2 | a) - simplest, no ambiguity about the seam | `SeamWidth`, `SeamMoveCost`, `bSeamStandable` |
| C5 | Sniper's nest: how to get up, how many fit, what does "hide" mean? | a) Ladder tile, climb cost 2, 1 unit, melee can't target the nest, +2 range from it b) Same but melee can target from the ladder c) Nest only gives range and LOS, no hiding | a) | Tile layout + `ClimbMoveCost`, `RangeBonusPerElevation`, ability `bCanTargetElevated` |
| C6 | What blocks line of sight, and what is "within viewing distance" (Grapple)? | a) Masts/cabins block everyone; crates/rails block deck units only; viewing distance = LOS + range b) Only masts/cabins block c) No LOS on ships, range only | a) | Tile `BlockerHeight`; Grapple `bRequiresLineOfSight` |
| C7 | Cannons: must the crew member stand at a cannon? Once per cannon per turn? Uses the action? What can they hit? | a) Stand on/next to own cannon tile, uses the action, each cannon once per turn, hits the enemy ship only b) Same but any crew can fire from anywhere on own ship c) Cannons can also target enemy crew on deck (splash) | a) - makes positioning and the two cannons matter | `FDwTargetingRules.RequiredSourceTileTags`, cannon usage rule in `UDwGA_FireCannon` |
| C8 | Can crew attacks damage the ship, or only cannons? | a) Only cannons (and some abilities) b) Melee attacks on a deck tile deal small hull damage | a) | Ability targeting patterns |
| C9 | Between fights: does HP carry over? MP? Any MP regen in a fight? | a) HP carries, MP refills each fight, no regen b) Both carry over c) HP carries, MP refills, +X MP per turn | a) - makes the restore items and repairs meaningful without starving abilities | `DT_RunTuning.bRefillManaOnFightStart`, `ManaRegenPerTurn` |
| C10 | If the captain dies, does the run end? | a) No - lose only when all crew are dead (captain abilities and combat rules stop, run rules stay) b) Yes, captain death = defeat c) Captain can't die, gets knocked out for the fight | Your call - affects drama and balance; a) is closest to the design text | `FDwOutcomeConfig.bDefeatOnCaptainDeath` |
| C11 | Kill XP when the kill is from a cannon sinking or a damage-over-time effect? | a) Sinking gives no kill XP; DoT kill credits the caster b) Sinking gives kill XP to whoever fired the last shot | a) | Death handling (code is the same either way, flag in `DT_CombatTuning`) |
| C12 | One action sinks the enemy ship and kills your last boarders - win or lose? | a) Lose (all crew dead) b) Win (enemy ship sunk first) | a) - consistent with "if player crew are on an enemy ship when destroyed they die as well" | Outcome priority list |
| C13 | Damage preview before confirming? Undo a move? | a) Preview yes, no undo b) Preview and undo-move-if-no-action-taken c) Neither | a) - undo complicates GAS state | UI + interaction |
| C14 | Enemy turn: all enemies act one by one in a fixed order? Auto-end player turn when nobody can act? | a) Fixed order, auto-prompt "End turn?" b) Fixed order, auto-end c) Fastest/initiative order | a) | Encounter slot order; UI setting |
| C15 | "For one turn" (freeze, grab) means? | a) Through the target's next turn, gone after it b) Until the start of the caster's next turn | a) | `UDwTurnDurationComponent` defaults |

## Captains (needed by Nov 2; K1 by Oct 27)

| ID | Question | Options | Recommendation | Where it lives |
|---|---|---|---|---|
| K1 | Conqueror's "one extra move" | a) A second action per turn (2 actions + normal move) b) Extra movement (+N tiles) c) A full second move-and-act turn | Your call (a is the strongest reading of "significantly stronger") | `GE_ConquerorExtraMove` (`MaxActionPoints` or `MoveRange`) |
| K2 | "Conqueror can only use a sword" means? | a) Basic attack is a sword and he can't learn firearm abilities b) Also can't use items c) Only flavour | a) | `AllowedWeaponTags` on the captain |
| K3 | Conqueror kill bonus: applies right away mid-fight? Do tentacles/Kraken count? | a) Immediately; tentacles count, Kraken body ends the run anyway b) From the next fight; tentacles don't count | a) | `FDwCaptainRules`, unit tag filter |
| K4 | Team Leader synergy moves - what are they? | a) Passive: attacking while adjacent to an ally deals +X% damage b) Combo abilities per archetype pair, costing both units' actions c) Adjacent allies grant a small buff (e.g. +damage reduction) | a) for v1 (one rule, all data); b) is richer but needs content per pair | `Rule.Captain.Synergy`, `SynergyDamageMultiplier` (a) or synergy ability defs (b) |
| K5 | Defend - how does it work? ("move onto them") | a) An action: move adjacent to an ally; until your next turn, single-target attacks on that ally hit the captain b) Passive: any adjacent ally is protected automatically c) Captain shares the ally's tile | a) - no tile stacking, clear counterplay; AoE not redirected | `GA_Defend`, `GE_Guarded`, redirect rule |
| K6 | Grapple: which tile does the target land on? Across ships? Out of the nest? MP cost? | a) Nearest free tile next to the captain along the line; works across ships and out of the nest b) Can't pull from the nest c) Pull only on the same ship | a) | `UDwGA_Grapple` + tuning |
| K7 | Veteran Crew: "+50% XP and levels up twice as fast" | a) Only +50% XP b) Only half the XP needed per level (2x speed) c) Both (about 3x speed) | a) or b) - c) likely too strong; your call | `XpMultiplier`, `LevelThresholdMultiplier` |
| K8 | After an enemy cowers, what happens to its Fear? | a) Resets to 0 b) Stays at 3 (cowers every turn) c) Drops to 1 | a) - b) is a lock | `DT_CombatTuning.FearCowerPolicy` |
| K9 | Dread Reputation: starting Fear could reach 3 and cause instant surrender | a) Cap starting Fear at 2 b) No cap (later fights can be won on turn 1) c) Cap at 1 | a) | `StartingFearCap` |
| K10 | Is the Kraken immune to Fear, freeze (Eye of the Tiger), grapple? | a) Immune to Fear and grapple; freeze works on tentacles only b) Immune to all three c) Not immune | a) | `Unit.Immune.*` tags |
| K11 | Smoke and Fire damage to afraid enemies? Six Pistols: enemies only, LOS needed, can repeat targets? | a) Damage from tuning; Six Pistols: enemies in LOS, repeats allowed b) Six Pistols hits anyone in LOS including the ship | a) | Ability tuning + targeting |
| K12 | Charge: straight line only? Blocked by obstacles? Eye of the Tiger: any range, no LOS? | a) Straight line, stops at obstacles; Eye ignores range and LOS b) Charge can path around obstacles | a) | `UDwGA_Charge` |
| K13 | MP costs and damage for all 6 captain abilities; Conqueror base stats | Balance Designer proposes 2-3 sets from simulations | Tuning | `DT_AbilityTuning`, `DT_UnitTuning` |
| K14 | Same starting crew (Swordsman + Sniper) for every captain? Captain stats besides the Conqueror? | a) Same crew; Team Leader and Fearmongerer captains use base crew stats b) Different crews per captain | a) | `StartingCrew`, unit rows |

## Progression and economy (needed by Nov 2)

| ID | Question | Options | Recommendation | Where it lives |
|---|---|---|---|---|
| P1 | XP: kill XP, clear XP, curve, max level | Balance Designer proposes; target ~1 level per encounter | Tuning | `DT_ProgressionTuning`, `XpOnKill`, `ClearXp`, `MaxLevel` |
| P2 | Level-up stat gains per archetype; perk pools; 2 or 3 perk choices | a) Shared pool + a few archetype perks, 3 choices b) Per-archetype pools, 2 choices c) One shared pool, 3 choices | a) | `UDwPerkPool`, `PerkChoices`, unit rows |
| P3 | Level-ups mid-fight or after? | a) After the fight b) Immediately on the kill | a) | Flow (code is the same) |
| P4 | Ability slots per crew member; can abilities be replaced? | a) 3 slots + basic attack, replace when full b) Unlimited c) 2 slots | a) | `MaxAbilitySlots` |
| P5 | "A new move" on victory: how? | a) Choose 1 of 3 abilities and who learns it b) Random ability to a random crew member c) Skip allowed for gold | a) | `RewardAbilityChoices` |
| P6 | Plunder: 1.5x gold plus a random item always, or a chance? | a) 1.5x gold + item chance 50% b) 1.5x gold + always 1 item c) 1.5x gold only, item on surrender | Your call | `PlunderItemChance` |
| P7 | Repair: 100 gold for how much hull? | a) +100 HP b) Full repair c) +50% max | a) - keeps the heal-or-upgrade tension | `RepairAmount` |
| P8 | Recruit/disband: cost, which archetypes, recruit level, refund | a) Swordsman/Sniper (+ any new archetypes), cost ~80, level = crew average - 1, no refund b) Random archetype | Tuning + N5 | `DT_UnitTuning.RecruitCost`, shop recruit pool |
| P9 | Ship upgrades list and prices | a) +50 max HP, +20 cannon damage, third cannon b) a) + armour (-X% ship damage) c) Only HP and cannon damage | a) | `UDwShipUpgradeDefinition` assets |
| P10 | Items: restore amounts, who can be targeted, inventory shared or per crew, cap | a) Shared ship inventory, cap 6, self or adjacent ally, +40 HP / +10 MP b) Per crew inventory | a) | `UDwItemDefinition`, `DT_ItemTuning` |
| P11 | Starting gold | Tuning (0, 50 or 100) | Balance | `StartingGold` |

## Content (needed by Nov 8 - week 3 is content week)

| ID | Question | Options | Recommendation | Where it lives |
|---|---|---|---|---|
| N1 | The ~12 crew abilities (names, effects, numbers) | Balance Designer + Gameplay Engineer can draft 2-3 sets of 12 within the existing ability classes for you to pick from | Ask for drafts by Nov 4 | `DA_Ability_*`, pools |
| N2 | 4-5 enemy crews: names, units, ship layouts, personalities | e.g. gunners / boarders / snipers / mixed / elite | Drafts on request | `DA_Encounter_*`, `DA_AiProfile_*` |
| N3 | Difficulty by depth: which crews appear where | a) Tier 1 layers 1-3, tier 2 layers 4-6, tier 3 layers 7-9 b) Any crew anywhere, scaled by level | a) | Encounter `MinLayer`/`MaxLayer` |
| N4 | The 10-12 events (text, choices, outcomes) | Use the event format in `Editor-Tools.md` | - | `DA_Event_*` |
| N5 | Any recruit archetypes beyond Swordsman and Sniper? | a) None in v1 b) 1-2 more (e.g. gunner, surgeon) | Your call; each adds abilities and art | Unit definitions |
| N6 | Crew names: fixed, random from a list, or none? | a) Random from a name list b) Archetype names only | a) adds attachment to permadeath | Name list DataAsset |

## Map (needed by Nov 2)

| ID | Question | Options | Recommendation | Where it lives |
|---|---|---|---|---|
| M1 | 8 or 9 nodes: random per run? | a) Random per seed b) Always 9 | a) | `MinPathNodes`/`MaxPathNodes` |
| M2 | "5 fights guaranteed" | a) At least 5 on every path b) Exactly 5 on every path c) At least 5 somewhere on the map | a) | `MinFightsPerPath` (+ an exact flag if b) |
| M3 | First node type; map width | a) First node is a fight, 3 lanes b) First node random, 3 lanes c) 4 lanes | a) | `FirstLayerType`, `LaneCount` |
| M4 | Event nodes: show "?" or good/bad? | a) "?" only b) Show valence | a) | UI only |
| M5 | "Guaranteed shop option before the boss" | a) The last node before the boss always offers a shop choice b) Every path has a shop in the last 3 nodes | a) | `bShopOptionBeforeBoss` |
| M6 | Does the map show which crew a fight is? | a) Icon only b) Crew name and difficulty | a) | UI only |

## Kraken (needed by Nov 8)

| ID | Question | Options | Recommendation | Where it lives |
|---|---|---|---|---|
| B1 | How many tentacles, how much HP, do they regrow? | a) 4 tentacles, 60 HP, no regrow b) 4 tentacles, 80 HP, regrow after 3 turns c) 6 tentacles, 40 HP, no regrow | a) | `DT_KrakenTuning` |
| B2 | What does destroying a tentacle do? | a) -1 action per turn per 2 tentacles b) -10% damage per tentacle c) Both | b) - always matters, never removes all actions | `FDwKrakenTentacleRules` |
| B3 | Does damaging tentacles damage the Kraken's 500 HP? | a) No, separate b) Yes, tentacle damage also hits the body c) Destroying a tentacle deals a chunk to the body | a) - makes it a clear choice | `FDwKrakenTentacleRules` |
| B4 | What can hit the body? | a) Cannons and ranged; melee only hits tentacles b) Everything in range c) Only cannons | a) | Targeting tags |
| B5 | Arena: player ship only, nothing to board? | a) Yes b) A rock/wreck crew can step onto | a) | `DA_Deck_KrakenArena` |
| B6 | Telegraphing | a) Show every intent with its target (Slay the Spire style) b) Show only big moves (Slam, Arms of Fury, Eyes of the Sea) c) Show move type, not target | a) - readable, rewards planning | `FDwAiIntent.bTelegraph` |
| B7 | Phase change at 30%: immediately or next turn? | a) On its next turn, with a banner b) Immediately mid-turn | a) | Brain rule |
| B8 | Do Kranky Swipe / Arms of Fury hit the ship too? ("Whole team deals 20" reads as "takes 20") | a) Crew only b) Crew and ship | a) | Ability targeting |
| B9 | Kraken immunities | See K10 | - | Unit tags |

## UI and shell (needed by Nov 2)

| ID | Question | Options | Recommendation | Where it lives |
|---|---|---|---|---|
| U1 | Game speed option? | a) 1x/1.5x/2x setting b) None | a) - cheap, helps the ~30 min target | `UDwGameUserSettings.GameSpeed` |
| U2 | Camera | a) Fixed angle, small pan/zoom b) Free orbit | a) | `ADwCameraRig` |
| U3 | Settings list | a) Resolution, window mode, vsync, 4 volumes, game speed, screen shake b) a) + colourblind-friendly highlights | b) if time allows | Settings |
| U4 | Run summary contents | Captain, seed, nodes, fights by outcome, gold, crew lost, kills, time | As listed | `FDwRunStats` |
| U5 | Abandon run from pause goes to the summary? | a) Yes b) Straight to menu | a) | Shell flow |
| U6 | Art direction: palette, fonts, logo | Needs your sign-off; asset-scraper brings font options (DW-005) | - | UI assets |

## Ownership and process

| ID | Question | Options | Recommendation |
|---|---|---|---|
| O1 | Who integrates art (Synty characters, ships, animations, environment, level dressing)? No roster agent explicitly owns it | a) gameplay-engineer (already owns VFX and Blueprints) b) ui-engineer for UI art only, gameplay-engineer for the rest | a) |
| O2 | Animations: Synty Pirate characters may not include combat animations | a) Buy an animation pack (asset-scraper options, DW-005) b) Synty's own animation packs c) Minimal procedural (lunge/flash) | Decide with DW-005 results |
| O3 | Logs in shipped builds (needs a source engine) | a) Launcher engine, no logs in Shipping b) Ship a Test build on a beta branch for diagnostics c) Switch to a source engine | a) + b) |
| O4 | Update agent definitions for this architecture | Proposed: `gameplay-engineer.md` rule line (nodes/fights) and owned folders (Core/, Board/, Presentation/, Shell/); `tools-engineer.md` adds `Source/DeadwaterRun/Sim/` and `Dev/`; `ui-engineer.md` unchanged | Approve the edits (orchestrator or Brandon makes them) |
| O5 | `DeadwaterRunPlayerController` had your WIP "first-move message" (Oct 23 status) | a) Delete with the template b) Keep a copy for reference in a branch | Your call before DW-002 |

## Doc inconsistencies to fix

| ID | Where | Now says | Should say (per today's decisions) |
|---|---|---|---|
| D1 | `Docs/Scope.md` Must ship, Run loop | "3 captains (1 unique ability each)" | 3 captains, 2 combat abilities each, plus their run rules **(Fixed 2026-10-24)** |
| D2 | `Docs/Scope.md` Must ship, Run loop | "7-8 node map" | 8-9 nodes (excluding the boss), 5 fights guaranteed **(Fixed 2026-10-24)** |
| D3 | `context/offer.md` | "7-8 node runs" | 8-9 node runs |
| D4 | `.claude/agents/gameplay-engineer.md` rules | "7-8 nodes before the boss, 4 fights guaranteed" | 8-9 nodes, 5 fights guaranteed |
| D5 | `Docs/Scope.md` Combat | "plunder on crew wipe" | Plunder on crew wipe or surrender (Fearmongerer) |
| D6 | `DeadwaterRun.md` | Today's fixed decisions (square grid boards + seam + nest, Kraken arena, level-up = stat + 1 of 2-3 perks) are not recorded | Add them to the design doc |
| D7 | `DeadwaterRun.md` Dread Reputation | "every later right" | "every later fight" |
| D8 | `DeadwaterRun.md` Kranky swipe | "Whole team deals 20 damage" | "Whole team takes 20 damage" (confirm, B8) |
| D9 | `DeadwaterRun.md` Progression | "When the crew member levels up (on average once per encounter)" is an unfinished sentence | Complete it with the level-up rule (D6) |
| D10 | `DeadwaterRun.md` Progression vs At Each Fight Node | Plunder = "1.5x the loot (gold and a new move)" vs "gold and/or random item" | One rule (P6) |
| D11 | `DeadwaterRun.md` Veteran Crew | "+50% XP and levels up twice as fast" - two different numbers | One rule (K7) |
