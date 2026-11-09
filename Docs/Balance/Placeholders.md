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
