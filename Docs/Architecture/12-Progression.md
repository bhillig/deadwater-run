# 12 - Progression: XP, Levels and Perks

## Problem
- Killing an enemy grants XP to the crew member who killed it; clearing an encounter grants XP to every crew member. Crew level up about once per encounter.
- Fixed decision: level-up = a stat increase plus a pick of 1 from 2-3 perks, from data-driven perk pools.
- Veteran Crew (Team Leader): +50% XP and levels up twice as fast (wording to confirm, Open-Questions K7).
- Crew also gain abilities (shop, loot, events) - see `10-Economy-Shop-Loot.md`.

## Decision
| Option | Verdict |
|---|---|
| **A. Pure `FDwProgressionRules` over `FDwCrewMemberState`; XP thresholds and per-level stat gains in tuning tables; perks are `UDwPerkDefinition` assets in `UDwPerkPool` assets; a perk's effect is an infinite GE (or a granted ability) applied at every fight spawn** | **Chosen.** Perks reuse GAS; no perk-specific code; pools are visible assets Brandon edits |
| B. Perks as code classes with hooks | Rejected: code per perk |
| C. Perks as stat-only numbers in a table | Rejected: can't express "grants an ability" or tag-based perks |

## Data
- `DT_ProgressionTuning`: one row per level - `XpToNext`, plus `MaxLevel` in `DT_RunTuning` (P1).
- Per-archetype stat gains live in `DT_UnitTuning` (`HealthPerLevel`, `ManaPerLevel`, `AttackPerLevel`, `MoveRangeAtLevel` if any) - "a stat increase" is automatic and shown on the level-up screen (P2).
- XP sources: `DT_UnitTuning.XpOnKill` (victim), `DT_EncounterTuning.ClearXp`.
- `UDwPerkDefinition : UDwDefinitionBase`: `FText Name`, `FText Description` (with `{Value}` tokens filled from tuning), icon, `TSubclassOf<UGameplayEffect> Effect` (infinite, asset tag `Effect.Perk`), optional `TObjectPtr<UDwAbilityDefinition> GrantedAbility`, `FGameplayTagContainer RequiredUnitTags`, `int32 MaxStacks`, `float Weight`, tuning row (`DT_PerkTuning`: `Value`, `Value2`).
- `UDwPerkPool : UDwDefinitionBase`: `TArray<TObjectPtr<UDwPerkDefinition>> Perks`; each unit definition references one pool (shared or per archetype - P2). `DT_RunTuning.PerkChoices` = 2 or 3.

## Rules (`FDwProgressionRules`, Run/, pure)
- `GrantXp(Crew&, Amount, XpMultiplier, ThresholdMultiplier, Table) -> int32 LevelsGained` - `Xp += round(Amount x XpMultiplier)`; level while `Xp >= XpToNext(Level) x ThresholdMultiplier`; cap at `MaxLevel`.
- `RollPerkChoices(Crew, Pool, Count, Rng) -> TArray<FPrimaryAssetId>` - weighted, filters `RequiredUnitTags` and `MaxStacks`, no duplicates in one offer.
- `ApplyPerk(Crew&, PerkId)` - appends to `Crew.Perks` (and `Abilities` for a granted ability).
- `GetLevelStats(UnitTuning, Level) -> FDwUnitStats` - base + gains x (Level - 1); used by spawn (`GE_InitUnitStats`) and the UI before/after preview. When `MaxHealth` grows, current `Health` grows by the same amount.
- Multipliers come from `FDwCaptainRules::GetXpMultiplier` and `GetLevelThresholdMultiplier` (Veteran Crew).

## Flow
- During combat: kills are recorded in `FDwCombatResult` (killer roster id -> victim XP); no mid-fight level-ups (recommended, P3).
- After victory (`UDwRun::ApplyCombatResult`): kill XP to each killer, clear XP to every living crew member, each level gained pushes an `FDwPendingLevelUp` (crew id, new level, stat delta, perk choices rolled now with `ForRewards` so they are stable).
- `Rewards` -> `LevelUp` screens, one per pending level-up -> `ChoosePerk(CrewId, PerkId)` -> next or `Map`.
- Events that grant XP use the same path.

```mermaid
sequenceDiagram
    participant Run as UDwRun
    participant Prog as FDwProgressionRules
    participant Cap as FDwCaptainRules
    participant UI as Level-up screen
    actor Player
    Run->>Cap: GetXpMultiplier and GetLevelThresholdMultiplier
    Run->>Prog: GrantXp for kills and clear
    Prog-->>Run: Levels gained per crew member
    Run->>Prog: RollPerkChoices(crew, pool, count, rng)
    Run->>Run: Queue FDwPendingLevelUp entries
    Run-->>UI: Phase LevelUp
    UI->>Player: Stat increase plus 2-3 perk cards
    Player->>UI: Pick a perk
    UI->>Run: ChoosePerk(CrewId, PerkId)
    Run->>Prog: ApplyPerk
```

## Connections
- `02-GAS-Foundation.md`: perk GEs, `GE_InitUnitStats`.
- `03-Combat-Turns.md`: kill credit in the result.
- `05-Crew-Captains.md`: Veteran Crew multipliers, Conqueror kill bonus is separate (a captain counter, not a perk).
- `09-Run-Map.md`, `11-Events.md`: phases and XP outcomes.
- `14-UI.md`: `UDwLevelUpViewModel`, XP bars on crew cards.
- `17-Testing-Simulator.md`: "levels per encounter" metric (target ~1).

## Build order (gameplay-engineer)
1. DW-203: tables, rules, pending level-ups, `ChoosePerk`, 3 placeholder perks in one pool.
2. DW-304: final perk pools once Brandon answers P2.
3. balance-designer DW-401: XP curve so the simulator shows ~1 level per encounter.

## How it's tested (`DeadwaterRun.Rules.Progression.*`, pure)
- XP exactly at threshold levels up; multi-level gains queue multiple level-ups; max level caps.
- Veteran Crew multipliers change levels as configured (both readings of K7 covered by table tests).
- Perk rolls: correct count, no duplicates, respect tags and stacks, stable per seed.
- Simulator: mean levels per encounter per captain.

## What the player sees
- After a fight the crew's XP bars fill; the sniper levels up, gains +HP, and the player picks one of three perk cards - the crew they lose in the next fight is a crew they built.
