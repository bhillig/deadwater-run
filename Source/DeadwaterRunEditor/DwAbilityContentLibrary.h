// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DwAbilityContentLibrary.generated.h"

/**
 * Creates or updates the crew abilities (DW-304, Open-Questions N1 = Set A of Docs/Balance/N1-AbilityDrafts.md): twelve
 * GA_Crew_* payload Blueprints with their DA_Ability_Crew_* definitions, the archetype pools DA_AbilityPool_Swordsman
 * and DA_AbilityPool_Sniper (what the shop sells and rewards offer), and points DA_Unit_Swordsman / DA_Unit_Sniper at
 * them. Captains get no pool: crew abilities are crew only. Numbers come from DT_AbilityTuning (Tuning/AbilityTuning.json,
 * synced by the combat content). Runs after UDwRunContentLibrary. Idempotent.
 */
UCLASS()
class DEADWATERRUNEDITOR_API UDwAbilityContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One line per asset touched or problem found; the last line is "CONTENT PASS" or "CONTENT FAIL". */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Content")
	static TArray<FString> CreateAbilityContent();
};
