// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DwEnemyCrewContentLibrary.generated.h"

/**
 * Creates or updates the five enemy crews (DW-305, Open-Questions N2/N3/C10b/N5): DA_Encounter_<Crew> (roster, map
 * tier, shared enemy deck) and DA_AiProfile_<Crew> (switches; weights in DT_AiTuning row <Crew>, Tuning/AiTuning.json).
 * Gold and XP come from DT_EncounterTuning row <Crew>. Levels, weights and rewards are DW-401 placeholders
 * (Docs/Balance/Placeholders.md). Runs after UDwAiContentLibrary (it syncs DT_AiTuning) and before
 * UDwArtContentLibrary (which gives every crew its sloop and raider looks). Idempotent.
 */
UCLASS()
class DEADWATERRUNEDITOR_API UDwEnemyCrewContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The crew ids, in tier order: DA_Encounter_<Id>, DA_AiProfile_<Id>, tuning rows <Id>. */
	static TArray<FString> CrewIds();

	/** One line per asset touched or problem found; the last line is "CONTENT PASS" or "CONTENT FAIL". */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Content")
	static TArray<FString> CreateEnemyCrewContent();
};
