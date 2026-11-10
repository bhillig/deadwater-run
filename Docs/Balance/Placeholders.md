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
