// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Core/DwDefinitionBase.h"
#include "Data/DwDataTypes.h"
#include "DwShopDefinition.generated.h"

class UDwItemDefinition;
class UDwShipUpgradeDefinition;
class UDwUnitDefinition;

/** What a shop can offer, DA_Shop_Default. Offer counts live in DT_EconomyTuning (Docs/Architecture/10). */
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TArray<TObjectPtr<UDwItemDefinition>> Items;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TArray<TObjectPtr<UDwUnitDefinition>> RecruitPool;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TArray<TObjectPtr<UDwShipUpgradeDefinition>> UpgradePool;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::Rules; }
};
