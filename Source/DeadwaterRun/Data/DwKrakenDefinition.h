// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/DwDefinitionBase.h"
#include "Data/DwDataTypes.h"
#include "DwKrakenDefinition.generated.h"

class UDwDeckLayout;
class UDwUnitDefinition;

/** One boss phase. Phases are ordered; each starts when body Health drops below EnterBelowHealthPct. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwKrakenPhase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase")
	FName PhaseId;

	/** 1.0 for the initial phase, 0.3 for enraged (design: "health below 30%"). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase", meta = (ClampMin = "0", ClampMax = "1"))
	float EnterBelowHealthPct = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase")
	TArray<FDwWeightedAbility> Moves;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase", meta = (Categories = "Music"))
	FGameplayTag MusicState;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase", meta = (Categories = "GameplayCue"))
	FGameplayTag EnterCue;
};

/** What tentacles do (Open-Questions B2, B3). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwKrakenTentacleRules
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tentacles")
	EDwTentacleEffect Effect = EDwTentacleEffect::ReduceDamage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tentacles")
	bool bTentacleDamageHitsBody = false;
};

/** The final boss (Docs/Architecture/08-Kraken-Boss.md). */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwKrakenDefinition : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kraken")
	TObjectPtr<UDwUnitDefinition> Body;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kraken")
	TObjectPtr<UDwUnitDefinition> Tentacle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kraken")
	TObjectPtr<UDwDeckLayout> Arena;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kraken")
	TArray<FDwKrakenPhase> Phases;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kraken")
	FDwKrakenTentacleRules TentacleRules;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::Kraken; }
	virtual const UScriptStruct* GetTuningRowStruct() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
