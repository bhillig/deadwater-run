// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Run/DwRunTypes.h"

class UDwShopDefinition;
struct FDwEconomyTuningRow;

/** The numbers the shop rules read besides each offer's own tuning row. */
struct DEADWATERRUN_API FDwShopContext
{
	/** DT_EconomyTuning "Default": repair, crew and inventory caps, recruit level offset, offer counts, disband refund. */
	const FDwEconomyTuningRow* Economy = nullptr;
	/** DT_RunTuning.MaxAbilitySlots (P4); 0 = no limit. */
	int32 MaxAbilitySlots = 0;
};

/**
 * The shop (Docs/Architecture/10-Economy-Shop-Loot.md "Shop"). Pure: UDwRun is the only caller; the simulator and tests
 * call it directly. Works on FDwRunState::Shop, the inventory of the shop the run is at.
 *
 * Prices come from each offer's own tuning row (repair: DT_EconomyTuning.RepairCost). Repair and items can be bought
 * again and again; ability, recruit, upgrade and disband offers sell once. Rules: gold never goes negative; the living
 * crew never exceeds MaxCrew; the captain and the last crew member beside him are never disbanded; repair never goes
 * past max hull; items stop at MaxInventory; abilities respect MaxAbilitySlots (a full crew member names one to
 * replace) and the captain's BlockedAbilityTags (K2); each upgrade at most once.
 */
struct DEADWATERRUN_API FDwShopRules
{
	/**
	 * The offers for one visit, in this order: repair, every item of Shop, ShopAbilityOffers abilities (weighted from
	 * the living crew's learnable pools, FDwRewardRules::RollAbilityChoices), ShopRecruitOffers recruits and
	 * ShopUpgradeOffers upgrades the ship lacks (both drawn without repeats), then a disband per crew member beside the
	 * captain. Only offer types in Shop.OfferTypes appear; anything without a price above 0 is left out. Same state and
	 * Random state -> same inventory. Node is left INDEX_NONE for the caller to set.
	 */
	static FDwShopInventory GenerateInventory(const FDwRunState& State, const UDwShopDefinition& Shop, const FDwShopContext& Context,
		FRandomStream& Random);

	/**
	 * Whether State can buy offer OfferId of State.Shop. TargetCrew is the learner of an ability offer (ignored
	 * otherwise); ReplaceSlot (index into the learner's Abilities) names the ability to forget, required once they know
	 * MaxAbilitySlots. Checked in EDwPurchaseResult order: offer, target, room, gold.
	 */
	static EDwPurchaseResult CanPurchase(const FDwRunState& State, int32 OfferId, const FGuid& TargetCrew, int32 ReplaceSlot,
		const FDwShopContext& Context);

	/**
	 * CanPurchase, then, only if Ok: takes the gold (Stats.GoldSpent), applies the offer - hull +RepairAmount (clamped);
	 * one item; the ability learned (or replacing ReplaceSlot); a recruit created by FDwCrewFactory at the offer's level;
	 * the upgrade added with its DT_ShipUpgradeTuning bonuses (MaxHealthBonus raises current hull too); the crew member
	 * removed from the roster with DisbandRefund paid - marks one-time offers sold out and refreshes the rest. On any
	 * other result nothing changes.
	 */
	static EDwPurchaseResult ApplyPurchase(FDwRunState& State, int32 OfferId, const FGuid& TargetCrew, int32 ReplaceSlot,
		const FDwShopContext& Context);

	/**
	 * Keeps State.Shop current after the roster changed: ability offers' EligibleCrew, recruit levels, a disband offer
	 * for every crew member beside the captain (new recruits get one; members who left sell out), owned upgrades sold out.
	 */
	static void Refresh(FDwRunState& State, const FDwShopContext& Context);

	/** max(1, floor(average level of the living crew, captain included) + RecruitLevelOffset) (P8). */
	static int32 GetRecruitLevel(const FDwRunState& State, const FDwEconomyTuningRow& Economy);

	/** Living crew beside the captain. */
	static int32 CountLivingCrewBesideCaptain(const FDwRunState& State);

	/** Offer types that sell once (everything but repair and items). */
	static bool SellsOnce(EDwShopOfferType Type) { return Type != EDwShopOfferType::Repair && Type != EDwShopOfferType::Item; }
};
