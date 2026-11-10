// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Run/DwRunTypes.h"

class UDwItemDefinition;
struct FDwEconomyTuningRow;
struct FDwEncounterTuningRow;

/** Everything ComputeRewards reads. Missing rows pay nothing rather than fail (a fight without tuning is still won). */
struct DEADWATERRUN_API FDwRewardContext
{
	EDwCombatOutcome Outcome = EDwCombatOutcome::None;
	const FDwEncounterTuningRow* Encounter = nullptr;
	const FDwEconomyTuningRow* Economy = nullptr;
	/** Roster (living crew learn), captain (BlockedAbilityTags, K2). */
	const FDwRunState* State = nullptr;
	/** What plunder can drop: DA_GameData.DefaultShop items (P6). */
	TArray<const UDwItemDefinition*> PlunderItems;
};

/**
 * Gold, plunder and "learn a new move" after a won fight (Docs/Architecture/10-Economy-Shop-Loot.md "Rewards"). Pure:
 * UDwRun is the only caller; the simulator and tests call it directly. XP is FDwProgressionRules' (12).
 */
struct DEADWATERRUN_API FDwRewardRules
{
	/** Sink SinkLootMultiplier, crew defeated PlunderLootMultiplier, surrender SurrenderLootMultiplier; boss and defeats 0. */
	static float GetLootMultiplier(EDwCombatOutcome Outcome, const FDwEconomyTuningRow& Economy);

	/** Plunder = crew defeated or surrender: the outcomes that roll PlunderItemChance. Never on a sink. */
	static bool IsPlunder(EDwCombatOutcome Outcome);

	/** Gold, plunder item and ability choices. Leaves Xp empty. Same Random state -> same bundle. */
	static FDwRewardBundle ComputeRewards(const FDwRewardContext& Context, FRandomStream& Random);

	/**
	 * Up to Count abilities, weighted, no repeats, drawn from the living crew's learnable pools. Each offer lists who can
	 * learn it: the ability is in their pool, they don't know it yet (P5), and for the captain it carries none of the
	 * captain's BlockedAbilityTags (K2). Abilities nobody can learn are never offered.
	 */
	static TArray<FDwAbilityOffer> RollAbilityChoices(const FDwRunState& State, int32 Count, FRandomStream& Random);

	/** Whether Member may learn Ability (the eligibility rule above, without the pool check). */
	static bool CanLearn(const FDwRunState& State, const FDwCrewMemberState& Member, const UDwAbilityDefinition& Ability);
};
