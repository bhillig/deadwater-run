// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DwRunContentLibrary.generated.h"

/**
 * Creates or updates the run content of DW-201: the three captains (DA_Captain_*), their captain units, their two
 * combat abilities as placeholder definitions (GA_Captain_Placeholder does nothing; DW-206..208 give each its real
 * class), DA_GameData.Captains, and the DT_Captain/Economy/UnitTuning rows they read (from Tuning/*.json).
 * Runs after UDwCombatContentLibrary (it needs the swordsman, sniper and cutlass) and before UDwArtContentLibrary
 * (which dresses the captain units). Idempotent. Run headless with Tools/CreateCombatContent.ps1.
 */
UCLASS()
class DEADWATERRUNEDITOR_API UDwRunContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One line per asset touched or problem found; the last line is "CONTENT PASS" or "CONTENT FAIL". */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Content")
	static TArray<FString> CreateRunContent();
};
