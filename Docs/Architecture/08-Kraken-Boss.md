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
| `TObjectPtr<UDwUnitDefinition> Body` | `BP_Unit_KrakenBody`, tags `Unit.Kraken.Body`, `Unit.Immune.Fear` (pending B9) |
| `TObjectPtr<UDwUnitDefinition> Tentacle` | `BP_Unit_KrakenTentacle`, tag `Unit.Kraken.Tentacle` |
| `TObjectPtr<UDwDeckLayout> Arena` | Water tiles, `Tile.Kraken.Tentacle` slots, `Tile.Kraken.Body` anchor |
| `TArray<FDwKrakenPhase> Phases` | Ordered; each: `FName PhaseId`, `float EnterBelowHealthPct` (1.0, 0.3), `TArray<FDwWeightedAbility> Moves`, `FGameplayTag MusicState`, `FGameplayTag EnterCue` |
| `FDwKrakenTentacleRules` | `EDwTentacleEffect` (`ReduceActions`, `ReduceDamage`, `Both`), and whether tentacle damage also hits the body (B2, B3) |
| `FDataTableRowHandle Tuning` | `DT_KrakenTuning`: TentacleCount, TentacleHealth, BaseActionsPerTurn per phase, ActionsLostPerTentacle, DamageMultiplierPerMissingTentacle, regrow turns (0 = never) |

Moves (BP subclasses of `UDwGA_EffectPayload`, numbers in `DT_AbilityTuning`):
| Move | Pattern | Effects |
|---|---|---|
| `GA_Kraken_TentacleSlam` | `SingleUnitOrShip` (player side) | `GE_Damage` 50 |
| `GA_Kraken_KrankySwipe` | `AllEnemies` from the Kraken's view (ship included pending B8) | `GE_Damage` 20 |
| `GA_Kraken_TentacleGrab` | `SingleUnit` | `GE_Rooted` 1 turn |
| `GA_Kraken_ArmsOfFury` | `RandomEnemies`, hits 3-5 from tuning | `GE_Damage` 40 per hit |
| `GA_Kraken_EyesOfTheSea` | `Self` | `GE_KrakenExtraAction`: +1 `MaxActionPoints` next turn (turn duration 1, tick `OwnerSideTurnEnd`; applied on its own turn, so the same-turn skip rule in `02-GAS-Foundation.md` keeps it alive for the next turn) |

## Behaviour

`UDwKrakenAiBrain : UDwAiBrain` (AI/, ai-engineer)
- Phase = first phase whose `EnterBelowHealthPct` is above current body Health %. Phase change emits `IntentDeclared` with the phase's `EnterCue` and asks `UDwMusicSubsystem` (via the combat event) for the enraged music state.
- Actions this turn = phase `BaseActionsPerTurn` + extra (Eyes of the Sea) - tentacle reduction; minimum 1 while the body lives. Damage multiplier from missing tentacles is applied as a `GE_DamageBonus` (negative) refreshed whenever a tentacle dies.
- Move choice: weighted pick from the phase's move list using the combat random stream, then a target with `FDwAiScoring` (lowest Health, clustered crew for Swipe, ship when it's low).
- Intents: at combat start (for its first turn) and at the end of each of its turns the brain calls `DeclareIntent` for next turn's moves; `UDwCombatSubsystem` stores them and emits `IntentDeclared`. On its turn the Kraken executes stored intents, re-targeting only if the target died or left range. UI shows intents above the Kraken and danger highlights on target tiles (B6 decides how much is shown).
- Tentacles never act on their own; they are targets with Health. A dead tentacle frees its slot (regrow per tuning).
- Tentacle kills don't count for the Conqueror's kill bonus (K3: `KillBonusIgnoredUnitTags = Unit.Kraken.Tentacle`). Kill XP for a tentacle is its `DT_UnitTuning.XpOnKill` row (placeholder 0 until P1).
- Captain death = defeat applies here too (C10): Tentacle Slam and Arms of Fury on the captain can end the run, so the intent display (B6) matters even more.

Arena build: `FDwBoardBuilder::BuildKrakenArena(PlayerDeck, Arena)`; encounter kind `Kraken` sets the outcome config to `VictoryBoss` on body death (tentacles don't need to die) and disables the enemy-ship rules.

## Placeholder boss (week 2, DW-209)
- `DA_Encounter_KrakenPlaceholder`: Kind `Kraken`, body with 500 HP, 0 tentacles, one phase with `GA_Kraken_TentacleSlam` only. Lets the full run reach a boss and the summary screen before week 3.

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
2. DW-301 (gameplay-engineer): definition, arena layout, body and tentacle units, five move BPs, intent storage in the combat subsystem.
3. DW-302 (ai-engineer): `UDwKrakenAiBrain` phases, action count, tentacle scaling, intents.
4. DW-303 (ui-engineer): boss HUD and intents.
5. DW-307 (gameplay-engineer): Synty-based Kraken visuals and cues (art source via asset-scraper if Synty lacks a Kraken).

## How it's tested
- Pure (`DeadwaterRun.AI.Kraken.*`): phase selection at 100%, 31%, 30%, 29%; action count with 0..N tentacles and Eyes of the Sea; never below 1 action; weighted move pick stable per seed.
- Test world: Arms of Fury hits within 3-5 times at 40; Grab roots for exactly one player turn; killing a tentacle reduces next turn's actions or damage per the rule flag; body death -> `VictoryBoss` even with tentacles alive.
- Simulator: Kraken win rate per captain and average rounds reported (balance target set by Brandon).

## What the player sees
- The ship alone on dark water, tentacles curling over the rail. Each turn the Kraken shows what it's about to do, so cutting a tentacle or moving out of a slam's shadow is a real decision. Under 30% the music shifts, the Kraken rages, and Arms of Fury rains down.
