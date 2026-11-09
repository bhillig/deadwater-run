// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Core/DwDefinitionBase.h"
#include "Data/DwDataTypes.h"
#include "DwPools.generated.h"

class UDwPerkDefinition;

/** Weighted list of learnable abilities; each unit definition points at its pool (Docs/Architecture/10). */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwAbilityPool : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pool")
	TArray<FDwWeightedAbility> Abilities;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::AbilityPool; }
};

/** Level-up perk choices; shared or per archetype is Open-Questions P2 (Docs/Architecture/12). */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwPerkPool : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pool")
	TArray<TObjectPtr<UDwPerkDefinition>> Perks;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::PerkPool; }
};
