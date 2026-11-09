// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/DwDefinitionBase.h"
#include "Abilities/DwGameplayAbility.h"
#include "DwAbilityDefinition.generated.h"

class UTexture2D;

/**
 * Expected-value hints for the AI when PreviewOutcome can't capture an ability's value (e.g. guard).
 * Fields are added by ai-engineer in DW-106/DW-214.
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwAbilityAiHints
{
	GENERATED_BODY()
};

/** The entry point for an ability: identity, text, class and tuning (Docs/Architecture/13-Data-Layout.md). */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwAbilityDefinition : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	TSoftObjectPtr<UTexture2D> Icon;

	/** A GA_* Blueprint subclass that only sets defaults; numbers come from the tuning row. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	TSubclassOf<UDwGameplayAbility> AbilityClass;

	/** Unique; used in combat commands. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (Categories = "Ability"))
	FGameplayTag AbilityTag;

	/** Weapon tags and categories (Ability.Weapon.Firearm, Ability.Attack.Melee, ...). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (Categories = "Ability"))
	FGameplayTagContainer AbilityTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	FDwAbilityAiHints AiHints;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::Ability; }
	virtual const UScriptStruct* GetTuningRowStruct() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
