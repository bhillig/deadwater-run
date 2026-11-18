// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DwKrakenContentLibrary.generated.h"

/**
 * Creates or updates the Kraken (DW-301, Docs/Architecture/08-Kraken-Boss.md): the arena DA_Deck_KrakenArena (4 tentacle
 * slots, body anchor), the five moves GA/DA_Ability_Kraken_*, the units DA_Unit_KrakenBody and DA_Unit_KrakenTentacle,
 * DA_Kraken (Initial and Enraged phases, tentacle rules), DA_Encounter_Kraken (kind Kraken) and DA_GameData.KrakenEncounter,
 * plus the DT_KrakenTuning rows (from Tuning/KrakenTuning.json). Numbers come from DT_UnitTuning (KrakenBody,
 * KrakenTentacle), DT_AbilityTuning (Kraken_*), DT_KrakenTuning and DT_EncounterTuning (Kraken). Runs after
 * UDwAiContentLibrary (the body uses DA_AiProfile_Default until UDwKrakenAiBrain, DW-302). Deletes the DW-209
 * placeholder Kraken and encounter if present. Idempotent.
 */
UCLASS()
class DEADWATERRUNEDITOR_API UDwKrakenContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The arena, authored facing right like every layout: its rightmost column lies next to the player's rail. */
	static TArray<FString> ArenaRows();

	/** One line per asset touched or problem found; the last line is "CONTENT PASS" or "CONTENT FAIL". */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Content")
	static TArray<FString> CreateKrakenContent();
};
