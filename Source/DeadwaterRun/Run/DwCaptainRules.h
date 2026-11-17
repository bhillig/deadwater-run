// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Combat/DwCombatTypes.h"

class UDwCaptainDefinition;
struct FDwCaptainCounters;

/**
 * Captain run rules (Docs/Architecture/05-Crew-Captains.md "Other FDwCaptainRules hooks"). Pure: reads the captain's
 * rule tags and DT_CaptainTuning row; UDwRun and the combat subsystem call it at fixed hooks.
 */
struct DEADWATERRUN_API FDwCaptainRules
{
	/** Veteran Crew (Rule.Captain.VeteranCrew): DT_CaptainTuning XpMultiplier. 1 for every other captain. */
	static float GetXpMultiplier(const UDwCaptainDefinition* Captain);

	/** Veteran Crew: DT_CaptainTuning LevelThresholdMultiplier (XP needed per level x this). 1 for every other captain. */
	static float GetLevelThresholdMultiplier(const UDwCaptainDefinition* Captain);

	/**
	 * Dread Reputation (Rule.Captain.DreadReputation, K9): the Fear every enemy starts a fight with,
	 * min(SurrenderWins x FearPerSurrender, StartingFearCap). 0 for every other captain.
	 */
	static float GetStartingFear(const FDwCaptainCounters& Counters, const UDwCaptainDefinition* Captain);

	/** After a won fight: SurrenderWins + 1 on VictorySurrender (Dread Reputation's count). */
	static void OnCombatWon(FDwCaptainCounters& Counters, EDwCombatOutcome Outcome);
};
