# DW-401 - Simulator balance pass

Balance Designer, 2026-11-24. Branch `balance-designer/dw-401-balance`, cut from master `4474b84`. Inputs: Docs/Architecture/13 (data layout) and 17 (simulator), `context/ideal-customer-profile.md` (medium difficulty, ~30 min runs).

Only tuning values in `Tuning/*.json` (and the `DT_*Tuning` tables imported from them) changed. No rules or game code changed. Automation tests in six files pinned old numbers; they now read them from data, or (for the design-number guards) pin the new values (see "Tests"). Automation: **484/484 pass**.

## Headline

| Metric (3000 runs, seeds 1-1000 x 3 captains, `DA_SimPolicy_Default`) | Before (master) | After (this branch) | Target |
|---|---|---|---|
| Win rate TeamLeader / Fearmongerer / Conqueror | 22.0% / 18.1% / 9.8% | **60.4% / 49.7% / 50.1%** | similar per captain |
| Spread between captains | 12.2 pts | 10.7 pts (Fearmongerer and Conqueror within 0.4) | small |
| Estimated minutes, all runs | 8.3 | **28.2** | ~30 |
| Estimated minutes, winning runs (TL / F / C) | 13.6 / 13.9 / 13.1 | **33.0 / 29.8 / 30.7** | ~30 |
| Rounds per fight | 1.9 | 5.9 | - |
| Won ship fights ended by sinking / plunder / surrender | 99.6% / 0.1% / 0.3% | 82.4% / 11.5% / 6.1% | real choice |
| Runs that reach the Kraken | 17.1% | 76.3% | - |
| Kraken win rate once reached | 97.1% | 70.0% | real final test |
| Hull lost per won ship fight | 75.8 | 29.3 | - |
| Shop visits per run | 0.88 | 1.35 | - |
| Stuck runs | 0 | **19 (0.6%) -> `SIM FAIL`** | 0 (see R1) |

Raw data: `Saved/Sim/DW401/base.json` (before), `Saved/Sim/DW401/locked.json` (after). Not committed (Saved/ is local).

## How the simulator skews these numbers

- The sim player is `DA_SimPolicy_Default` playing the crew with `DA_AiProfile_Default`, the enemies' own utility brain (DW-214's `DA_AiProfile_SimPlayer` doesn't exist yet). It is greedy and short-sighted:
  - It never plans a plunder. Plunders only happen when the crew happens to kill everyone before the hull is gone.
  - It never Defends or Grapples for position on purpose, and spends MP as soon as a cast scores well.
  - Its route is random, apart from a 3x pull towards shops when the hull is low, so it sees only 1.35 shops per run.
  - It always repairs to 70% first, then recruits, then buys upgrades, abilities and items until the gold runs out.
- A real player should win more often than these numbers, and the Team Leader probably gains least from smart play. So 50-60% sim wins likely means 60-75% for a medium player: the easy side of "medium". Option 2 below gives the difficulty dial.
- Conqueror numbers depend most on the AI. His crew make one choice per turn (move or act), and the brain often spends that choice walking instead of firing.

## How "estimated minutes" is computed, and is 30 reachable

- `UDwRunDriver`: minutes = (player actions x 8 s + enemy actions x 2 s + visited nodes x 30 s) / 60. The seconds come from `DA_SimPolicy_Default` (`SecondsPerAction`, `SecondsPerEnemyAction`, `SecondsPerNode`). They are first guesses, uncalibrated until DW-404 playtests. A "player action" is one unit's command.
- Winning run before: 60 player actions, 25 enemy actions, ~10.6 nodes, about 13.6 min.
- Winning run after: 177 player actions, 90 enemy actions, ~10.5 nodes, about 31.8 min.
- **30 min is reachable by tuning alone.** The node count (8-9 + boss) is unchanged. The extra time comes from fights lasting ~6 rounds instead of 2.
- Caveat: if DW-404 measures ~5 s per action instead of 8, the same runs come out ~22 min. Then fights would need to be longer again (tuning), or the chart longer (a map rule, gameplay-engineer).

## Where runs die

| | Before | After |
|---|---|---|
| End reason (defeats) | ShipSunk 100% | ShipSunk 43%, CaptainDead 37%, CrewDead 20% |
| Defeats by layer 0..9 (9 = boss) | 0 356 218 436 493 448 339 180 23 8 | 3 31 47 21 55 126 259 127 430 280 |
| Defeats by encounter | Cutthroats 717, Powder Monkeys 697, Sharpshooters 460, Dread Crew 316, Deckhands 296, Kraken 15 | **Kraken 686**, Dread Crew 260, Cutthroats 229, Deckhands 75, Sharpshooters 73, Powder Monkeys 56 |

| Ship fight, after: loss % / rounds / hull lost / sunk share of wins | Loss | Rounds | Hull lost | Sunk |
|---|---|---|---|---|
| Deckhands (tier 1) | 2.4% | 5.3 | 4.5 | 66% |
| Powder Monkeys (tier 1-2) | 1.2% | 5.8 | 46.7 | 96% |
| Sharpshooters (tier 2) | 3.1% | 5.7 | 35.8 | 96% |
| Cutthroats (tier 2-3) | 5.6% | 5.3 | 22.1 | 78% |
| Dread Crew (tier 3) | 10.6% | 4.8 | 30.9 | 68% |

- **Per captain (after):**
  - Team Leader dies at the Kraken (250 of 396 defeats).
  - Fearmongerer dies at the Kraken (334 of 503). Fear does nothing to it, so he wins only 59.8% of boss fights.
  - Conqueror dies on the voyage, by sinking (368 of 480): only 60% of his runs reach the boss, but he wins 83% there.
- **How ship fights end, by captain:**
  - Team Leader: 97% sunk.
  - Fearmongerer: 75% sunk, 17.5% surrender.
  - Conqueror: 65% sunk, 28% plunder. Each captain now wins in his own way.
- **Kraken phase:** the report has no phase data (no field for phase reached or body HP at the end). Kraken fights last 9.2 rounds and the boss now kills 30% of the crews that reach it. Phase data is recommendation R6.

## Gold, shops and heal-vs-upgrade

| | Before | After |
|---|---|---|
| Gold earned / spent per run | 597 / 301 | 770 / 591 |
| Gold on shop arrival: mid-run / pre-boss | 309 / 580 | 301 / 575 |
| Buys per shop visit: repair / recruit / upgrade / ability / item | 1.01 / 0.63 / 0.82 / 0.88 / 0.85 | 0.98 / 0.84 / 0.74 / 1.24 / 1.06 |
| Gold left when leaving a shop | 18 | 11 |
| Hurt visits (<70% hull) where full repair + the remaining upgrades cost more than the purse ("must choose") | 87.6% | 96.9% |
| Visits where the repair-first policy's repairs cost it an upgrade it could otherwise have bought | 15.7% | 5.6% |

- **Is heal-vs-upgrade a real choice? Yes, for a person.**
  - At 97% of hurt visits the player can't have everything: 100 gold buys 60 hull (was 100), and upgrades cost 200 / 250 / 300 (was 150 each).
  - Mid-run, ~300 gold means one repair and one upgrade, or two upgrades and no repair.
- **The sim barely feels it.** It always repairs only to 70%, which leaves gold for an upgrade. So the "repairs cost an upgrade" figure (5.6%) shows the policy's habit, not the room for decisions. The "before" 15.7% came from runs that were dying with empty pockets.
- **Gold pile-up before the boss:**
  - Before, tier gold scaled x1.6 and arrived ~700 at the pre-boss shop, enough for everything.
  - Now it's flatter (110-140 per fight). The pre-boss shop sees ~575, which buys one more upgrade plus repairs, not all three.
- Plunder (1.5x loot) and surrender (1.5x) now happen in 17.6% of won fights, so the gold reward for a riskier win is real.

## Every value changed

| File / row / field | Old -> new | Why | Other systems it touches |
|---|---|---|---|
| `ShipTuning` `EnemyPlaceholder` `MaxHealth` | 250 -> 700 | Two 70-damage cannons need 5 rounds, not 2. Boarding, abilities, Fear and plunder get time to matter. Fights go from 1.9 to 5.9 rounds, which accounts for most of the 8 -> 28 min | Every enemy crew shares this row. AI `SinkingRisk` / `ShipLowHull` scoring. Kills and XP per fight (more crew kills before the sink: levels per run 9.5 -> 14.4). Plunder and surrender loot (more fights end that way). Run time. Sim stalls (R1) |
| `ShipTuning` `EnemyPlaceholder` `CannonDamage` | 70 -> 10 | Longer fights at 70 would sink the player in 2 fights. At 10, hull loss per won fight is 29 (was 76) | Repair demand and shop gold. Powder Monkeys' identity: they still do the most hull damage (47 per fight). HeavyShot / ReinforcedHull value. Conqueror parity (his slower fights cost hull) |
| `AbilityTuning` captain `ManaCost`: Rally Cry / Grapple / Smoke and Fire / Six Pistols / Charge / Eye of the Tiger | 0 -> 8 / 4 / 7 / 6 / 3 / 5 | K13 asks for MP costs. Free casts plus longer fights let Smoke and Fire chain round-1 surrenders (Fearmongerer 98% wins, 11-min runs). 7 MP = two casts per 20 MP fight | AI `ManaCost` weight. Mana Restore item value (30 g, +10 MP). Crew ability MP budget (shared pool). Fear and surrender rate. UI cost labels |
| `AbilityTuning` `Captain_Charge` `Damage` / `DamageScale` | 40 / 0 -> 60 / 1.0 | K13: Conqueror's signature hit (85 at level 1) | Conqueror kill bonus (more captain kills). Kraken tentacles (melee only, B4) |
| `AbilityTuning` `Attack_Sword` `DamageScale` | 0 -> 1.0 | Adds the attacker's `AttackDamage` to the cutlass. Only the Conqueror has any, so every other sword still hits for 25 | Any future unit given `AttackDamage` will hit harder with a cutlass. Previews and tooltips use the same formula |
| `UnitTuning` `Captain_Conqueror` `MaxHealth` / `HealthPerLevel` | 100 / 10 -> 200 / 20 | **Design number** ("crew of 100 HP"); K13 "Conqueror base stats". At 100 HP he died charging Deckhands in 37% of first fights | Captain death = defeat (C10). Cutthroats' captain hunting. Kraken target scoring. HUD bars |
| `UnitTuning` `Captain_Conqueror` `AttackDamage` / `AttackDamagePerLevel` | 0 / 0 -> 25 / 2 | Cutlass 50 at level 1. Makes up for his crew's one-choice turns, which cost him ~1 extra round and ~20 hull per fight | **Kraken Tentacle Grab targets the highest `AttackDamage`** (`UDwKrakenAiBrain`), so the Kraken now always grabs the Conqueror. AI danger estimates. Level-up gains ("+2 attack" shown on level-up) |
| `UnitTuning` `KrakenBody` `MaxHealth` | 500 -> 1600 | **Design number.** Cannons hit the body for flat `CannonDamage` (B4): 2-3 cannons at 70-90 took 500 in ~2 rounds (97% boss wins). 1600 gives a 9-round fight and a 70% boss win rate | Boss bar. Phase threshold (30% = 480). Six Pistols and musket value at the boss. Run length (+~4 min). A stopgap for R4 |
| `EncounterTuning` `BaseGold` Deckhands / Powder Monkeys / Sharpshooters / Cutthroats / Dread Crew | 150 / 170 / 195 / 215 / 240 -> 110 / 120 / 125 / 130 / 140 | Late-run gold piled up (~700 at the pre-boss shop). Flatter tiers (x1.0 to x1.27) and less total gold make the shop choose. The design's "e.g. 150 gold" was an example | Plunder / surrender loot (1.5x of this). Shop affordability. Event gold outcomes (fixed numbers in events, so now relatively bigger) |
| `ShipUpgradeTuning` `Price` Reinforced Hull / Heavy Shot / Extra Cannon | 150 / 150 / 150 -> 200 / 250 / 300 | P18 b) tiered. The third cannon is +50% ship damage and was the best buy at any price | Shop UI. Kraken fight (cannons are the main body damage). Run length |
| `EconomyTuning` `RepairAmount` | 100 -> 60 | P7 placeholder. 100 gold now buys 60 hull, so repair competes with upgrades (`RepairCost` stays at the design's 100) | Shop UI text ("+60 hull"). Repair-first sim policy. Events that restore hull are relatively stronger |

Unchanged on purpose:
- Player ship 250 HP / 70 damage, repair cost 100, item and ability prices, Kraken move damage, 60 HP tentacles (B1), -10% damage per cut tentacle (B2).
- TL synergy 1.2: setting it to 1.0 changed TL's win rate by under 1 point.
- `StartingFearCap` 2 (K9), crew abilities, perks, AI weights.

**Not tunable from data:**
- Enemy levels 1 / 2 / 3: these are DW-401's to tune, but they are hard-coded in C++ (`UDwEnemyCrewContentLibrary::Crews`). I didn't change them; see R3.

## Judgment calls (options for Brandon)

1. **Team Leader leads by ~10 pts (60 vs 50).** His edge is Veteran Crew (+50% XP, more levels and perks). Synergy and Rally cost barely move it.
   - a) Accept: a real player gains more from smart Defend and Grapple play with the other two captains.
   - b) Veteran Crew +50% -> +25% XP (`XpMultiplier` 1.25; design number).
   - c) Rally Cry heal 15 -> 10 (design number) and MP 8 -> 10.
2. **Difficulty: 53% sim wins, probably 60-75% for a real player.**
   - a) Keep (recommended until DW-404 playtests).
   - b) Enemy cannon 10 -> 11: about -4 pts, and it hurts the Conqueror most.
   - c) Kraken body 1600 -> 1800: about -3 pts, and it hurts TL and Fearmongerer most.
3. **How the Kraken gets tough.**
   - a) Body 1600, the committed stopgap.
   - b) Keep the design's 500 and add a rule so cannons do reduced damage to the body (R4).
   - c) `BaseActionsPerTurn` [1, 2] with body 950. Tested at 3000 runs: **45.8 / 42.9 / 46.2%**, the best parity I found, 25.3 min. It's not committed because an enraged Kraken with two base actions makes Eyes of the Sea "three actions", against the design's "two actions next turn", and it fails 3 Kraken AI tests that encode that rule. Needs your ruling.
4. **Heal-vs-upgrade pressure.**
   - a) Keep `RepairAmount` 60 (a person has to choose at 97% of hurt visits).
   - b) `RepairAmount` 50 for a sharper choice: costs ~3-5 win pts.
   - c) More shops on the route (map rule) so mid-run choices happen more often than once a run.
5. **Levels per fight.** The crew total is 2.2 per fight (~0.6 per member); the design says about one per encounter.
   - a) Leave.
   - b) `ClearXp` x1.4: tested at about +8 pts win rate, mostly Fearmongerer; it would need a difficulty offset.
   - c) A flatter `XpToNext` curve.
6. **Fearmongerer surrender economy.** 7 MP Smoke and Fire means a first surrender needs a third Fear source.
   - a) Keep (surrender 17.5% of his won fights).
   - b) 6 MP, which allows three casts: tested at 97% wins, so no.
   - c) 6 MP with `StartingFearCap` 2 -> 1 (changes your K9 answer).

## Recommended rule / code changes (other agents)

- **R1 - ai-engineer + tools-engineer: fights stall once they are long.**
  - 19 of 3000 runs (0.6%, 18 of them Conqueror) stall: a fight passes 40 rounds with both sides barely acting (e.g. 12 player and 6 enemy actions in 41 rounds), so `Simulate.ps1` reports `SIM FAIL`.
  - It doesn't happen with 2-round fights, but appears in every config with 5+ round fights.
  - Repro: `Tools/Simulate.ps1 -Runs 1 -Seed 87 -Captain Conqueror` with this branch's tuning (Conqueror vs Deckhands, node 4).
  - Likely the Conqueror crew's one-choice turns plus `Danger` and `Approach` scoring leaving no action worth taking. If the enemy AI goes idle this way in the real game, a fight could drag. Fix before the RC sim (DW-503).
- **R2 - gameplay-engineer: the Conqueror's crew can't move to a cannon and fire in the same turn** (K1 single choice).
  - That costs him ~1 round and ~20 hull per fight; his attack bonus and HP are compensating for it.
  - Options: a) firing from a cannon tile you already stand on doesn't need the move; b) Conqueror crew spawn beside the cannons; c) keep the rule and the compensation.
- **R3 - gameplay-engineer: enemy `Level` is in C++** (`UDwEnemyCrewContentLibrary::Crews`).
  - Move it to data (e.g. an `EncounterTuning` field) so the balance pass can tune it, per doc 13's "numbers live in data".
- **R4 - gameplay-engineer: cannons vs the Kraken.** Flat `CannonDamage` on the body (B4) is why the body needs 1600 HP.
  - Options: a) a damage multiplier for cannons vs `Unit.CannonTarget` (e.g. x0.3), then return to 500 HP; b) only one cannon may target the body per turn; c) keep the 1600 HP.
- **R5 - gameplay-engineer: one enemy ship row for all five crews** (`EnemyPlaceholder`). Separate rows per crew would let hull and guns rise with tier (tier 3 is where the voyage still kills).
- **R6 - tools-engineer, simulator report:**
  - Add per-fight Kraken phase reached and body HP at the end, upgrade names in `bought`, and gold spent per category.
  - Add a second policy (upgrade-first, or repair to full) to bracket the heal-vs-upgrade space.
  - `levelsPerFight` counts the Kraken fight; worth splitting out.
- **R7 - ai-engineer:** Tentacle Grab picks the highest `AttackDamage`, which is now always the Conqueror. Fine as flavour; flagging the ripple.
- **R8 - qa-tester (DW-404):** calibrate `SecondsPerAction`. The 30-minute claim rests on 8 s per action.

## Tests

- `Tools/Test.ps1`: before this change 484/484. After the tuning, 16 tests failed. Of those:
  - 3 Kraken AI tests and 1 Kraken moves test failed only under option 3c (`BaseActionsPerTurn` [1, 2]), which I reverted.
  - 1 Fearmongerer test failed under Smoke and Fire costs; it was fixed in the test, because the rule under test (3 Fear = surrender) still holds.
  - The rest pinned old numbers.
- Changes, all assertions only:
  - `Tests/Run/DwKrakenSpec.cpp`, `Tests/UI/DwKrakenHudSpec.cpp`: read the body's max HP instead of 500. Phase tests set 30% / 29% / 28% of it.
  - `Tests/Combat/DwCombatContentSpec.cpp`: the enemy hull is read from its `ShipTuning` row. The sink test sets the hull to just under four shots, so the rule "the shot that takes it to 0 sinks it" is tested independently of tuning.
  - `Tests/Run/DwRunContentSpec.cpp`: the shop test wins fights until the gold covers repair + recruit + third cannon, reading prices and `RepairAmount` from tuning.
  - `Tests/Run/DwCaptainSpec.cpp`: refills MP before each Smoke and Fire in the surrender test.
  - `Tests/Data/DwDataSpec.cpp` (the "design doc numbers" guard): Conqueror 200 HP, Kraken 1600 HP, `RepairAmount` 60, each commented as DW-401 pending approval. **If you reject a design-number change, revert that line together with the JSON.**
- `Tests/Run/DwRunContentSpec.cpp` also: with the new prices, seed 7's purse only covers all three buys at the pre-boss shop, so the next fight is the Kraken. The enemy-ship cannon comparison now runs only when there is an enemy ship.
- Result after the changes: `BUILD PASS (0 warnings)`, **`TEST PASS 484/484`**. `Simulate.ps1` at 3000 runs: `SIM FAIL` only because of the 19 stuck runs (R1).
