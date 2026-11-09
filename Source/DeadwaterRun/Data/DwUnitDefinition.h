// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/DwDefinitionBase.h"
#include "DwUnitDefinition.generated.h"

class ADwCombatUnit;
class UDwAbilityDefinition;
class UDwAbilityPool;
class UDwPerkPool;
class UTexture2D;

/** Every combatant: crew archetypes, enemy pirates, Kraken body, tentacles (Docs/Architecture/05-Crew-Captains.md). */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwUnitDefinition : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unit")
	TSoftObjectPtr<UTexture2D> Portrait;

	/** BP_Unit_* (mesh, anim BP, montages). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unit")
	TSoftClassPtr<ADwCombatUnit> UnitClass;

	/** Unit.Archetype.*, Unit.Role.Captain, Unit.Immune.Fear, ... */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unit", meta = (Categories = "Unit"))
	FGameplayTagContainer UnitTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UDwAbilityDefinition> BasicAttack;

	/** Granted at creation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TArray<TObjectPtr<UDwAbilityDefinition>> DefaultAbilities;

	/** Shop and loot ability offers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UDwAbilityPool> LearnablePool;

	/** Level-up perk choices. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression")
	TObjectPtr<UDwPerkPool> PerkPool;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::Unit; }
	virtual const UScriptStruct* GetTuningRowStruct() const override;
};
