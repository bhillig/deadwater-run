// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/DwDefinitionBase.h"
#include "DwPerkDefinition.generated.h"

class UDwAbilityDefinition;
class UGameplayEffect;
class UTexture2D;

/** A level-up perk: an infinite GE (asset tag Effect.Perk) and/or a granted ability (Docs/Architecture/12-Progression.md). */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwPerkDefinition : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk")
	TSubclassOf<UGameplayEffect> Effect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk")
	TObjectPtr<UDwAbilityDefinition> GrantedAbility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk", meta = (Categories = "Unit"))
	FGameplayTagContainer RequiredUnitTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk", meta = (ClampMin = "1"))
	int32 MaxStacks = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk", meta = (ClampMin = "0"))
	float Weight = 1.f;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::Perk; }
	virtual const UScriptStruct* GetTuningRowStruct() const override;
};
