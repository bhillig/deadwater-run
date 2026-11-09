// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Combat/DwCombatTypes.h"
#include "Data/DwDataTypes.h"

class UDwAbilityDefinition;
class UDwGameplayAbility;
struct FDwCombatSnapshot;
struct FDwUnitSnapshot;

/*
 * Pure combat rules (Docs/Architecture/03-Combat-Turns.md). No world: they read an FDwCombatSnapshot, so the combat
 * subsystem, the UI, the AI and the simulator all apply exactly the same checks.
 */

/** Docs/Architecture/03 "Outcome priority": one ordered list, defeats before victories (C12). */
struct DEADWATERRUN_API FDwCombatOutcomeRules
{
	static EDwCombatOutcome Evaluate(const FDwCombatSnapshot& Snapshot, const FDwOutcomeConfig& Config);
};

/** Fear and surrender (Docs/Architecture/05-Crew-Captains.md "Fear"). */
struct DEADWATERRUN_API FDwFearRules
{
	static bool ShouldCower(float Fear, float MaxFear, bool bImmune);

	/** Fear left once a unit has cowered for its turn (K8 policy from DT_CombatTuning). */
	static float FearAfterCower(float Fear, EDwFearCowerPolicy Policy);

	/** Every living enemy that can feel fear is at MaxFear. False if none can (Unit.Immune.Fear units are ignored). */
	static bool AllEnemiesSurrender(const FDwCombatSnapshot& Snapshot);
};

struct DEADWATERRUN_API FDwCombatQueries
{
	/** The ability CDO of a definition, or nullptr. Pure queries (targets, previews, budgets) run on it. */
	static const UDwGameplayAbility* GetAbilityCDO(const UDwAbilityDefinition* Definition);

	/** The unit's granted ability with this AbilityTag, or nullptr. */
	static const UDwAbilityDefinition* FindAbility(const FDwUnitSnapshot& Unit, const FGameplayTag& AbilityTag);

	/**
	 * Every rule SubmitCommand applies, in order: outcome, side, sequence, unit, ability, status, item, budget, target.
	 * Does not know about "busy" (that is live subsystem state). OutReason gets a player-facing reason on rejection.
	 */
	static EDwCommandResult ValidateCommand(const FDwCombatSnapshot& Snapshot, const FDwCombatCommand& Command, FText* OutReason = nullptr);

	/** Status and budget checks for one ability, before any target is considered (greyed-out buttons). */
	static EDwCommandResult CanActivate(const FDwCombatSnapshot& Snapshot, const FDwUnitSnapshot& Unit, const UDwAbilityDefinition* Ability,
		FName ItemId = NAME_None, FText* OutReason = nullptr);

	/** The ability can be activated and has at least one valid target (for items: with any item in stock). */
	static bool HasAnyUse(const FDwCombatSnapshot& Snapshot, const FDwUnitSnapshot& Unit, const UDwAbilityDefinition* Ability);

	/**
	 * True if any living, non-incapacitated unit of Side can still move or use any ability on a valid target.
	 * Same checks as ValidateCommand, so the "End turn?" prompt never appears while a legal action exists (C14).
	 */
	static bool SideHasAvailableAction(const FDwCombatSnapshot& Snapshot, EDwSide Side);
};

/** Cannons (Docs/Architecture/06-Ship-Cannons.md, C7). */
struct DEADWATERRUN_API FDwCannonRules
{
	/**
	 * An unused Tile.Cannon of Side's own ship on UnitTile or 4-way next to it. The unit's own tile wins, then the lowest
	 * board index (every cannon on a ship deals the same damage, so the choice never changes the outcome).
	 */
	static TOptional<FIntPoint> FindUsableCannon(const FDwGridModel& Grid, FIntPoint UnitTile, EDwSide Side, const TArray<FIntPoint>& UsedCannons);
};
