# Open Questions for Brandon

- Every creative or design call the architecture needs. Agents never pick; they build the recommended default behind a data flag, clearly marked as a placeholder, until you answer.
- **Where it lives** = the data field that holds your answer. Almost all answers are data, not code. Questions marked **(code)** change behaviour that data can't express.
- **Needed by** follows the critical path in `Roadmap.md`.
- Answered questions move to **Resolved** at the bottom with the decision and the docs that now carry it. Last update: 2026-11-10 (K7, P6, P11 answered during DW-201/DW-203; earlier: C1-C15, K1-K4, U1-U5 from `AnswersToOpenQuestions.md`).

## Top 5 (they shape the most work)
1. **C8a** - Explosive barrels: new in your C8 answer, not in the design doc or Scope. In v1 or post-launch?
2. **K5** - Defend: now that captain death = defeat, should the captain still take hits for allies?
3. **B1-B3** - Kraken tentacles: how many, how much HP, what destroying one does, does it hurt the Kraken.
4. **M2 + P2** - "5 fights guaranteed" exact or minimum per path; the perk pools (shared or per archetype, 2 or 3 choices).
5. **K1a + K4a** - Conqueror's two actions (can they repeat?) and what the Team Leader's synergy bonus applies to.

## Follow-ups from the 2026-10-25 answers

| ID | From | Question | Options | Recommendation | Where it lives | Needed by |
|---|---|---|---|---|---|---|
| C5a | C5 | Your answer listed "ladder tile, climb cost 2, 1 unit, melee can't target the nest" but left out option a's "+2 range from it". Is there a range bonus from the nest? And can a unit in the nest use melee on the deck? | a) +2 range from the nest; melee blocked both ways (can't hit into or out of the nest) b) No range bonus (nest = LOS over crates + melee protection); melee blocked both ways c) +2 range; a nest unit can melee the ladder tile | a) - the nest is the sniper's spot; built as placeholder now | `DT_CombatTuning.RangeBonusPerElevation`, melee `bCanTargetElevated` | Oct 27 |
| C6a | C6 | Do units block line of sight? | a) No, only masts/cabins and crates/rails b) Enemy units block c) All units block | a) - easiest to read; placeholder now | `FDwLineOfSight` **(code)** | Oct 27 |
| C8a | C8 | Explosive barrels are new: not in `DeadwaterRun.md` and not in Scope (Scope rule: new ideas go on the post-launch list). In v1? | a) Post-launch; v1 ship damage = cannons + ship-targeting abilities only b) v1: barrels are 1-HP neutral prop units placed in deck layouts; destroying one deals X to that ship and Y to adjacent units (needs numbers, how many per ship, whether either side can shoot them, whether it counts as a kill, VFX/SFX, AI awareness - about 3-4 agent-days across gameplay, AI, art) c) v1, ship damage only, no splash | a) - protects Dec 3; b) fits the architecture cleanly if you want it (no new systems, just a unit definition + a death effect) | Scope; if b: `DA_Unit_ExplosiveBarrel`, deck layout slots, `DT_UnitTuning` | Nov 2 |
| C9a | C9 | MP now refills every fight, so the mana restore item only matters mid-fight. Can items be used outside combat? | a) Combat only (one item path, through GAS) b) Health restore also usable from the map/shop screens | a) | `UDwItemDefinition` usage flag **(code if b)** | Nov 2 |
| C10a | C10 | Can event outcomes kill? With captain death = defeat, a bad event could end the run off-screen | a) Event Health loss floors at 1 for everyone; ship Health floors at 1 b) Can kill crew, captain floors at 1 c) Can kill anyone; captain death = defeat | a) - deaths happen in fights the player can play | `UDwEventOutcome_CrewHealth`, `_ShipHealth` | Nov 2 |
| C10b | C10 | Should enemies hunt the captain? | a) Every crew has a moderate captain-pressure weight b) Only some crews (e.g. an "assassin" crew) hunt the captain; others ignore him c) No captain preference | b) - a personality you can see; ties to N2 | `UDwAiProfile` captain-pressure weight | Nov 8 |
| C13a | C13 | With no undo, how is a move confirmed? | a) Hover shows the path, one click on a highlighted tile moves b) First click previews, second click confirms c) a) with a setting for b) | a) - keeps the pace; the hover path is the preview | `UDwCombatInteraction` | Oct 27 |
| K1a | K1 | Conqueror's 2 actions: can both be the same thing? | a) Any two actions, repeats allowed (two sword attacks, or the same ability twice if MP allows); each cannon still fires once per turn b) The two actions must differ c) Captain abilities at most once per turn, everything else may repeat | a) - simplest, MP costs limit abuse; balance designer checks in the simulator | `UDwGameplayAbility` per-turn use limit **(code if b or c)** | Oct 27 |
| K2a | K2 | Can the Conqueror fire cannons? (cannons are not firearm abilities) | a) Yes b) No | a) | `BlockedAbilityTags` | Nov 2 |
| K4a | K4 | Synergy "+X% when attacking while adjacent to an ally": which damage, and does the captain count? | a) Basic attacks and damaging abilities of every crew member including the captain; any ally (captain included) counts as the neighbour; not cannons b) Basic attacks only c) Everything including cannon shots | a) | `FDwSynergyRules`, ability tag filter | Nov 2 |
| U3a | U3 | Colourblind-friendly highlights: one palette or several? | a) One colourblind-safe palette, plus distinct patterns per highlight type in both palettes b) Three palettes (deuteranopia, protanopia, tritanopia) | a) | `DA_HighlightPalette_*`, `UDwGameUserSettings.HighlightPalette` | Nov 2 |

## Combat and board (needed by Oct 27)

All answered - see Resolved. Follow-ups C5a, C6a, C8a, C9a, C10a, C10b, C13a above.

## Captains (needed by Nov 2; K1 by Oct 27)

K1-K4 answered - see Resolved. Follow-ups K1a, K2a, K4a above.

| ID | Question | Options | Recommendation | Where it lives |
|---|---|---|---|---|
| K5 | Defend - how does it work? ("move onto them") | a) An action: move adjacent to an ally; until your next turn, single-target attacks on that ally hit the captain b) Passive: any adjacent ally is protected automatically c) Captain shares the ally's tile | a) - no tile stacking, clear counterplay; AoE not redirected. **New since C10:** captain death = defeat, so every redirected hit risks the run - consider capping redirected damage or letting Defend grant the captain damage reduction | `GA_Defend`, `GE_Guarded`, redirect rule |
| K6 | Grapple: which tile does the target land on? Across ships? Out of the nest? MP cost? | a) Nearest free tile next to the captain along the line; works across ships and out of the nest b) Can't pull from the nest c) Pull only on the same ship | a) | `UDwGA_Grapple` + tuning |
| K8 | After an enemy cowers, what happens to its Fear? | a) Resets to 0 b) Stays at 3 (cowers every turn) c) Drops to 1 | a) - b) is a lock | `DT_CombatTuning.FearCowerPolicy` |
| K9 | Dread Reputation: starting Fear could reach 3 and cause instant surrender | a) Cap starting Fear at 2 b) No cap (later fights can be won on turn 1) c) Cap at 1 | a) | `StartingFearCap` |
| K10 | Is the Kraken immune to Fear, freeze (Eye of the Tiger), grapple? | a) Immune to Fear and grapple; freeze works on tentacles only b) Immune to all three c) Not immune | a) | `Unit.Immune.*` tags |
| K11 | Smoke and Fire damage to afraid enemies? Six Pistols: enemies only, LOS needed, can repeat targets? | a) Damage from tuning; Six Pistols: enemies in LOS, repeats allowed b) Six Pistols hits anyone in LOS including the ship | a) | Ability tuning + targeting |
| K12 | Charge: straight line only? Blocked by obstacles? Eye of the Tiger: any range, no LOS? | a) Straight line, stops at obstacles; Eye ignores range and LOS b) Charge can path around obstacles | a) | `UDwGA_Charge` |
| K13 | MP costs and damage for all 6 captain abilities; Conqueror base stats; Team Leader synergy X% | Balance Designer proposes 2-3 sets from simulations | Tuning | `DT_AbilityTuning`, `DT_UnitTuning`, `DT_CaptainTuning.SynergyDamageMultiplier` |
| K14 | Same starting crew (Swordsman + Sniper) for every captain? Captain stats besides the Conqueror? | a) Same crew; Team Leader and Fearmongerer captains use base crew stats b) Different crews per captain | a) | `StartingCrew`, unit rows |

## Progression and economy (needed by Nov 2)

| ID | Question | Options | Recommendation | Where it lives |
|---|---|---|---|---|
| P1 | XP: kill XP, clear XP, curve, max level (and kill XP for a Kraken tentacle) | Balance Designer proposes; target ~1 level per encounter | Tuning | `DT_ProgressionTuning`, `XpOnKill`, `ClearXp`, `MaxLevel` |
| P2 | Level-up stat gains per archetype; perk pools; 2 or 3 perk choices | a) Shared pool + a few archetype perks, 3 choices b) Per-archetype pools, 2 choices c) One shared pool, 3 choices | a) | `UDwPerkPool`, `PerkChoices`, unit rows |
| P3 | Level-ups mid-fight or after? | a) After the fight b) Immediately on the kill | a) | Flow (code is the same) |
| P4 | Ability slots per crew member; can abilities be replaced? | a) 3 slots + basic attack, replace when full b) Unlimited c) 2 slots | a) | `MaxAbilitySlots` |
| P5 | "A new move" on victory: how? | a) Choose 1 of 3 abilities and who learns it b) Random ability to a random crew member c) Skip allowed for gold | a) | `RewardAbilityChoices` |
| P7 | Repair: 100 gold for how much hull? | a) +100 HP b) Full repair c) +50% max | a) - keeps the heal-or-upgrade tension | `RepairAmount` |
| P8 | Recruit/disband: cost, which archetypes, recruit level, refund | a) Swordsman/Sniper (+ any new archetypes), cost ~80, level = crew average - 1, no refund b) Random archetype | Tuning + N5 | `DT_UnitTuning.RecruitCost`, shop recruit pool |
| P9 | Ship upgrades list and prices | a) +50 max HP, +20 cannon damage, third cannon b) a) + armour (-X% ship damage) c) Only HP and cannon damage | a) | `UDwShipUpgradeDefinition` assets |
| P10 | Items: restore amounts, who can be targeted, inventory shared or per crew, cap | a) Shared ship inventory, cap 6, self or adjacent ally, +40 HP / +10 MP b) Per crew inventory | a) | `UDwItemDefinition`, `DT_ItemTuning` |

## Content (needed by Nov 8 - week 3 is content week)

| ID | Question | Options | Recommendation | Where it lives |
|---|---|---|---|---|
| N1 | The ~12 crew abilities (names, effects, numbers) | Balance Designer + Gameplay Engineer can draft 2-3 sets of 12 within the existing ability classes for you to pick from | Ask for drafts by Nov 4 | `DA_Ability_*`, pools |
| N2 | 4-5 enemy crews: names, units, ship layouts, personalities | e.g. gunners / boarders / snipers / mixed / elite (see C10b for a captain-hunting crew) | Drafts on request | `DA_Encounter_*`, `DA_AiProfile_*` |
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
| B6 | Telegraphing | a) Show every intent with its target (Slay the Spire style) b) Show only big moves (Slam, Arms of Fury, Eyes of the Sea) c) Show move type, not target | a) - readable, rewards planning; matters more now that a hit on the captain can end the run | `FDwAiIntent.bTelegraph` |
| B7 | Phase change at 30%: immediately or next turn? | a) On its next turn, with a banner b) Immediately mid-turn | a) | Brain rule |
| B8 | Do Kranky Swipe / Arms of Fury hit the ship too? ("Whole team deals 20" reads as "takes 20") | a) Crew only b) Crew and ship | a) | Ability targeting |
| B9 | Kraken immunities | See K10 | - | Unit tags |

## UI and shell (needed by Nov 2)

U1-U5 answered - see Resolved. Follow-up U3a above.

| ID | Question | Options | Recommendation | Where it lives |
|---|---|---|---|---|
| U6 | Art direction: palette, fonts, logo | Needs your sign-off; asset-scraper brings font options (DW-005). Your 2026-10-25 answer confirms the process; the sign-off itself is still to come (also covers the highlight palettes, U3/U3a) | - | UI assets |

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
| D6 | `DeadwaterRun.md` | Today's fixed decisions (square grid boards + seam + nest, Kraken arena, level-up = stat + 1 of 2-3 perks) are not recorded | Add them to the design doc, using the 2026-10-25 board rules: 4-way grid, decks touch at the boarding edge (no seam tile), nest via ladder |
| D7 | `DeadwaterRun.md` Dread Reputation | "every later right" | "every later fight" |
| D8 | `DeadwaterRun.md` Kranky swipe | "Whole team deals 20 damage" | "Whole team takes 20 damage" (confirm, B8) |
| D9 | `DeadwaterRun.md` Progression | "When the crew member levels up (on average once per encounter)" is an unfinished sentence | Complete it with the level-up rule (D6) |
| D10 | `DeadwaterRun.md` Progression vs At Each Fight Node | Plunder = "1.5x the loot (gold and a new move)" vs "gold and/or random item" | One rule (P6) |
| D11 | `DeadwaterRun.md` Veteran Crew | "+50% XP and levels up twice as fast" - two different numbers | One rule (K7) |
| D12 | `Docs/Scope.md` Must ship, Combat | "lose the same ways" | Lose if all crew die, the captain dies, or your ship sinks (C10) |
| D13 | `DeadwaterRun.md` "Win/Lose" (line ~109) | "the player loses if all their crew members die or their ship is destroyed" | Add "or the captain dies" (C10) |
| D14 | `Docs/Scope.md` Must ship, Shell | "settings" | Settings incl. game speed (1x/1.5x/2x) and colourblind-friendly highlights (U1, U3 - U3 is now must-ship, no longer "if time allows") |
| D15 | `DeadwaterRun.md` Conqueror / Team Leader | "one extra move"; "synergy moves (dealing bonus damage/effects)" | "a second action each turn" (K1); "attacking next to an ally deals bonus damage" (K4) |
| D16 | `Docs/Scope.md` | Explosive barrels (C8 answer) are in neither Must ship nor Cut | Add to Must ship or to the post-launch list once C8a is answered |

## Resolved

### Combat and board (answered 2026-10-25)
| ID | Question | Decision | Now in |
|---|---|---|---|
| C1 | Grid movement and range | a) 4-way movement, Manhattan range | `04-Grid-Board.md` (rule is code; flags removed) |
| C2 | Walk through units? | a) Through allies, not enemies | `04-Grid-Board.md` |
| C3 | Split movement? | b) No - one move per turn (move-act or act-move) | `04`, `02` (`UDwGA_Move` zeroes `MovePoints`), `03`, `07` |
| C4 | Boarding seam | a) Decks touch along the boarding edge, cost 1, nobody stands "between" | `04` (no `Seam` tile kind, `BoardingMoveCost`) |
| C5 | Sniper's nest | a) Ladder tile, climb cost 2, 1 unit, melee can't target the nest (range bonus: see C5a) | `04` |
| C6 | Line of sight / viewing distance | a) Masts/cabins block everyone; crates/rails block deck units only; viewing distance = LOS + range (units blocking: C6a) | `04` |
| C7 | Cannons | a) Stand on/next to own cannon tile, uses the action, each cannon once per turn, hits the enemy ship only | `06` (`FDwCannonRules::FindUsableCannon`), `02`, Roadmap DW-104 |
| C8 | Can crew attacks damage the ship? | a) No - only cannons, explosive barrels and some abilities (barrels: C8a) | `06` |
| C9 | HP/MP between fights | a) HP carries, MP refills each fight, no regen | `02`, `03`, `05`, `09`, `11` (`CrewMana` outcome removed), `13` (flags removed) |
| C10 | Captain death ends the run? | b) Yes, captain death = defeat | `03` (`DefeatCaptainDead`), `05`, `07`, `08`, `09`, `16` |
| C11 | Kill XP from sinking / DoT | a) Sinking gives no kill XP; DoT kill credits the caster | `03`, `12` |
| C12 | Sink + last boarders die | a) Lose | `03` (defeats before victories) |
| C13 | Preview / undo | a) Damage preview yes, no undo (move confirm: C13a) | `03`, `14` |
| C14 | Enemy order / end of player turn | a) Fixed order; auto-prompt "End turn?" | `03` (`SideHasAvailableAction`, `OnEndTurnSuggested`), `14` |
| C15 | "For one turn" | a) Through the target's next turn, gone after it | `02` (plus the same-turn skip rule so self-buffs like Eyes of the Sea work), `08` |

### Captains (answered 2026-10-25)
| ID | Question | Decision | Now in |
|---|---|---|---|
| K1 | Conqueror's "one extra move" | a) A second action per turn (2 actions + normal move) (repeats: K1a) | `05`, `02` (`GE_ConquerorExtraMove` +1 `MaxActionPoints`) |
| K2 | "Only a sword" | a) Basic attack is a sword; can't learn firearm abilities (cannons: K2a) | `05` (`BlockedAbilityTags` replaces `AllowedWeaponTags`), `10` |
| K3 | Kill bonus timing; tentacles | Applies immediately; Kraken tentacles don't count as kills | `05` (`KillBonusIgnoredUnitTags`), `08` |
| K4 | Team Leader synergy | a) Passive: attacking while adjacent to an ally deals +X% damage (scope: K4a; X: K13) | `05`, `02` (`FDwSynergyRules`, multiplier in `FDwDamageRules`), `13` |

### UI and shell (answered 2026-10-25)
| ID | Question | Decision | Now in |
|---|---|---|---|
| U1 | Game speed | a) 1x / 1.5x / 2x setting (presentation only) | `16` (`EDwGameSpeed`) |
| U2 | Camera | a) Fixed angle, small pan/zoom | `03` (`ADwCameraRig`) |
| U3 | Settings list | Resolution, window mode, vsync, 4 volumes, game speed, screen shake + colourblind-friendly highlights (must ship; palettes: U3a) | `16`, `14`, `04` (`UDwHighlightPalette`) |
| U4 | Run summary | Captain, seed, nodes, fights by outcome, gold, crew lost, kills, time | `16`, `09` (`FDwRunStats`, end reason) |
| U5 | Abandon goes to summary | a) Yes | `16` |

### Answered 2026-11-10 (DW-201, DW-203)
| ID | Question | Decision | Now in |
|---|---|---|---|
| K7 | Veteran Crew: "+50% XP and levels up twice as fast" | a) Only +50% XP (`XpMultiplier` 1.5, `LevelThresholdMultiplier` 1.0). The threshold multiplier still works if set, for balance passes | `12`, `Tuning/CaptainTuning.json`, `FDwCaptainRules` |
| P6 | Plunder: 1.5x gold plus an item always, or a chance? | b) 1.5x gold + always 1 random item from `DA_Shop_Default`'s item list (`PlunderItemChance` 1.0); surrender pays the same | `10`, `Tuning/EconomyTuning.json` |
| P11 | Starting gold | 100 | `Tuning/EconomyTuning.json`, `Docs/Balance/Placeholders.md` |
