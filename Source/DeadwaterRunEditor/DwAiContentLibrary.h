// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DwAiContentLibrary.generated.h"

/**
 * Creates or updates the crew AI content of DW-106 headless, so it is reproducible instead of hand-clicked:
 * DT_AiTuning (synced from Tuning/AiTuning.json), DA_AiProfile_Default, the placeholder encounter's AI profile and the
 * DA_GameData tuning-table slot. Idempotent. Run by Tools/CreateCombatContent.ps1 after the combat content.
 */
UCLASS()
class DEADWATERRUNEDITOR_API UDwAiContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One line per asset touched or problem found; the last line is "CONTENT PASS" or "CONTENT FAIL". */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Content")
	static TArray<FString> CreateAiContent();
};
