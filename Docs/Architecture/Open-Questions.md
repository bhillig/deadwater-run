# Open Questions for Brandon

- Every creative or design call the architecture needs. Agents never pick; they build the recommended default behind a data flag, clearly marked as a placeholder, until you answer.
- **Where it lives** = the data field that holds your answer. Almost all answers are data, not code. Questions marked **(code)** change behaviour that data can't express.
- **Needed by** follows the critical path in `Roadmap.md`.
- Answered questions move to **Resolved** at the bottom with the decision and the docs that now carry it. Last update: 2026-11-17 (B1-B8 answered for DW-301 and N1 + P2 for DW-304: recommended defaults, N1 = the balance-designer's Set A, crew only; earlier that day: C10a, K1a, K2a, K4a, K5, K6, K8-K12 answered for DW-205..DW-208: recommended defaults, K5 with reduced redirected damage; 2026-11-11: P4, P7-P10, C9a and the DW-204 price questions answered; P12-P18 raised by DW-204. 2026-11-10: K7, P6, P11, M1-M6 answered during DW-201..DW-211; earlier: C1-C15, K1-K4, U1-U5 from `AnswersToOpenQuestions.md`).

## Top open questions (they shape the most work)
1. **C8a** - Explosive barrels: new in your C8 answer, not in the design doc or Scope. In v1 or post-launch?
2. **N2-N3** - the 4-5 enemy crews and where they appear (DW-305 is waiting on them).
3. **E1** - events: one result text per gamble branch, so a gamble shows what actually happened (the critic's top finding on the drafts).

## Follow-ups from the 2026-10-25 answers

| ID | From | Question | Options | Recommendation | Where it lives | Needed by |
|---|---|---|---|---|---|---|
| C5a | C5 | Your answer listed "ladder tile, climb cost 2, 1 unit, melee can't target the nest" but left out option a's "+2 range from it". Is there a range bonus from the nest? And can a unit in the nest use melee on the deck? | a) +2 range from the nest; melee blocked both ways (can't hit into or out of the nest) b) No range bonus (nest = LOS over crates + melee protection); melee blocked both ways c) +2 range; a nest unit can melee the ladder tile | a) - the nest is the sniper's spot; built as placeholder now | `DT_CombatTuning.RangeBonusPerElevation`, melee `bCanTargetElevated` | Oct 27 |
| C6a | C6 | Do units block line of sight? | a) No, only masts/cabins and crates/rails b) Enemy units block c) All units block | a) - easiest to read; placeholder now | `FDwLineOfSight` **(code)** | Oct 27 |
| C8a | C8 | Explosive barrels are new: not in `DeadwaterRun.md` and not in Scope (Scope rule: new ideas go on the post-launch list). In v1? | a) Post-launch; v1 ship damage = cannons + ship-targeting abilities only b) v1: barrels are 1-HP neutral prop units placed in deck layouts; destroying one deals X to that ship and Y to adjacent units (needs numbers, how many per ship, whether either side can shoot them, whether it counts as a kill, VFX/SFX, AI awareness - about 3-4 agent-days across gameplay, AI, art) c) v1, ship damage only, no splash | a) - protects Dec 3; b) fits the architecture cleanly if you want it (no new systems, just a unit definition + a death effect) | Scope; if b: `DA_Unit_ExplosiveBarrel`, deck layout slots, `DT_UnitTuning` | Nov 2 |
| C10b | C10 | Should enemies hunt the captain? | a) Every crew has a moderate captain-pressure weight b) Only some crews (e.g. an "assassin" crew) hunt the captain; others ignore him c) No captain preference | b) - a personality you can see; ties to N2 | `UDwAiProfile` captain-pressure weight | Nov 8 |
| C13a | C13 | With no undo, how is a move confirmed? | a) Hover shows the path, one click on a highlighted tile moves b) First click previews, second click confirms c) a) with a setting for b) | a) - keeps the pace; the hover path is the preview | `UDwCombatInteraction` | Oct 27 |
| U3a | U3 | Colourblind-friendly highlights: one palette or several? | a) One colourblind-safe palette, plus distinct patterns per highlight type in both palettes b) Three palettes (deuteranopia, protanopia, tritanopia) | a) | `DA_HighlightPalette_*`, `UDwGameUserSettings.HighlightPalette` | Nov 2 |

## Combat and board (needed by Oct 27)

All answered - see Resolved. Follow-ups C5a, C6a, C8a, C10a, C10b, C13a above.

## Captains (needed by Nov 2; K1 by Oct 27)

K1-K4 answered - see Resolved. Follow-ups K1a, K2a, K4a above.

| ID | Question | Options | Recommendation | Where it lives |
|---|---|---|---|---|
| K13 | MP costs and damage for all 6 captain abilities; Conqueror base stats; Team Leader synergy X% | Balance Designer proposes 2-3 sets from simulations | Tuning | `DT_AbilityTuning`, `DT_UnitTuning`, `DT_CaptainTuning.SynergyDamageMultiplier` |
| K14 | Same starting crew (Swordsman + Sniper) for every captain? Captain stats besides the Conqueror? | a) Same crew; Team Leader and Fearmongerer captains use base crew stats b) Different crews per captain | a) | `StartingCrew`, unit rows |

## Progression and economy (needed by Nov 2)

| ID | Question | Options | Recommendation | Where it lives |
|---|---|---|---|---|
| P1 | XP: kill XP, clear XP, curve, max level (and kill XP for a Kraken tentacle) | Balance Designer proposes; target ~1 level per encounter | Tuning | `DT_ProgressionTuning`, `XpOnKill`, `ClearXp`, `MaxLevel` |
| P3 | Level-ups mid-fight or after? | a) After the fight b) Immediately on the kill | a) | Flow (code is the same) |
| P5 | "A new move" on victory: how? | a) Choose 1 of 3 abilities and who learns it b) Random ability to a random crew member c) Skip allowed for gold | a) | `RewardAbilityChoices` |
| P12 | Repair when less than `RepairAmount` hull is missing (DW-204) | a) Full price, hull fills to max b) Pay only for what is fixed (rounded up) c) Refused until a full `RepairAmount` is missing | a) - built as the placeholder; simplest to read | `FDwShopRules::ApplyPurchase` **(code if b or c)** |
| P13 | How often can a shop repair / sell items in one visit? (DW-204) | a) Any number of times, until the hull is full / the hold is full (`MaxInventory`) b) Once each per visit c) A per-shop stock (e.g. 1 repair, 2 of each item) | a) - built as the placeholder | `FDwShopRules::SellsOnce` **(code if b)**; stock counts in `DT_EconomyTuning` if c |
| P14 | Does "+50 max hull" also add 50 to the current hull? (DW-204) | a) Yes, the new planks arrive intact b) Max only; repair to fill it | a) - built as the placeholder | `FDwShopRules::ApplyPurchase` **(code if b)** |
| P15 | Plunder with a full hold (6 items, DW-204) | a) Left behind; the rewards screen shows it (`FDwRewardBundle::ItemsNoRoom`) b) Swap: drop a carried item to take it c) Sold on the spot for gold | a) - built as the placeholder; b) needs a rewards-screen choice | `FDwRewardRules`, `UDwRun::ClaimRewards` **(code if b or c)** |
| P16 | "Can't disband the last crew member": may the captain sail alone? (DW-204) | a) No: the captain plus at least one crew member always stay b) Yes: everyone but the captain can be disbanded | a) - built as the placeholder (safest) | `FDwShopRules::CanPurchase` (`CantDisband`) **(code if b)** |
| P17 | Recruit level "crew average - 1": does the captain count in the average? (DW-204) | a) Yes, every living crew member, captain included b) Crew only | a) - built as the placeholder | `FDwShopRules::GetRecruitLevel` **(code if b)** |
| P18 | Ship upgrades: price, how many times, names and the third cannon's spot (DW-204) | a) 150 gold each, once per run; placeholder names Reinforced Hull / Heavy Shot / Third Cannon; slot at row 0 beside the first cannon b) Tiered prices (e.g. 100 / 150 / 200) c) Hull and cannon damage stack (buyable again), third cannon once | a) - built as the placeholder; names and the deck spot are yours (art pass) | `Tuning/ShipUpgradeTuning.json`, `DA_ShipUpgrade_*`, `DA_Deck_PlayerShip` |
| P8 | Recruit/disband: cost, which archetypes, recruit level, refund | a) Swordsman/Sniper (+ any new archetypes), cost ~80, level = crew average - 1, no refund b) Random archetype | Tuning + N5 | `DT_UnitTuning.RecruitCost`, shop recruit pool |

## Content (needed by Nov 8 - week 3 is content week)

| ID | Question | Options | Recommendation | Where it lives |
|---|---|---|---|---|
| N2 | 4-5 enemy crews: names, units, ship layouts, personalities | e.g. gunners / boarders / snipers / mixed / elite (see C10b for a captain-hunting crew) | Drafts on request | `DA_Encounter_*`, `DA_AiProfile_*` |
| N3 | Difficulty by depth: which crews appear where | a) Tier 1 layers 1-3, tier 2 layers 4-6, tier 3 layers 7-9 b) Any crew anywhere, scaled by level | a) | Encounter `MinLayer`/`MaxLayer` |
| N4 | The 10-12 events (text, choices, outcomes) | Use the event format in `Editor-Tools.md` | - | `DA_Event_*` |
| E1 | A gamble (`RandomOf`) has one result text for all its branches, so the text can't say which way it went (the critic flagged it on several drafts) | a) Optional `ResultText` per branch, shown after the choice's text b) Keep one text; writers stay branch-neutral and the change lines say what happened | a) - small code change, much better writing | `FDwEventRandomOption` **(code)** |
| N5 | Any recruit archetypes beyond Swordsman and Sniper? | a) None in v1 b) 1-2 more (e.g. gunner, surgeon) | Your call; each adds abilities and art | Unit definitions |
| N6 | Crew names: fixed, random from a list, or none? | a) Random from a name list b) Archetype names only | a) adds attachment to permadeath | Name list DataAsset |

## Map (needed by Nov 2)

M1-M6 answered 2026-11-10 (recommended defaults) - see Resolved.

## Kraken (needed by Nov 8)

B1-B8 answered 2026-11-17 (recommended defaults, DW-301); B9 = K10 - see Resolved.

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

### Answered 2026-11-10 (DW-201, DW-203, DW-202)
| ID | Question | Decision | Now in |
|---|---|---|---|
| K7 | Veteran Crew: "+50% XP and levels up twice as fast" | a) Only +50% XP (`XpMultiplier` 1.5, `LevelThresholdMultiplier` 1.0). The threshold multiplier still works if set, for balance passes | `12`, `Tuning/CaptainTuning.json`, `FDwCaptainRules` |
| P6 | Plunder: 1.5x gold plus an item always, or a chance? | b) 1.5x gold + always 1 random item from `DA_Shop_Default`'s item list (`PlunderItemChance` 1.0); surrender pays the same | `10`, `Tuning/EconomyTuning.json` |
| P11 | Starting gold | 100 | `Tuning/EconomyTuning.json`, `Docs/Balance/Placeholders.md` |
| M1 | 8 or 9 nodes | a) Picked per seed (`MinPathNodes` 8, `MaxPathNodes` 9) | `09`, `DA_MapRules` |
| M2 | "5 fights guaranteed" | a) At least 5 on every path (`MinFightsPerPath`; the generator forces fights, the validator proves it) | `09`, `DA_MapRules` |
| M3 | First node; map width | a) First layer fights, 3 lanes | `09`, `DA_MapRules` |
| M4 | Event nodes on the map | a) "?" only | `14` (map screen) |
| M5 | Shop before the boss | a) The middle node of the last layer is always a shop, offered from every node before it | `09`, `DA_MapRules.bShopOptionBeforeBoss` |
| M6 | Fight info on the map | a) Icon only | `14` (map screen) |

### Answered 2026-11-11 (DW-204 shop)
| ID | Question | Decision | Now in |
|---|---|---|---|
| P4 | Ability slots per crew member; can abilities be replaced? | a) 3 slots + basic attack; when full, buying (or claiming) an ability replaces one (`MaxAbilitySlots` 3) | `10`, `12`, `Tuning/RunTuning.json`, `FDwShopRules` (`NoSlot`) |
| P7 | Repair: 100 gold for how much hull? | a) +100 hull (`RepairCost` 100, `RepairAmount` 100), clamped at max | `06`, `10`, `Tuning/EconomyTuning.json` |
| P8 | Recruit/disband: cost, archetypes, level, refund | a) Swordsman / Sniper (+ any new archetypes) for 80 (`DT_UnitTuning.RecruitCost`); level = max(1, floor(crew average) + `RecruitLevelOffset` -1); disband gives no refund (`DisbandRefund` 0) | `10`, `Tuning/UnitTuning.json`, `Tuning/EconomyTuning.json`, `DA_Shop_Default.RecruitPool` |
| P9 | Ship upgrades list and prices | a) +50 max hull, +20 cannon damage, third cannon; numbers in the new `DT_ShipUpgradeTuning` (`Price`, `MaxHealthBonus`, `CannonDamageBonus`) | `06`, `10`, `13`, `Tuning/ShipUpgradeTuning.json`, `DA_ShipUpgrade_*` |
| P10 | Items: amounts, targets, inventory, cap | a) Shared ship inventory, 6 items in total (`DT_EconomyTuning.MaxInventory`, shop and plunder both respect it), self or an adjacent ally, +40 HP / +10 MP | `10`, `Tuning/ItemTuning.json`, `Tuning/EconomyTuning.json` |
| C9a | Can items be used outside combat? | a) Combat only | `10` |
| DW-204 a | Where does a price live? (economy row vs. each thing's row) | Per row only: `DT_AbilityTuning.ShopPrice` (50), `DT_ItemTuning.Price` (30), `DT_UnitTuning.RecruitCost` (80). `AbilityUnlockCost`, `ItemCost`, `RecruitCostDefault` deleted from the economy row. Repair keeps `DT_EconomyTuning.RepairCost` (no row of its own) | `10`, `13` |
| DW-204 b | Where does a ship upgrade's price live? | A new `DT_ShipUpgradeTuning` (`FDwShipUpgradeTuningRow`: `Price` and the effect amounts), synced from `Tuning/ShipUpgradeTuning.json` by the content script; `UDwShipUpgradeDefinition` links its row | `06`, `13` |
| DW-204 c | Inventory cap: total or per stack? | 6 items in total in the shared inventory, `DT_EconomyTuning.MaxInventory`. A full hold leaves plunder behind (P15 placeholder) | `10` |
| DW-204 d | Recruit level formula | `max(1, floor(crew average level) + RecruitLevelOffset)`, `RecruitLevelOffset` -1 in `DT_EconomyTuning`; `DisbandRefund` stays 0 | `10` |


### Answered 2026-11-17 (DW-205..DW-208: events and the three captains)

| # | Question | Decision | Where it lives now |
|---|---|---|---|
| C10a | Can event outcomes kill? | a) Event Health loss floors at 1 for everyone; ship Health floors at 1 | `11`, `UDwEventOutcome_CrewHealth` / `_ShipHealth` |
| K1a | Conqueror's 2 actions: repeats? | a) Any two actions, repeats allowed; each cannon still fires once per turn | `05`, `02` |
| K2a | Conqueror and cannons | a) Yes, he can fire cannons | `05` (`BlockedAbilityTags` = firearm only) |
| K4a | Synergy scope | a) Basic attacks and damaging abilities of every crew member including the captain; any ally counts as the neighbour; not cannons | `05`, `02` (`FDwSynergyRules`) |
| K5 | Defend | a) An action: guard an adjacent ally until your next turn; single-target attacks on that ally hit the captain instead, **at reduced damage** (placeholder -50%, `DT_CaptainTuning.GuardDamageMultiplier`); AoE not redirected | `05`, `GA_Defend`, `GE_Guarded` |
| K6 | Grapple landing tile | a) Nearest free tile next to the captain along the line; works across ships and out of the nest | `05`, `UDwGA_Grapple` |
| K8 | Fear after cowering | a) Resets to 0 | `DT_CombatTuning.FearCowerPolicy` |
| K9 | Dread Reputation cap | a) Starting Fear capped at 2 | `DT_CaptainTuning.StartingFearCap` |
| K10 | Kraken immunities | a) Immune to Fear and grapple; freeze works on tentacles only | `Unit.Immune.*` tags on the Kraken units |
| K11 | Smoke and Fire / Six Pistols | a) Damage from tuning; Six Pistols: enemies in LOS, repeats allowed | `DT_AbilityTuning`, targeting |
| K12 | Charge / Eye of the Tiger | a) Charge: straight line, stops at obstacles; Eye ignores range and LOS | `UDwGA_Charge`, Eye of the Tiger BP |
| N4 (partial) | The 10-12 events | The 12 content-pipeline drafts go in as they are (`Tuning/Events.json`); the critic's open findings in `Docs/ContentPipeline/out/*/report.md` still need Brandon before DW-306 closes | `Tuning/Events.json`, `DA_Event_*` |

### Answered 2026-11-17 (DW-301 Kraken, DW-304 content)

| # | Question | Decision | Where it lives now |
|---|---|---|---|
| B1 | Tentacles: how many, HP, regrow? | a) 4 tentacles, 60 HP, no regrow | `KrakenTuning.json` `TentacleCount`; `UnitTuning.json` `KrakenTentacle.MaxHealth`; four `T` slots in `DA_Deck_KrakenArena` |
| B2 | What destroying a tentacle does | b) -10% damage per tentacle, on every Kraken hit (crew and ship) | `DA_Kraken.TentacleRules.Effect = ReduceDamage`, `KrakenTuning.json` `DamageMultiplierPerMissingTentacle`, `FDwKrakenRules` |
| B3 | Does tentacle damage hurt the body? | a) No, separate | Tentacles are separate units (the unused `bTentacleDamageHitsBody` flag was removed) |
| B4 | What can hit the body | a) Cannons and ranged; melee only hits tentacles | `Unit.Immune.Melee` + `Unit.CannonTarget` on `DA_Unit_KrakenBody`; `UDwGameplayAbility::IsUnitTargetable`, `UDwGA_FireCannon` |
| B5 | Arena | a) Player ship only, nothing to board | `DA_Deck_KrakenArena` (water, `B` anchor, `T` slots) |
| B6 | Telegraphing | a) Every intent with its target | Intent storage `UDwCombatSubsystem::DeclareIntents` (DW-301); declaring them is DW-302, showing them DW-303 |
| B7 | Phase change at 30% | a) At the start of its next turn, with a banner | `UDwCombatSubsystem::UpdateKrakenTurn`, `PhaseChanged` event (banner: DW-303) |
| B8 | Do Swipe / Arms of Fury hit the ship? | a) Crew only | `GA_Kraken_KrankySwipe` / `GA_Kraken_ArmsOfFury` targeting |
| N1 | The ~12 crew abilities | Set A "Cutlass and Musket" from `Docs/Balance/N1-AbilityDrafts.md` (balance-designer draft, numbers are placeholders for DW-401); crew only, captains don't learn them | `GA/DA_Ability_Crew_*`, `AbilityTuning.json` `Crew_*`, `DA_AbilityPool_Swordsman` / `_Sniper` (built in DW-304) |
| P2 | Perk pools | a) Shared pool + a few archetype perks, 3 choices; perk list (Brandon): shared Sea Legs, Hardened, Cutthroat, Thick Skin, Rum Ration; Swordsman Iron Hide, Brawler; Sniper Steady Hand, Deep Breath | `DA_PerkPool_Shared` (archetype perks by `RequiredUnitTags`), `PerkTuning.json` (built in DW-304) |
