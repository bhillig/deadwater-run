// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Core/DwDefinitionBase.h"
#include "Abilities/DwTargetingRules.h"
#include "DwItemDefinition.generated.h"

class UGameplayEffect;
class UTexture2D;

/** A consumable (health / mana restore). Docs/Architecture/10-Economy-Shop-Loot.md. */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwItemDefinition : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UTexture2D> Icon;

	/** e.g. GE_Heal with Data.Heal from the tuning row's Amount. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TSubclassOf<UGameplayEffect> Effect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FDwTargetingRules Targeting;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStack = 1;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::Item; }
	virtual const UScriptStruct* GetTuningRowStruct() const override;
};
