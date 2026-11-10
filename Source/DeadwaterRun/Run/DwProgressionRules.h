// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Run/DwRunTypes.h"

class UDataTable;
class UDwPerkDefinition;
class UDwPerkPool;
struct FDwUnitTuningRow;

/**
 * XP, levels and perks (Docs/Architecture/12-Progression.md "Rules"). Pure: UDwRun is the only caller, the simulator
 * and tests call it directly. DT_ProgressionTuning has one row per level, named by the level ("1", "2", ...), holding
 * the XP needed to reach the next one; FDwCrewMemberState::Xp is the progress inside the current level.
 */
struct DEADWATERRUN_API FDwProgressionRules
{
	/** XP to go from Level to Level + 1, x ThresholdMultiplier (rounded, at least 1). 0 = no row: Level is the last. */
	static int32 GetXpToNext(const UDataTable* Progression, int32 Level, float ThresholdMultiplier = 1.f);

	/**
	 * Crew.Xp += round(Amount x XpMultiplier); levels up while Xp reaches GetXpToNext, up to MaxLevel (<= 0: the table's
	 * last row). Each level's MaxHealth gain is added to current Health too. Dead crew gain nothing.
	 * Returns the levels gained (several for a big gain).
	 */
	static int32 GrantXp(FDwCrewMemberState& Crew, int32 Amount, float XpMultiplier, float ThresholdMultiplier, const UDataTable* Progression,
		int32 MaxLevel);

	/** Base + per-level gains x (Level - 1); the same numbers combat spawns with (FDwUnitTuningRow::Get*AtLevel). */
	static FDwLevelStats GetLevelStats(const FDwUnitTuningRow& Row, int32 Level);

	/** How many times Crew already has Perk. */
	static int32 CountPerk(const FDwCrewMemberState& Crew, const UDwPerkDefinition& Perk);

	/** Perk is below MaxStacks for Crew and Crew's unit has every RequiredUnitTag. */
	static bool CanTakePerk(const FDwCrewMemberState& Crew, const UDwPerkDefinition& Perk);

	/** Up to Count perks from Pool that Crew can take, weighted, no duplicates in one offer. Stable per Random state. */
	static TArray<const UDwPerkDefinition*> RollPerkChoices(const FDwCrewMemberState& Crew, const UDwPerkPool* Pool, int32 Count, FRandomStream& Random);

	/** Adds Perk (and its GrantedAbility, if not known yet). False, changing nothing, if CanTakePerk fails. */
	static bool ApplyPerk(FDwCrewMemberState& Crew, const UDwPerkDefinition& Perk);
};
