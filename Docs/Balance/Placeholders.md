# Placeholder tuning values

Values in `Tuning/*.json` that are **not** from `DeadwaterRun.md`. They exist so the game is playable; the Balance Designer replaces them from simulator data (DW-112, DW-401). Owner of this file: balance-designer.

## Set by DW-104 (core actions), 2026-11-09

| File | Row | Field | Placeholder | Why this number |
|---|---|---|---|---|
| `AbilityTuning.json` | `Attack_Sword` | `Damage` | 25 | 4 hits to kill a 100 HP pirate |
| `AbilityTuning.json` | `Attack_Sword` | `Range` | 1 | Melee: adjacent only |
| `AbilityTuning.json` | `Attack_Rifle` | `Damage` | 20 | 5 hits; weaker than the sword because it hits from range |
| `AbilityTuning.json` | `Attack_Rifle` | `Range` | 6 | Reaches across both decks from the back row; the nest adds +2 (C5a) |
| `ItemTuning.json` | `HealthRestore` | `Amount` | 30 | About one sword hit |
| `ItemTuning.json` | `HealthRestore` | `Range` | 1 | Self or an adjacent ally |
| `ItemTuning.json` | `ManaRestore` | `Amount` | 10 | Half of the 20 MP pool |
| `ItemTuning.json` | `ManaRestore` | `Range` | 1 | Self or an adjacent ally |
| `ShipTuning.json` | `EnemyPlaceholder` | `MaxHealth` / `CannonDamage` | 250 / 70 | Mirrors the player ship until enemy ships are designed (P9 / DW-304) |

New rows with no numbers (all zeros, structure only): `AbilityTuning.json` `Move`, `FireCannon`, `UseItem`. Move cost comes from the path, cannon damage from the firing ship's `CannonDamage`, and item amounts from `ItemTuning.json`.

## Set by DW-106 (crew AI), 2026-11-09

All of `AiTuning.json` row `Default` (used by `DA_AiProfile_Default`, the placeholder crew). Scores are in "hit points of value": one point of damage dealt to a pirate is worth 1. Terms are defined in `FDwAiWeights` (`Source/DeadwaterRun/Data/Tuning/DwAiTuningRow.h`); `DeadwaterRun.AI.Brain` checks the doc 07 behaviours with these values.

| Field | Placeholder | Why this number |
|---|---|---|
| `Weights.Damage` | 1.0 | The unit of value |
| `Weights.Kill` | 40 | A kill is worth more than the HP it removes (one fewer attacker) |
| `Weights.Win` | 1000 | Ending the fight (e.g. killing the captain, C10) beats anything else |
| `Weights.ShipDamage` | 0.25 | At full hull a cannon shot (70 -> 17.5) is worth less than a cutlass hit (25) or a musket shot (20) |
| `Weights.ShipLowHull` | 1.0 | Grows with hull already lost: at half hull a shot is worth 52, so crews fire cannons when your hull is low |
| `Weights.FriendlyFire` | 1.5 | Hurting your own side is worse than the same damage to the enemy is good |
| `Weights.Heal` | 0.8 | A heal is worth a bit less than the same damage dealt |
| `Weights.Fear` | 10 | Three Fear makes a pirate cower for a turn (MaxFear 3) |
| `Weights.LowHealthFocus` | 20 | Focus the wounded: a target at 20% Health adds 16 |
| `Weights.CaptainPressure` | 0 | No extra captain hunting until Brandon answers C10b |
| `Weights.ManaCost` | 0.5 | MP never comes back in a fight (C9) |
| `Weights.Danger` | 0.25 | A quarter of the damage the player could deal next turn: three cutlasses on a tile cost 18.75 |
| `Weights.SinkingRisk` | 60 | More than any normal attack is worth: don't board a ship your side is about to sink |
| `Weights.Approach` | 3 | Per tile short of attack range: pulls crews forward and snipers into range or the nest |
| `Weights.HighGround` | 8 | Snipers like the nest (+2 range) |
| `Weights.Boarding` | 10 | Only for profiles with `bBoarders` (none yet) |
| `Weights.CannonAffinity` | 10 | Only for profiles with `bPreferCannons` (none yet) |
| `Temperature` | 0 | Always the best move: readable and repeatable |
| `MaxMoveCandidates` | 16 | Destinations kept before danger is scored; full enemy turn plans in about 1-1.5 ms on a 12x24 board |

## Set by DW-201 (run state), 2026-11-10

| File | Row | Field | Placeholder | Why this number |
|---|---|---|---|---|
| `EconomyTuning.json` | `Default` | `StartingGold` | 100 | Brandon's pick: one repair (100g) or two abilities (50g) before the first fight's 150 base gold |

Captain abilities (`AbilityTuning.json` `Captain_*`) still have `ManaCost` 0 (K13); they are placeholder definitions with no effect until DW-206..208.

## Set by DW-203 (rewards and progression), 2026-11-10

| File | Row | Field | Placeholder | Why this number |
|---|---|---|---|---|
| `EconomyTuning.json` | `Default` | `PlunderItemChance` | 1.0 | Brandon's P6 answer: plunder always drops one item |
| `EconomyTuning.json` | `Default` | `RewardAbilityChoices` | 3 | P5 recommendation: pick 1 of 3. No learnable abilities exist yet (N1), so no choice is offered today |
| `CaptainTuning.json` | `TeamLeader` | `LevelThresholdMultiplier` | 1.0 (was 0.5) | Brandon's K7 answer: Veteran Crew = +50% XP only (`XpMultiplier` stays 1.5) |
| `EncounterTuning.json` | `Placeholder` | `ClearXp` | 80 | With 20 per kill, a crew member earns about 100 XP per fight: about one level per encounter early on (12 target) |
| `EncounterTuning.json` | `Placeholder`, `Kraken` | `bGrantsAbilityChoice` | true, false | The boss ends the run |
| `UnitTuning.json` | `Swordsman`, `Sniper` | `XpOnKill` | 20 | Three placeholder enemies = 60 kill XP per fight, shared by whoever lands the blows |
| `UnitTuning.json` | crew and captains | `HealthPerLevel` | 10 | "A stat increase" on every level-up (fixed decision); Mana and attack gains stay 0 |
| `ProgressionTuning.json` | `1`..`9` | `XpToNext` | 100, 110, ... 180 | Flat curve: about 1 level per fight early, a bit slower later; max level 10 |
| `RunTuning.json` | `Default` | `PerkChoices` / `MaxLevel` / `MaxAbilitySlots` | 3 / 10 / 3 | P2 a), P1, P4 a) recommendations |
| `PerkTuning.json` | `SeaLegs` / `Hardened` / `Cutthroat` | `Value` | 1 / 0.1 / 0.1 | +1 move tile, 10% less damage taken, 10% more damage dealt |

## Set by DW-209 (placeholder Kraken), 2026-11-11

The placeholder boss (`DA_Encounter_KrakenPlaceholder`, Docs/Architecture/08 "Placeholder boss"): the body only, no tentacles, one phase with Tentacle Slam. **Superseded by DW-301 (below)**; kept as history.

| File | Row | Field | Placeholder | Why this number |
|---|---|---|---|---|
| `UnitTuning.json` | `KrakenBody` | `MaxHealth` | 500 | The design doc's number, used unchanged; listed here because the fight it sits in is a placeholder (no tentacles, one move), so 500 has not been balanced against anything yet |
| `UnitTuning.json` | `KrakenBody` | `MaxActionPoints` | 1 (was 0) | One Tentacle Slam per Kraken turn until `UDwKrakenAiBrain` sets actions from `KrakenTuning.json` `BaseActionsPerTurn` (DW-302) |
| `AbilityTuning.json` | `Kraken_TentacleSlam` | `Damage` | 50 | Design doc number, unchanged (one crew member or the ship) |
| `KrakenTuning.json` | `Kraken` | all | 0 / empty | Unchanged: no tentacles in the placeholder; any `TentacleCount` above 0 is refused until DW-301 |
| `DA_Deck_KrakenArena` (layout, not JSON) | - | body anchor | next to the rail | So cutlasses can reach the body while there are no tentacles; what may hit the body is Open-Questions B4 |

## Set by DW-204 (shop), 2026-11-11

Brandon's P4 / P7-P10 answers (2026-11-11, "recommended defaults, all placeholders") plus the shop numbers they need. Prices now live only on each offered thing's row (Open-Questions "DW-204 a"); `AbilityUnlockCost`, `ItemCost` and `RecruitCostDefault` are gone from `EconomyTuning.json`.

| File | Row | Field | Placeholder | Why this number |
|---|---|---|---|---|
| `EconomyTuning.json` | `Default` | `RepairAmount` | 100 | P7 a): 100 gold = +100 hull (`RepairCost` stays 100, the design number) |
| `EconomyTuning.json` | `Default` | `RecruitLevelOffset` | -1 | P8 a): recruits join at the crew's average level - 1 (at least 1) |
| `EconomyTuning.json` | `Default` | `DisbandRefund` | 0 | P8 a): no refund |
| `EconomyTuning.json` | `Default` | `MaxInventory` | 6 | P10 a): 6 items in the shared hold, all kinds together |
| `EconomyTuning.json` | `Default` | `ShopAbilityOffers` / `ShopRecruitOffers` / `ShopUpgradeOffers` | 3 / 2 / 3 | Abilities like the rewards' pick of 3; both recruit archetypes; all three upgrades (each sells once per run, so later shops show fewer). Alternatives: 2 / 1 / 2 for scarcer shops, or 3 / 2 / 1 so upgrades feel rare |
| `ItemTuning.json` | `HealthRestore`, `ManaRestore` | `Price` | 30 | Design number (was `EconomyTuning.ItemCost`) |
| `ItemTuning.json` | `HealthRestore` | `Amount` | 40 (was 30) | P10 a): +40 HP |
| `UnitTuning.json` | `Swordsman`, `Sniper` | `RecruitCost` | 80 | P8 a): "~80" |
| `AbilityTuning.json` | every ability in a learnable pool | `ShopPrice` | 50 (none yet) | Design number (was `EconomyTuning.AbilityUnlockCost`). No ability is in a learnable pool until DW-304, so every row is still 0; data validation fails for a pool ability without a price, so DW-304 must set 50 on each one |
| `ShipUpgradeTuning.json` (new) | `ReinforcedHull` / `HeavyShot` / `ExtraCannon` | `Price` | 150 each | No design number (Open-Questions P18). A bit under a fight's plunder (225), so an upgrade competes with a repair plus a recruit. Alternatives: 100 / 150 / 200 tiered, or 120 flat |
| `ShipUpgradeTuning.json` (new) | `ReinforcedHull` | `MaxHealthBonus` | 50 | P9 a): +50 max hull (current hull rises too, P14) |
| `ShipUpgradeTuning.json` (new) | `HeavyShot` | `CannonDamageBonus` | 20 | P9 a): +20 cannon damage (70 -> 90) |
| `DA_Deck_PlayerShip` (layout, not JSON) | - | third cannon slot | row 0, column 1 | Next to the first cannon, reachable from the spawn column; the art pass decides (P18) |

## Set by DW-206 (Fearmongerer), 2026-11-17

| File | Row | Field | Placeholder | Why this number |
|---|---|---|---|---|
| `AbilityTuning.json` | `Captain_SmokeAndFire` | `Damage` | 20 | K11 a): damage from tuning. Hits only enemies already afraid; one cast per turn means 3 casts take a fresh crew from 0 to 3 Fear (surrender) |
| `CaptainTuning.json` | `Fearmongerer` | `StartingFearCap` | 2 | Brandon's K9 a): Dread Reputation never starts a fight at 3 (no instant surrenders) |

Smoke and Fire and Six Pistols are real now; their `ManaCost` stays 0 until K13.

## Set by DW-207 (Team Leader), 2026-11-17

| File | Row | Field | Placeholder | Why this number |
|---|---|---|---|---|
| `CaptainTuning.json` | `TeamLeader` | `SynergyDamageMultiplier` | 1.2 | K13 is open: +20% when attacking beside an ally is noticeable but doesn't double-dip with perks |
| `CaptainTuning.json` | `TeamLeader` | `GuardDamageMultiplier` | 0.5 (new field) | Brandon's K5 answer (2026-11-17): Defend redirects at reduced damage; half keeps guarding worth it while the captain's life ends the run |
| `AbilityTuning.json` | `Captain_Defend` | `Range` | 1 | Defend guards an adjacent crewmate (K5 a) |

Rally Cry (heal 15 + cleanse), Grapple Hook (range 10, LOS), Defend are real now; their `ManaCost` stays 0 until K13.

## Set by DW-208 (Conqueror), 2026-11-17

| File | Row | Field | Placeholder | Why this number |
|---|---|---|---|---|
| `AbilityTuning.json` | `Captain_Charge` | `Damage` | 40 | "Heavy damage": more than a basic swing, kills a 30 HP pirate; K13 sets the real number |

Charge and Eye of the Tiger are real now; their `ManaCost` stays 0 until K13. The Conqueror keeps the design doc's 100 HP / 20 MP ("a pirate crew of three (100 HP/20 MP)"): his strength is the second action (K1) and the kill bonus.

## Set by DW-301 (Kraken), 2026-11-17

The real Kraken (`DA_Encounter_Kraken`, Docs/Architecture/08). Tentacle count, HP and the -10% per tentacle are Brandon's B1/B2 answers; the move numbers are the design doc's. Nothing here has been balanced yet: the Kraken's win rate per captain is a DW-401 simulator target.

| File | Row | Field | Placeholder | Why this number |
|---|---|---|---|---|
| `KrakenTuning.json` | `Kraken` | `BaseActionsPerTurn` | [1, 1] | One move a turn in both phases; the design doc gives the enraged Kraken its extra actions through Eyes of the Sea, not a higher base |
| `KrakenTuning.json` | `Kraken` | `ActionsLostPerTentacle` | 0 | B2 = damage only; the field is ready if `TentacleRules.Effect` ever becomes `ReduceActions` or `Both` |
| `UnitTuning.json` | `KrakenBody` | `MaxActionPoints` | 1 | Now overwritten each Kraken turn from `BaseActionsPerTurn`; kept at 1 for the first turn's display |
| `UnitTuning.json` | `KrakenTentacle` | `XpOnKill` | 0 | P1 is open: cutting a tentacle gives no kill XP yet |
| `DA_Kraken` (data asset) | phases | move `Weight` | 1 each | Even odds until DW-302's brain uses them |
| `DA_Deck_KrakenArena` (layout) | - | body anchor | 3 tiles from the rail | Muskets (range 6) reach it from the rail and from the nest; from the back row (8 tiles) they don't, so snipers have to advance |
| Ship cannons vs the body | - | damage | 70 per cannon, flat | The ship's `CannonDamage`, unchanged: two cannons take 140 of 500 a turn. Likely too strong once balanced (DW-401) |

## Set by DW-305 (enemy crews), 2026-11-19

The five crews of N2 (`DA_Encounter_<Crew>`, `DA_AiProfile_<Crew>`, built by `UDwEnemyCrewContentLibrary`). Rosters, tiers (N3) and "only the Cutthroats hunt the captain" (C10b) are Brandon's; every number below is a placeholder for the DW-401 simulator pass. Each AI row is the `Default` row with the listed changes. `DA_Encounter_Placeholder` stays as the L_Sea dev fight with `Weight` 0, so runs never draw it.

| File | Row | Field | Placeholder | Why this number |
|---|---|---|---|---|
| `UDwEnemyCrewContentLibrary` | all crews | `MinLayer`-`MaxLayer` | Deckhands 0-2, Powder Monkeys 0-5, Sharpshooters 3-5, Cutthroats 3-99, Dread Crew 6-99 | N3 tiers on 0-based layers (design layers 1-3 / 4-6 / 7-9); the top tier is open-ended so a longer chart never has a fight without a crew |
| `UDwEnemyCrewContentLibrary` | all crews | enemy `Level` | 1 (Deckhands, Powder Monkeys), 2 (Sharpshooters, Cutthroats), 3 (Dread Crew) | One level per tier; Deckhands are "low-level", the Dread Crew "higher-level" (N2) |
| `UDwEnemyCrewContentLibrary` | all crews | `Weight` | 1 | Equal odds within a tier |
| `EncounterTuning.json` | `Deckhands` / `PowderMonkeys` / `Sharpshooters` / `Cutthroats` / `DreadCrew` | `BaseGold` | 150 / 170 / 195 / 215 / 240 | The old placeholder's 150 at tier 1, x1.3 at tier 2, x1.6 at tier 3; crews spanning two tiers sit between |
| `EncounterTuning.json` | same | `ClearXp` | 80 / 90 / 105 / 115 / 130 | Same scale from the old 80. Tier-1 values keep a fresh crew member under 100 XP from the clear alone |
| `AiTuning.json` | `Deckhands` | `Boarding` / `Approach` / `Danger` | 25 / 6 / 0.1 | Boards at once and closes in, barely caring about return fire (`bBoarders`) |
| `AiTuning.json` | `PowderMonkeys` | `CannonAffinity` / `ShipDamage` / `ShipLowHull` / `Approach` / `Danger` | 25 / 0.6 / 2 / 1 / 0.4 | Stays by its guns and shoots the hull; no wish to close in (`bPreferCannons`, not `bBoarders`) |
| `AiTuning.json` | `Sharpshooters` | `HighGround` / `Approach` / `Danger` | 20 / 1.5 / 0.5 | Values the nest well above the default 8 and keeps its distance |
| `AiTuning.json` | `Cutthroats` | `CaptainPressure` / `Approach` / `Boarding` | 1.5 / 4 / 15 | C10b: the only crew above 0. 1.5 per point of captain damage outweighs the default wounded-target focus, so it shoots a fresh captain over a scratched mate (`DeadwaterRun.AI.EnemyCrews`) |
| `AiTuning.json` | `DreadCrew` | all | the `Default` row | The balanced elite: its edge is level 3 and a fourth man, not a quirk; both `bBoarders` and `bPreferCannons` |

## Set by DW-302 (Kraken AI), 2026-11-19

The Kraken brain (`UDwKrakenAiBrain`, `DA_AiProfile_Kraken`). Its move draw uses `DA_Kraken`'s phase weights (still 1 each, above); these are its target-scoring weights.

| File | Row | Field | Placeholder | Why this number |
|---|---|---|---|---|
| `AiTuning.json` | `Kraken` | `Damage` / `Kill` / `Win` / `LowHealthFocus` | 1 / 40 / 1000 / 20 | The crew AI's defaults: finish the wounded, always take a run-ending hit |
| `AiTuning.json` | `Kraken` | `CaptainPressure` | 0.5 | A mild lean towards the captain (captain death = defeat, C10); among the crews only the Cutthroats hunt him (C10b, DW-305) |
| `AiTuning.json` | `Kraken` | `ShipDamage` / `ShipLowHull` | 0.25 / 1 | Slams the ship mostly when the hull is low; a sinking hit scores as `Win` |
| `AiTuning.json` | `Kraken` | position terms, `Temperature`, `MaxMoveCandidates` | 0 | The body never moves; target ties are drawn from the AI stream |
| `UDwKrakenAiBrain` (code rule) | - | Grab target | hardest hitter (`AttackDamage`) not already rooted | Pins the pirate who would cut tentacles fastest; no weight in `FDwAiWeights` for control yet |
| `UDwKrakenAiBrain` (code rule) | - | Eyes of the Sea | never while its extra action is pending | Otherwise it could chain double turns forever |

## Set by DW-304 (crew abilities and perks), 2026-11-17

The numbers of Set A (`Docs/Balance/N1-AbilityDrafts.md`, chosen by Brandon) and of Brandon's perk list are all placeholders for the DW-401 simulator pass.

| File | Rows | Fields | Placeholder | Why this number |
|---|---|---|---|---|
| `AbilityTuning.json` | `Crew_*` (12) | `ManaCost`, `Damage`, `Heal`, `Fear`, `Range`, hits, `DurationTurns` | Set A table | Rough rule: ~+2 damage per MP over the free basic attack; average 8.75 MP = about 2 casts per 20 MP fight |
| `AbilityTuning.json` | `Crew_*` (12) | `ShopPrice` | 50 | The design doc's "unlock new abilities (50 gold each)" |
| `PerkTuning.json` | `ThickSkin` / `RumRation` | `Value` | 20 HP / 5 MP | A fifth of a fresh pirate's Health and a quarter of its MP; 2 stacks each |
| `PerkTuning.json` | `IronHide` / `SteadyHand` | `Value` | 0.2 / 0.2 | Twice the shared Hardened / Cutthroat, as the archetype's signature pick; 1 stack each |
| `PerkTuning.json` | `Brawler` / `DeepBreath` | `Value` | 30 HP / 10 MP | One and a half / two times the shared versions |

## Set by DW-401 (simulator balance pass), 2026-11-24

From 3000-run simulator data (`Docs/Balance/DW-401-Balance.md`). These rows replace the earlier placeholders for the same fields. They are still placeholders until Brandon approves them and QA playtests (DW-404) calibrate the time estimate. Rows marked **design** change a number from `DeadwaterRun.md` and need Brandon's sign-off.

| File | Row | Field | Old -> new | Why |
|---|---|---|---|---|
| `ShipTuning.json` | `EnemyPlaceholder` | `MaxHealth` | 250 -> 700 | Two 70-damage cannons take 5 rounds to sink it instead of 2, so boarding, abilities and Fear get time to matter |
| `ShipTuning.json` | `EnemyPlaceholder` | `CannonDamage` | 70 -> 10 | Keeps hull loss per won fight at ~30 (was ~76) over the longer fights, so repairs stay a choice and not a tax |
| `AbilityTuning.json` | `Captain_RallyCry` / `GrappleHook` / `SmokeAndFire` / `SixPistols` / `Charge` / `EyeOfTheTiger` | `ManaCost` | 0 -> 8 / 4 / 7 / 6 / 3 / 5 | K13. Free abilities let Smoke and Fire chain round-1 surrenders (Fearmongerer 98% wins once fights got longer). 7 MP = two casts per 20 MP fight, so a first surrender needs a third Fear source (Boarding Cry, Dread Reputation); at 6 MP (three casts) he won 97% |
| `AbilityTuning.json` | `Captain_Charge` | `Damage` / `DamageScale` | 40 / 0 -> 60 / 1.0 | K13: Conqueror's signature hit; scales with his `AttackDamage` (85 at level 1) |
| `AbilityTuning.json` | `Attack_Sword` | `DamageScale` | 0 -> 1.0 | Lets the Conqueror's `AttackDamage` add to his cutlass. Every other unit has `AttackDamage` 0, so their swords are unchanged |
| `UnitTuning.json` | `Captain_Conqueror` | `MaxHealth` / `HealthPerLevel` | 100 / 10 -> 200 / 20 | K13 "Conqueror base stats"; **design** (crew of "100 HP"). "Significantly stronger than his crew"; at 100 HP he died charging Deckhands in 37% of first fights |
| `UnitTuning.json` | `Captain_Conqueror` | `AttackDamage` / `AttackDamagePerLevel` | 0 / 0 -> 25 / 2 | K13: cutlass 50 at level 1 (crew 25). Makes up for his crew's one-choice turns, which cost him ~1 round per fight |
| `UnitTuning.json` | `KrakenBody` | `MaxHealth` | 500 -> 1600 | **design**. Cannons hit the body for flat `CannonDamage` (B4); 500 fell in ~2 rounds with a 97% win rate. Stopgap until a rule change (report, R4) |
| `EncounterTuning.json` | `Deckhands` / `PowderMonkeys` / `Sharpshooters` / `Cutthroats` / `DreadCrew` | `BaseGold` | 150 / 170 / 195 / 215 / 240 -> 110 / 120 / 125 / 130 / 140 | Flatter tiers (x1.0 to x1.27, was x1.6): late gold piled up to ~700 at the pre-boss shop, which bought everything |
| `ShipUpgradeTuning.json` | `ReinforcedHull` / `HeavyShot` / `ExtraCannon` | `Price` | 150 / 150 / 150 -> 200 / 250 / 300 | P18 b) tiered. The third cannon is +50% ship damage and was the best buy at any price |
| `EconomyTuning.json` | `Default` | `RepairAmount` | 100 -> 60 | P7: 100 gold now buys 60 hull, so a hurt crew must choose between repairing and upgrading (`RepairCost` stays 100, the design number) |

Not changed and still placeholders: enemy `Level` 1 / 2 / 3 (set in C++ in `UDwEnemyCrewContentLibrary`, not in data; see the report's recommendations), `ClearXp`, perk values, crew ability numbers, AI weights.
