// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Core/DwDefinitionBase.h"
#include "Data/DwDataTypes.h"
#include "DwShopDefinition.generated.h"

class UDwItemDefinition;
class UDwShipUpgradeDefinition;
class UDwUnitDefinition;

/**
 * What a shop can offer, DA_Shop_Default (Docs/Architecture/10 "Shop"). Offer counts live in DT_EconomyTuning; every price
 * lives on the offered thing's own tuning row (DT_ItemTuning.Price, DT_AbilityTuning.ShopPrice, DT_UnitTuning.RecruitCost,
 * DT_ShipUpgradeTuning.Price), repair's in DT_EconomyTuning.RepairCost. FDwShopRules::GenerateInventory reads it.
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwShopDefinition : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TArray<EDwShopOfferType> OfferTypes = {
		EDwShopOfferType::Repair, EDwShopOfferType::Item, EDwShopOfferType::Ability,
		EDwShopOfferType::Recruit, EDwShopOfferType::ShipUpgrade, EDwShopOfferType::Disband
	};

	/** Every item is always on sale (and is what plunder drops, P6). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TArray<TObjectPtr<UDwItemDefinition>> Items;

	/** ShopRecruitOffers of these, drawn without repeats per visit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TArray<TObjectPtr<UDwUnitDefinition>> RecruitPool;

	/** ShopUpgradeOffers of the ones the ship doesn't have yet, drawn without repeats per visit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TArray<TObjectPtr<UDwShipUpgradeDefinition>> UpgradePool;

	bool Offers(EDwShopOfferType Type) const { return OfferTypes.Contains(Type); }

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::Rules; }

#if WITH_EDITOR
	/** No empty entries; every item, recruit and upgrade has a tuning row with a price above 0. */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
