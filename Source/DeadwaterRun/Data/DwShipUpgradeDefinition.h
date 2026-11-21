// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/DwDefinitionBase.h"
#include "DwShipUpgradeDefinition.generated.h"

class UGameplayEffect;
class UTexture2D;

/**
 * A shop ship upgrade (Docs/Architecture/06-Ship-Cannons.md, P9). Bought once per run, kept in FDwShipState::Upgrades.
 * Numbers live in the DT_ShipUpgradeTuning row (Price, MaxHealthBonus, CannonDamageBonus): FDwShopRules adds the bonuses
 * to the run's ship stats, which combat spawns the ship with. GrantedShipTags unlock deck tiles that require them
 * (Ship.Upgrade.ExtraCannon -> the third cannon).
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwShipUpgradeDefinition : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	TSoftObjectPtr<UTexture2D> Icon;

	/**
	 * Optional infinite GE applied to the ship at every fight spawn, for effects that are not one of the tuning row's stat
	 * bonuses. Empty for the three v1 upgrades: a stat bonus here as well would count twice.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	TSubclassOf<UGameplayEffect> Effect;

	/** e.g. Ship.Upgrade.ExtraCannon unlocks the player deck's cannon tiles that require it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade", meta = (Categories = "Ship.Upgrade"))
	FGameplayTagContainer GrantedShipTags;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::ShipUpgrade; }
	virtual const UScriptStruct* GetTuningRowStruct() const override;

	/** Description with {MaxHealthBonus}, {CannonDamageBonus} and {Price} filled from the tuning row. */
	FText GetFormattedDescription() const;
	/** GetSummary() (the shop ledger's short line), filled the same way. */
	FText GetFormattedSummary() const;

private:
	FText FormatFromTuning(const FText& Pattern) const;
};
