// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/DwDefinitionBase.h"
#include "DwCaptainDefinition.generated.h"

class UDwUnitDefinition;
class UDwAbilityDefinition;
class UGameplayEffect;
class UTexture2D;

/** A captain = data: abilities, rule tags, spawn GEs, tuning (Docs/Architecture/05-Crew-Captains.md). */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwCaptainDefinition : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	static constexpr int32 RequiredCaptainAbilities = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Captain")
	TSoftObjectPtr<UTexture2D> Portrait;

	/** The captain as a unit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Captain")
	TObjectPtr<UDwUnitDefinition> CaptainUnit;

	/** Swordsman, Sniper. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Captain")
	TArray<TObjectPtr<UDwUnitDefinition>> StartingCrew;

	/** Exactly 2 (validated). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Captain")
	TArray<TObjectPtr<UDwAbilityDefinition>> CaptainAbilities;

	/** Rule.Captain.* */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules", meta = (Categories = "Rule"))
	FGameplayTagContainer RuleTags;

	/** Applied to the captain at spawn (e.g. GE_ConquerorExtraMove). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules")
	TArray<TSubclassOf<UGameplayEffect>> CaptainCombatEffects;

	/** Applied to non-captain crew at spawn (e.g. grant Rule.Turn.SingleChoice). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules")
	TArray<TSubclassOf<UGameplayEffect>> CrewCombatEffects;

	/** Abilities the captain can never learn. Conqueror = Ability.Weapon.Firearm (K2). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules", meta = (Categories = "Ability"))
	FGameplayTagContainer BlockedAbilityTags;

	/** Victims that don't count for the kill bonus. Conqueror = Unit.Kraken.Tentacle (K3). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules", meta = (Categories = "Unit"))
	FGameplayTagContainer KillBonusIgnoredUnitTags;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::Captain; }
	virtual const UScriptStruct* GetTuningRowStruct() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
