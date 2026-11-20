// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Core/DwDefinitionBase.h"
#include "DwSimPolicy.generated.h"

class UDwAiProfile;

/**
 * How the headless simulator plays a run outside the fights (Docs/Architecture/17-Testing-Simulator.md): route, shop,
 * rewards, perks, events, plus the numbers that turn a run into estimated minutes. Several assets = several "player
 * types" (DA_SimPolicy_<Name>, picked with -Policy=). A tool setting, not game tuning: its numbers live on the asset,
 * and no game rule reads it. Owner: tools-engineer.
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwSimPolicy : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	/** Plays the player's crew in fights through UDwCrewAiBrain. DA_AiProfile_SimPlayer once DW-214 delivers it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fights")
	TObjectPtr<UDwAiProfile> PlayerProfile;

	/** A fight still going after this many rounds is a stuck fight (the run ends as Stuck). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fights", meta = (ClampMin = "1"))
	int32 MaxRoundsPerFight = 40;

	/** Route choice weights, drawn from the policy's own stream. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "0")) float FightWeight = 1.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "0")) float ShopWeight = 1.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "0")) float EventWeight = 1.f;
	/** ShopWeight multiplier while the hull is below RepairBelowHullPct (head for a repair). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "0")) float HurtShopBias = 3.f;

	/** Shop: repairs while the hull is below this fraction of max, before anything else. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "0", ClampMax = "1"))
	float RepairBelowHullPct = 0.7f;

	/** Shop: recruits while fewer crew than this are alive. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "0"))
	int32 RecruitBelowCrew = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop") bool bBuyShipUpgrades = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop") bool bBuyAbilities = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop") bool bBuyItems = true;

	/** Rewards: learn an offered ability when a crew member has a free slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewards") bool bTakeAbilities = true;

	/** Estimated run length = (player actions x this + enemy actions x SecondsPerEnemyAction + nodes x SecondsPerNode) / 60. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Estimate", meta = (ClampMin = "0")) float SecondsPerAction = 8.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Estimate", meta = (ClampMin = "0")) float SecondsPerEnemyAction = 2.f;
	/** Map, rewards, level-ups, shop or event screens per node. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Estimate", meta = (ClampMin = "0")) float SecondsPerNode = 30.f;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::SimPolicy; }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
