// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DwKrakenContentLibrary.generated.h"

/**
 * Creates or updates the placeholder Kraken of DW-209 (Docs/Architecture/08 "Placeholder boss"): the arena layout
 * DA_Deck_KrakenArena, GA_Kraken_TentacleSlam and its definition, the body unit DA_Unit_KrakenBody, DA_Kraken_Placeholder
 * (one phase, Tentacle Slam only, no tentacles), DA_Encounter_KrakenPlaceholder (kind Kraken) and DA_GameData.KrakenEncounter,
 * plus the DT_KrakenTuning rows (from Tuning/KrakenTuning.json). Numbers come from DT_UnitTuning (KrakenBody),
 * DT_AbilityTuning (Kraken_TentacleSlam) and DT_EncounterTuning (Kraken). Runs after UDwAiContentLibrary (the body uses
 * DA_AiProfile_Default until UDwKrakenAiBrain, DW-302). DW-301 replaces it with the real Kraken. Idempotent.
 */
UCLASS()
class DEADWATERRUNEDITOR_API UDwKrakenContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The arena, authored facing right like every layout: its rightmost column lies next to the player's rail. */
	static TArray<FString> PlaceholderArenaRows();

	/** One line per asset touched or problem found; the last line is "CONTENT PASS" or "CONTENT FAIL". */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Content")
	static TArray<FString> CreateKrakenContent();
};
