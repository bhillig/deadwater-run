// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Combat/DwCombatTypes.h"

class UDwEncounterDefinition;
class UDwGameData;

/**
 * Turns content (DA_GameData + an encounter definition) into an FDwEncounterSetup: board from the two deck layouts,
 * grid/fear numbers from DT_CombatTuning, ships from DT_ShipTuning, enemies in encounter order, common abilities.
 * The run (DW-201) adds crew from run state; the dev quick start passes placeholder crew. Pure: no world.
 */
struct DEADWATERRUN_API FDwEncounterSetupBuilder
{
	/** False (with a message per problem) if any referenced data is missing. Out is only valid on success. */
	static bool BuildShipBattle(const UDwGameData& GameData, const UDwEncounterDefinition& Encounter, const TArray<FDwCombatantSetup>& Crew,
		int32 Seed, FDwEncounterSetup& Out, TArray<FString>& OutErrors);
};
