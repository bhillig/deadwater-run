// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"

class UDwCaptainDefinition;

/**
 * Captain run rules (Docs/Architecture/05-Crew-Captains.md). Pure. DW-203 adds the Veteran Crew XP hooks; Fear,
 * surrender counters (DW-206) and the Conqueror kill bonus (DW-208) join here.
 */
struct DEADWATERRUN_API FDwCaptainRules
{
	/** Veteran Crew (Rule.Captain.VeteranCrew): DT_CaptainTuning XpMultiplier. 1 for every other captain. */
	static float GetXpMultiplier(const UDwCaptainDefinition* Captain);

	/** Veteran Crew: DT_CaptainTuning LevelThresholdMultiplier (XP needed per level x this). 1 for every other captain. */
	static float GetLevelThresholdMultiplier(const UDwCaptainDefinition* Captain);
};
