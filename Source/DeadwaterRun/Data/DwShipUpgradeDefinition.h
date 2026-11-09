// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/DwDefinitionBase.h"
#include "DwShipUpgradeDefinition.generated.h"

class UGameplayEffect;
class UTexture2D;

/**
 * A shop ship upgrade (Docs/Architecture/06-Ship-Cannons.md). The upgrade list is Open-Questions P9.
 * Doc 06 says "tuning row (price)" but doc 13 defines no ship-upgrade table, so no tuning row is required yet
 * (raised for technical-architect in the DW-004 report).
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwShipUpgradeDefinition : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Infinite GE on the ship, e.g. +MaxHealth or +CannonDamage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	TSubclassOf<UGameplayEffect> Effect;

	/** e.g. Ship.Upgrade.ExtraCannon unlocks layout cannon tiles that require it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade", meta = (Categories = "Ship"))
	FGameplayTagContainer GrantedShipTags;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::ShipUpgrade; }
};
