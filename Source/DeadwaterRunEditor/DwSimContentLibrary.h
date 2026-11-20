// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DwSimContentLibrary.generated.h"

/**
 * Creates or updates the simulator's content (DW-212, Docs/Architecture/17): DA_SimPolicy_Default in Data/Sim. Its
 * player profile is DA_AiProfile_Default until DW-214 delivers DA_AiProfile_SimPlayer (the captain-ability terms).
 * Runs after UDwAiContentLibrary. Idempotent. Owner: tools-engineer.
 */
UCLASS()
class DEADWATERRUNEDITOR_API UDwSimContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One line per asset touched or problem found; the last line is "CONTENT PASS" or "CONTENT FAIL". */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Content")
	static TArray<FString> CreateSimContent();
};
