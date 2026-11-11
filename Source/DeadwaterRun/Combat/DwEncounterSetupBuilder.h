// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Combat/DwCombatTypes.h"

class UDwEncounterDefinition;
class UDwGameData;

/**
 * Turns content (DA_GameData + an encounter definition) into an FDwEncounterSetup: board from the deck layouts,
 * grid/fear numbers from DT_CombatTuning, ships from DT_ShipTuning, enemies in encounter order, common abilities.
 * The run (DW-201) adds crew from run state; the dev quick start passes placeholder crew. Pure: no world.
 */
struct DEADWATERRUN_API FDwEncounterSetupBuilder
{
	/** Builds the encounter's kind: BuildShipBattle or BuildKraken. */
	static bool Build(const UDwGameData& GameData, const UDwEncounterDefinition& Encounter, const TArray<FDwCombatantSetup>& Crew,
		int32 Seed, FDwEncounterSetup& Out, TArray<FString>& OutErrors);

	/** False (with a message per problem) if any referenced data is missing. Out is only valid on success. */
	static bool BuildShipBattle(const UDwGameData& GameData, const UDwEncounterDefinition& Encounter, const TArray<FDwCombatantSetup>& Crew,
		int32 Seed, FDwEncounterSetup& Out, TArray<FString>& OutErrors);

	/**
	 * The boss (Docs/Architecture/08): the player ship beside the Kraken's arena, no enemy ship, the body as the only
	 * enemy (driven by the encounter's AiProfile until UDwKrakenAiBrain, DW-302), and a boss outcome config - killing
	 * the body is VictoryBoss. Tentacles are DW-301: a tuning row asking for any is reported as an error.
	 */
	static bool BuildKraken(const UDwGameData& GameData, const UDwEncounterDefinition& Encounter, const TArray<FDwCombatantSetup>& Crew,
		int32 Seed, FDwEncounterSetup& Out, TArray<FString>& OutErrors);
};
