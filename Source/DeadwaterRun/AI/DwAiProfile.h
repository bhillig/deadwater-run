// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Core/DwDefinitionBase.h"
#include "Data/Tuning/DwAiTuningRow.h"
#include "DwAiProfile.generated.h"

/**
 * A crew personality for the utility AI (Docs/Architecture/07-Enemy-AI.md). Owned by ai-engineer.
 * What the crew is (switches) lives here; every number (weights, Temperature, MaxMoveCandidates) lives in its
 * DT_AiTuning row so the Balance Designer tunes it in Tuning/AiTuning.json. Encounters and enemy slots reference one;
 * FDwEncounterSetupBuilder copies it onto each enemy's FDwCombatantSetup.
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwAiProfile : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	/** Gunners: cannon tiles attract the crew (FDwAiWeights::CannonAffinity applies). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	bool bPreferCannons = false;

	/** Boarders: the opponent's deck attracts the crew (FDwAiWeights::Boarding applies). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	bool bBoarders = false;

	/** The DT_AiTuning row, or an all-zero row (and an error in the log) if the link is broken. */
	FDwAiTuningRow GetTuningValues() const;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::AiProfile; }
	virtual const UScriptStruct* GetTuningRowStruct() const override { return FDwAiTuningRow::StaticStruct(); }
};
