# 08 - Kraken Boss

## Problem
- Final node: the Kraken, 500 HP. Players can target its tentacles to reduce its actions/damage.
- Initial phase moves: Tentacle Slam (50 to one crew member or the ship), Kranky Swipe (20 to the whole team), Tentacle Grab (one crew member can't move for a turn).
- Enraged phase below 30% HP: Arms of Fury (3-5 random hits of 40), Eyes of the Sea (two actions next turn).
- Fixed decision: its own board layout (player ship plus tentacle targets). Big moves must be telegraphed (ai-engineer rule).
- A placeholder boss is needed in week 2 before the full Kraken in week 3.

## Decisions
| Option | Verdict |
|---|---|
| **A. Kraken = units on the same systems: a body `ADwCombatUnit` (500 HP) plus N tentacle units on arena tiles; moves are `UDwGA_EffectPayload` BPs; a `UDwKrakenAiBrain` picks moves from data-defined phases and declares intents** | **Chosen.** Zero special combat code paths; cannons, LOS, statuses, UI bars all just work |
| B. A bespoke boss actor with its own turn logic | Rejected: a second combat system |
| C. A scripted sequence (fixed move order) | Rejected: no tentacle interaction, low replay value |

## Data

`UDwKrakenDefinition : UDwDefinitionBase` (Data/)
| Field | Notes |
|---|---|
| `TObjectPtr<UDwUnitDefinition> Body` | `DA_Unit_KrakenBody`: all five moves; tags `Unit.Kraken.Body`, `Unit.Immune.Fear/Grapple/Freeze` (K10), `Unit.Immune.Melee` and `Unit.CannonTarget` (B4) |
| `TObjectPtr<UDwUnitDefinition> Tentacle` | `DA_Unit_KrakenTentacle`: no moves, 60 HP (`DT_UnitTuning` KrakenTentacle); tags `Unit.Kraken.Tentacle`, `Unit.Immune.Fear/Grapple` (freeze works) |
| `TObjectPtr<UDwDeckLayout> Arena` | `DA_Deck_KrakenArena`: water, four `T` slots (`Tile.Kraken.Tentacle`) in the rail column, the `B` anchor (`Tile.Kraken.Body`) three tiles out: muskets and cannons reach it, cutlasses don't |
| `TArray<FDwKrakenPhase> Phases` | Ordered; each: `FName PhaseId`, `float EnterBelowHealthPct` (1.0, 0.3), `TArray<FDwWeightedAbility> Moves`, `FGameplayTag MusicState`, `FGameplayTag EnterCue` |
| `FDwKrakenTentacleRules` | `EDwTentacleEffect` (`ReduceActions`, `ReduceDamage`, `Both`); `ReduceDamage` (B2). Tentacle damage never reaches the body (B3) |
| `FDataTableRowHandle Tuning` | `DT_KrakenTuning`: TentacleCount (4), BaseActionsPerTurn per phase ([1, 1]), ActionsLostPerTentacle (0), DamageMultiplierPerMissingTentacle (0.1). Tentacle HP is the tentacle unit's row; no regrow (B1) |

Moves (BP subclasses of `UDwGA_EffectPayload`, numbers in `DT_AbilityTuning`):
| Move | Pattern | Effects |
|---|---|---|
| `GA_Kraken_TentacleSlam` | `SingleUnitOrShip` (player side) | `GE_Damage` 50 |
| `GA_Kraken_KrankySwipe` | `AllEnemies`: every crew member, not the ship (B8) | `GE_Damage` 20 |
| `GA_Kraken_TentacleGrab` | `SingleUnit` | `GE_Rooted` 1 turn |
| `GA_Kraken_ArmsOfFury` | `RandomEnemies`, hits 3-5 from tuning | `GE_Damage` 40 per hit |
| `GA_Kraken_EyesOfTheSea` | `Self` | `UDwGE_ExtraActionNextTurn`: +1 `MaxActionPoints` next turn (turn duration 1, tick `OwnerSideTurnEnd`; applied on its own turn, so the same-turn skip rule in `02-GAS-Foundation.md` keeps it alive for the next turn) |

## Behaviour

Rules (combat, DW-301) - the same for the player's view, the brain and the simulator:
- `FDwKrakenRules` (pure, `Combat/DwCombatRules.h`): phase = the deepest phase whose `EnterBelowHealthPct` the body's Health % is *below* (30% exactly is still initial); actions = phase `BaseActionsPerTurn` - tentacle reduction (`ReduceActions`/`Both`), minimum 1; damage multiplier = 1 - cut tentacles x `DamageMultiplierPerMissingTentacle` (`ReduceDamage`/`Both`, B2).
- `UDwCombatSubsystem` keeps `FDwKrakenSnapshot` (body, phase, the phase's moves, tentacles total/cut, damage multiplier), also in every `FDwCombatSnapshot`. At the start of the Kraken's side turn it changes phase if the body crossed a threshold (B7: never mid-turn) - executing the phase's `EnterCue` on the body and emitting `PhaseChanged` (Tag = the phase's `MusicState`, for the banner and music) - then sets the body's `MaxActionPoints` base from the rules; Eyes of the Sea's +1 comes on top as an effect.
- The body may only use its current phase's moves (`FDwCombatQueries::CanActivate`; it never moves, fires or uses items). Its hits are scaled by the damage multiplier through `GetHitDamageMultiplier`, so previews show the weakened numbers and ship hits are reduced too.
- Tentacles never act: they have no AI profile and no turn in the enemy sequence.

`UDwKrakenAiBrain : UDwAiBrain` (AI/, DW-302, done 2026-11-19) - decisions only. Chosen by data: `DA_Encounter_Kraken.AiProfile = DA_AiProfile_Kraken`, whose `BrainClass` is `UDwKrakenAiBrain` (an empty `BrainClass` = the crew brain); target weights in `AiTuning.json` row `Kraken`.
- Move choice: weighted draw from the current phase's moves (`DA_Kraken` phase weights, gated by `FDwKrakenSnapshot.PhaseMoves`) from the **AI stream** (the side controller's, seeded from the encounter seed - never the combat stream, so replays stay seed + commands). Eyes of the Sea is never drawn while its extra action is pending (`MaxActionPoints > FDwKrakenSnapshot.BaseActions`) or twice in one plan, so it can't chain double turns. A move with nobody to hit is dropped and the draw repeats.
- Targets: `FDwAiScoring::ScoreAction` with the profile weights (kills, lowest Health, a run-ending hit on the captain or a sinking hit on the ship scores as Win); a move without damage (Grab) goes for the hardest hitter not already rooted. Group moves (Swipe, Arms of Fury) carry no target. Later moves in one plan see the earlier ones' predicted hits (no two Slams on a pirate the first one kills).
- **Tile aims (B6a):** Slam and Grab have `FDwTargetingRules::bAimAtTile`: they may be aimed at a tile of the opposing deck, and the brain always aims them at the victim's tile. When the move lands, whoever stands on that tile is hit (a crew mate who stepped in included); an empty tile is a miss: the action is spent, the move plays out and `AttackMissed` (Source, Tile, Tag) is emitted. Unit aims keep working for anything else.
- Intents (B6: every intent with its target): the side controller asks `DeclareIntents` at combat start (`IDwCombatSideController::OnCombatStarted`) and after every enemy side turn, once its durations have ticked (`OnSideTurnEnded`) - so the plan has one move per action of the next turn, two after Eyes of the Sea. `UDwCombatSubsystem` stores them and emits `IntentDeclared`. On its turn the brain carries them out in order (`FDwAiTurnContext::Actions`); an intent the rules refuse (or an undeclared action) is chosen afresh. At its turn start `ShouldRedeclare` re-plans when the phase changed (B7: the old moves are no longer allowed) or the action count no longer matches.
- Tentacles never act on their own; they are targets with Health. A dead tentacle frees its slot (regrow per tuning).
- Tentacle kills don't count for the Conqueror's kill bonus (K3: `KillBonusIgnoredUnitTags = Unit.Kraken.Tentacle`). Kill XP for a tentacle is its `DT_UnitTuning.XpOnKill` row (placeholder 0 until P1).
- Captain death = defeat applies here too (C10): Tentacle Slam and Arms of Fury on the captain can end the run, so the intent display (B6) matters even more.

Arena build: `FDwBoardBuilder::BuildKrakenArena(PlayerDeck, Arena)`; encounter kind `Kraken` sets the outcome config to `VictoryBoss` on body death (tentacles don't need to die) and disables the enemy-ship rules.

## Placeholder boss (week 2, DW-209)
- `DA_Encounter_KrakenPlaceholder`: Kind `Kraken`, body with 500 HP, 0 tentacles, one phase with `GA_Kraken_TentacleSlam` only. Lets the full run reach a boss and the summary screen before week 3. Replaced by `DA_Encounter_Kraken` / `DA_Kraken` in DW-301 (the content script deletes the placeholder assets).

## Connections
- `02-GAS-Foundation.md`: payload abilities, `GE_Rooted`, extra action GE.
- `03-Combat-Turns.md`: encounter kind, outcome `VictoryBoss`, intent storage and events.
- `04-Grid-Board.md`: arena builder, melee reach to tentacle slots next to the rail.
- `06-Ship-Cannons.md`: cannons may target the body (B4); Slam may hit the ship.
- `07-Enemy-AI.md`: brain base, scoring.
- `09-Run-Map.md`: boss node after the last layer.
- `14-UI.md`: boss health bar, phase banner, intent widgets, tentacle bars.
- `15-Audio-VFX.md`: Kraken cues, enraged music state.

## Build order
1. DW-209 (gameplay-engineer): placeholder Kraken encounter.
2. DW-301 (gameplay-engineer, done 2026-11-17): definition, arena layout, body and tentacle units, five move BPs, intent storage in the combat subsystem, and the Kraken rules (phase change, action count, tentacle damage scaling, phase move gate) - moved here from DW-302 because they are combat rules, not decisions.
3. DW-302 (done 2026-11-19): `UDwKrakenAiBrain` and `DA_AiProfile_Kraken`: weighted move pick per phase, targets, tile aims for Slam and Grab (B6a), declaring intents for its next turn, re-planning on a phase change.
4. DW-303 (ui-engineer): boss HUD and intents.
5. DW-307 (gameplay-engineer): Synty-based Kraken visuals and cues (art source via asset-scraper if Synty lacks a Kraken).

## How it's tested
- `DeadwaterRun.Run.Kraken` (DW-301, 24 tests): arena layout and validation; the content (valid, numbers, tags, phases); setup placement; `FDwKrakenRules` (phase at 100%, 31%, 30%, 29%; actions with 0..4 cut, never below 1; damage multiplier); B4 targeting (cutlass refused at the body, accepted at a tentacle; musket and flat cannon at the body); each move (Slam 50 crew or ship, Swipe 20 crew only, Grab roots one player turn, Arms of Fury 3-5 x 40 over 12 seeds, Eyes of the Sea 1-2-1 actions); cut tentacles weaken hits and previews; the phase gate and the B7 timing with its event; intents; body death -> `VictoryBoss` with tentacles alive; the run's boss node.
- `DeadwaterRun.AI.Kraken` (DW-302, 15 tests): content wiring (profile, brain class, tile-aimed Slam and Grab); one telegraphed initial-phase intent before turn 1; same intents per seed, every initial move drawn and no enraged move over 30 seeds; carries out the shown move then declares the next; a slammed or grabbed tile the victim left is a miss (`AttackMissed`, no damage, action spent) and whoever steps in is hit; two intents after Eyes of the Sea, none of them Eyes, then one; re-plans with enraged moves on the phase change; ten rounds with no refused command; targets (the killable pirate, the captain over a cheaper kill, the ship when the hit sinks it, Grab on the hardest unrooted hitter, no double Slam on a doomed pirate).
- Proof: `Tools/Capture.ps1 -Demo boss` - sniper aiming at the body, swordsman aiming at a tentacle, the initial-phase turn, the enraged turn, Victory.
- Simulator: Kraken win rate per captain and average rounds reported (balance target set by Brandon).

## What the player sees
- The ship alone on dark water, tentacles curling over the rail. Each turn the Kraken shows what it's about to do, so cutting a tentacle or moving out of a slam's shadow is a real decision. Under 30% the music shifts, the Kraken rages, and Arms of Fury rains down.
