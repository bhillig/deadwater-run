// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DwCombatContentLibrary.generated.h"

/**
 * Creates or updates the placeholder combat content of DW-104 so it is reproducible and reviewable instead of
 * hand-clicked: GA_* ability Blueprints, DA_Ability_* / DA_Item_* / DA_Unit_* definitions, DA_Encounter_Placeholder,
 * BP_Ship_Placeholder, IA_* / IMC_Combat, the DA_GameData slots they plug into (incl. the dev fight), the DT_*Tuning rows
 * they read (from Tuning/*.json), and lighting for L_Sea.
 * Idempotent: running it again rewrites the same assets with the same values. Run headless with
 * Tools/CreateCombatContent.ps1. Real art (DW-307) and real tuning (DW-112) replace values, not structure.
 */
UCLASS()
class DEADWATERRUNEDITOR_API UDwCombatContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One line per asset touched or problem found; the last line is "CONTENT PASS" or "CONTENT FAIL". */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Content")
	static TArray<FString> CreateCombatPlaceholderContent();

	/**
	 * Refills DT_<Name>Tuning from Tuning/<Name>Tuning.json (e.g. "Ability"). A stop-gap until the strict importer
	 * (DW-109) exists; UE's JSON import reports unknown rows/fields as problems, which are returned as errors.
	 */
	static bool SyncTuningTable(const FString& Name, TArray<FString>& OutLog);
};
