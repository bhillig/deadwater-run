// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Core/DwDefinitionBase.h"
#include "Data/DwDataTypes.h"
#include "DwMapRules.generated.h"

/** Weights for unconstrained map slots. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwNodeTypeWeights
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weights", meta = (ClampMin = "0")) float Fight = 1.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weights", meta = (ClampMin = "0")) float Shop = 1.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weights", meta = (ClampMin = "0")) float Event = 1.f;
};

/** Map generation rules, DA_MapRules (Docs/Architecture/09-Run-Map.md). Defaults are the design numbers. */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwMapRules : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	/** Excluding the boss. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "1")) int32 MinPathNodes = 8;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "1")) int32 MaxPathNodes = 9;

	/** "At least" vs "exactly" is Open-Questions M2. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "0")) int32 MinFightsPerPath = 5;

	/** 3 lanes gives 2-3 choices per step. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "1")) int32 LaneCount = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map") bool bNoConsecutiveShops = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map") bool bNoConsecutiveEvents = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map") bool bShopOptionBeforeBoss = true;

	/** Open-Questions M3. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map") EDwNodeType FirstLayerType = EDwNodeType::Fight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map") FDwNodeTypeWeights TypeWeights;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "1")) int32 MaxAttempts = 50;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::Rules; }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
