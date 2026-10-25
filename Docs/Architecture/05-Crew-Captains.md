# 05 - Crew, Captains and Captain Rules

## Problem
- Three captains change how the whole run plays: combat rules (synergy, defend, Fear and surrender, single-choice crew, extra captain move), run rules (Veteran Crew, Dread Reputation, Conqueror kill bonus) and 2 combat abilities each (fixed decision).
- Crew persist across fights (permadeath, levels, abilities, perks), but combat actors are per fight.
- Enemies must be built from the same unit definitions and abilities as the player's crew.

## Decisions

### D1 - Units
| Option | Verdict |
|---|---|
| **A. One `UDwUnitDefinition` type for every combatant (crew archetypes, enemy pirates, Kraken body, tentacles); one actor class `ADwCombatUnit` with Blueprint variants for visuals** | **Chosen.** Enemies use exactly the player's systems |
| B. Separate player/enemy classes | Rejected: duplicate stats and abilities |

### D2 - Captain rules
| Option | Verdict |
|---|---|
| **A. Captain definition = data (abilities, rule tags, GEs applied to captain/crew at spawn, tuning row); a small pure `FDwCaptainRules` reads the rule tags at fixed hooks (spawn, kill, outcome, XP)** | **Chosen.** Numbers and on/off in data; the 6 rule behaviours are code once |
| B. A UObject "rule" class per captain with virtual hooks | Rejected: more classes for 3 captains, hides rules from data validation |
| C. Every rule as a GE only | Rejected: run-level rules (XP, Dread Reputation) are not combat effects |

## Data

`UDwUnitDefinition : UDwDefinitionBase` (Data/)
| Field | Notes |
|---|---|
| `FText DisplayName`, `TSoftObjectPtr<UTexture2D> Portrait` | UI |
| `TSoftClassPtr<ADwCombatUnit> UnitClass` | `BP_Unit_Swordsman` etc. (mesh, anim BP, montages) |
| `FGameplayTagContainer UnitTags` | `Unit.Archetype.*`, `Unit.Role.Captain`, `Unit.Immune.Fear`, ... |
| `TObjectPtr<UDwAbilityDefinition> BasicAttack` | Melee or ranged basic attack |
| `TArray<TObjectPtr<UDwAbilityDefinition>> DefaultAbilities` | Granted at creation |
| `TObjectPtr<UDwAbilityPool> LearnablePool` | Shop and loot ability offers |
| `TObjectPtr<UDwPerkPool> PerkPool` | Level-up perk choices |
| `FDataTableRowHandle Tuning` | `DT_UnitTuning`: MaxHealth, MaxMana, AttackDamage, MoveRange, MaxActionPoints, per-level gains, XpOnKill, RecruitCost |

`UDwCaptainDefinition : UDwDefinitionBase` (Data/)
| Field | Notes |
|---|---|
| `FText DisplayName`, `FText Description`, `Portrait` | Captain select |
| `TObjectPtr<UDwUnitDefinition> CaptainUnit` | The captain as a unit |
| `TArray<TObjectPtr<UDwUnitDefinition>> StartingCrew` | Swordsman, Sniper |
| `TArray<TObjectPtr<UDwAbilityDefinition>> CaptainAbilities` | Exactly 2 (validated) |
| `FGameplayTagContainer RuleTags` | `Rule.Captain.*` |
| `TArray<TSubclassOf<UGameplayEffect>> CaptainCombatEffects` | Applied to the captain at spawn (e.g. extra action) |
| `TArray<TSubclassOf<UGameplayEffect>> CrewCombatEffects` | Applied to non-captain crew at spawn (e.g. grant `Rule.Turn.SingleChoice`) |
| `FGameplayTagContainer BlockedAbilityTags` | Abilities the captain can never learn. Empty = none; Conqueror = `Ability.Weapon.Firearm` (K2) |
| `FGameplayTagContainer KillBonusIgnoredUnitTags` | Victims that don't count for the kill bonus; Conqueror = `Unit.Kraken.Tentacle` (K3) |
| `FDataTableRowHandle Tuning` | `DT_CaptainTuning`: XpMultiplier, LevelThresholdMultiplier, FearPerSurrender, StartingFearCap, KillBonusPerKill, KillBonusCap, SynergyDamageMultiplier |

Changed 2026-10-25: `AllowedWeaponTags` (an allow-list) became `BlockedAbilityTags` (a block-list) because Brandon's K2 answer is "can't learn firearm abilities" - abilities with no weapon tag (items, cannons, utility abilities) stay allowed without tagging every one of them.

Run-side crew (`Run/`, see `09-Run-Map.md`): `FDwCrewMemberState` - `FGuid Id`, `FPrimaryAssetId UnitDefinition`, `bIsCaptain`, `Level`, `Xp`, `Health` (no `Mana`: MP refills every fight, C9), `TArray<FPrimaryAssetId> Abilities`, `TArray<FPrimaryAssetId> Perks`, `bAlive`. Max crew 4 including the captain (`DT_EconomyTuning.MaxCrew`). Dead crew stay in the array with `bAlive=false` for the run summary.

## Spawn and write-back
1. `UDwRun::BuildEncounterSetup` copies living crew, captain definition, counters.
2. `UDwCombatSubsystem` spawns each crew member's `UnitClass` on a `Tile.Spawn.Player` tile, applies `GE_InitUnitStats` (base + level gains from tuning, current Health from run state, Mana = MaxMana), grants the basic attack, learned abilities, captain abilities (captain only), `Ability.Move`, `Ability.Item`, `Ability.Cannon`; applies perk GEs, `CaptainCombatEffects`/`CrewCombatEffects`, and the Conqueror bonus GE.
3. At fight end, `FDwCombatResult` returns Health per survivor and the dead list; `UDwRun::ApplyCombatResult` writes them back. Permadeath = `bAlive=false`, never revived.
4. Captain death = defeat (C10). In combat it is outcome `DefeatCaptainDead` (`03-Combat-Turns.md`). The captain is never disbanded and never removed by `UDwEventOutcome_LoseCrew`; whether event damage can kill anyone is C10a.

## The three captains

| Captain | Combat rule | How it's built | Abilities (BP subclasses, numbers in `DT_AbilityTuning`) | Run rule |
|---|---|---|---|---|
| Team Leader | Synergy moves when adjacent | K4 decided: a passive - attacking while 4-way adjacent to an ally deals `SynergyDamageMultiplier` damage (X% is balance, K13). Built as a situational multiplier in `FDwDamageRules` via `FDwSynergyRules::IsActive` (`02-GAS-Foundation.md`); after every resolved command the subsystem also sets `State.Synergy.Adjacent` on crew with an adjacent ally, for the UI indicator only. No synergy abilities, no per-pair content. Scope details are K4a | Rally Cry: `UDwGA_EffectPayload`, `AllAllies`, `GE_Heal` (15) then `GE_Cleanse`. Grapple Hook: `UDwGA_Grapple`, range 10, LOS, unit target of either side | Veteran Crew: `Rule.Captain.VeteranCrew`, `XpMultiplier` and/or `LevelThresholdMultiplier` (K7) |
| Team Leader | Defend crew | `Rule.Captain.Defend` grants `GA_Defend` (payload: `GE_Guarded` on an adjacent ally, instigator = captain); `UDwGameplayAbility::ResolveTargets` redirects single-target hits on a guarded unit to its guardian (K5, open; now that captain death = defeat, Defend puts the run on the line - Brandon should weigh that in K5) | | |
| Fearmongerer | Fear, cower at 3, surrender | Fear attribute + `MaxFear` (3); cower check in turn manager; `Rule.Captain.Surrender` enables outcome rule 6 | Smoke and Fire: payload `AllEnemies`: `GE_Damage` with `RequiredTargetTags=Status.Afraid`, then `GE_AddFear` 1. Six Pistols: payload `RandomEnemies`, hits 6, 15 damage | Dread Reputation: `Rule.Captain.DreadReputation`, starting Fear = min(SurrenderWins x FearPerSurrender, StartingFearCap) via `GE_AddFear` at spawn (K9) |
| The Conqueror | Extra move, much stronger | K1 decided: a second action per turn (2 actions + the normal single move). Captain unit tuning row (higher stats) + `CaptainCombatEffects` (`GE_ConquerorExtraMove`: +1 `MaxActionPoints`). He can move between the two actions (act, move, act). Whether both actions can be the same ability is K1a (placeholder: yes, any two actions; each cannon still fires once per turn) | Charge: `UDwGA_Charge`, up to 6 tiles, first enemy hit. Eye of the Tiger: payload `SingleUnit`, `bIgnoreRange`, no LOS, `GE_Frozen` 1 turn | Kill bonus: `Rule.Captain.KillBonus`, +1% per own kill up to 20%; applies immediately; Kraken tentacles don't count (K3) |
| The Conqueror | Sword only | K2 decided: basic attack is a sword; `BlockedAbilityTags = Ability.Weapon.Firearm`, so shop, loot, events and perks never offer him firearm abilities. Items and cannons stay allowed (K2a confirms cannons) | | |
| The Conqueror | Crew: one choice per turn | `CrewCombatEffects` grants `Rule.Turn.SingleChoice`; `UDwGameplayAbility::ApplyCost` zeroes the other budget (`02-GAS-Foundation.md`) | | |

### Fear (`FDwFearRules`, Combat/, pure)
- `ShouldCower(Fear, MaxFear, bImmune) -> bool`.
- `FearAfterCower(Fear, Policy) -> float` where `EDwFearCowerPolicy` (`ResetToZero`, `Keep`, `ReduceBy`) comes from `DT_CombatTuning` (K8).
- `AllEnemiesSurrender(Snapshot) -> bool` - every living, non-immune enemy at `MaxFear`; false if no fear-capable enemies remain.
- Turn manager: at an enemy unit's turn start, if `ShouldCower`, apply `GE_Cowering`, emit `UnitTurnSkipped`, apply `FearAfterCower` when the cowering ends.

### Conqueror kill bonus (`FDwCaptainRules`, Run/, pure)
- `OnEnemyKilled(Counters, KillerIsCaptain, Tuning) -> Counters` - increments `ConquerorKillBonusPct` by `KillBonusPerKill`, capped at `KillBonusCap`.
- In combat, `HandleUnitDied` calls it when the instigator is the captain, `Rule.Captain.KillBonus` is present and the victim has none of `KillBonusIgnoredUnitTags`, then refreshes the captain's `GE_DamageBonus` (SetByCaller `Data.DamageBonus`) so it applies to his very next hit in the same fight (K3, decided). The counter persists in `FDwRunState`. Killing the Kraken body ends the run, so it never matters for the bonus.

### Other `FDwCaptainRules` hooks
- `GetXpMultiplier(Captain, Crew)` and `GetLevelThresholdMultiplier(Captain)` - used by `FDwProgressionRules`.
- `GetStartingFear(Counters, Tuning)` - Dread Reputation.
- `OnCombatWon(Counters, Outcome)` - `SurrenderWins++` on `VictorySurrender`.

## Connections
- `02-GAS-Foundation.md`: abilities, costs, single-choice, redirect, Fear attribute and tags.
- `03-Combat-Turns.md`: spawn, cower skip, death hook, outcome rule 6.
- `04-Grid-Board.md`: adjacency for synergy/defend, LOS and lines for Grapple/Charge.
- `07-Enemy-AI.md`: AI must value Fear, guarded targets and frozen units.
- `09-Run-Map.md`: roster and counters in run state.
- `10-Economy-Shop-Loot.md`: recruit/disband, ability offers filtered by `BlockedAbilityTags` and `LearnablePool`; surrender pays plunder.
- `12-Progression.md`: XP multipliers, perks.
- `14-UI.md`: captain select, Fear pips on enemies, guard icon, synergy indicator.

## Build order
1. DW-103/DW-104 (gameplay-engineer): `UDwUnitDefinition`, `ADwCombatUnit`, placeholder Swordsman/Sniper/Captain units.
2. DW-201 (gameplay-engineer): `UDwCaptainDefinition`, roster creation, write-back.
3. DW-206 Fearmongerer, DW-207 Team Leader, DW-208 Conqueror (gameplay-engineer, parallel worktrees).
4. DW-214 (ai-engineer): AI handling of Fear, guard, freeze.

## How it's tested
- Pure: `FDwFearRules` (cower, policies, surrender with immune and dead enemies); `FDwCaptainRules` (kill bonus cap at 20%, tentacle kills ignored, Dread Reputation cap, XP multipliers); `FDwSynergyRules` (ally on a 4-way neighbour -> active; diagonal ally -> inactive; no Team Leader -> inactive; enemy side never).
- Test world: each of the 6 captain abilities against a fixed board (expected HP, tiles, tags); single-choice crew cannot move after attacking; the Conqueror can act, move, act and is refused a third action; guarded ally's hit lands on the captain; surrender ends the fight with zero enemy deaths; Conqueror bonus raises damage on the next hit in the same fight; a synergy attack's preview equals its result.
- Simulator: per-captain win rate reported separately (balance target: similar rates).

## What the player sees
- Three captains that each change the run: a Team Leader whose crew fight better shoulder to shoulder, a Fearmongerer whose legend makes later crews surrender faster, and a Conqueror who strikes twice a turn and grows stronger with every kill while his crew make one choice each. Every captain is the run: if he falls, the voyage is over.
