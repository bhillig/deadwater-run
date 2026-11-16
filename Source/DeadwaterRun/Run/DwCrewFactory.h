// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Run/DwRunTypes.h"

class UDwUnitDefinition;

/**
 * Adds crew to a run's roster (Docs/Architecture/10 "Shop": the same function for the starting crew and shop recruits).
 * Pure: no world, no randomness beyond the seeded roster id.
 */
struct DEADWATERRUN_API FDwCrewFactory
{
	/** The next roster id: seeded from the run seed and LastCrewSerial, never random, so a seed always gives the same ids. */
	static FGuid MakeId(FDwRunState& State);

	/** Appends a living crew member of Unit at Level (at least 1) with full Health for that level, no XP, nothing learned. */
	static FDwCrewMemberState& Create(FDwRunState& State, const UDwUnitDefinition& Unit, int32 Level, bool bIsCaptain);
};
