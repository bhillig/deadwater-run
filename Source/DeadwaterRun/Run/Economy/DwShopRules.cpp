// Copyright Brandon Hillig 2026

#include "Run/Economy/DwShopRules.h"

#include "Data/DwAbilityDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwShipUpgradeDefinition.h"
#include "Data/DwShopDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Run/DwCrewFactory.h"
#include "Run/Economy/DwRewardRules.h"

namespace DwShop
{
	/** Each offer's own tuning row is the only place its price lives (Docs/Architecture/13). 0 without a row. */
	static int32 PriceOf(const UDwItemDefinition& Item)
	{
		const FDwItemTuningRow* Row = Item.GetTuningRow<FDwItemTuningRow>();
		return Row ? Row->Price : 0;
	}

	static int32 PriceOf(const UDwAbilityDefinition& Ability)
	{
		const FDwAbilityTuningRow* Row = Ability.GetTuningRow<FDwAbilityTuningRow>();
		return Row ? Row->ShopPrice : 0;
	}

	static int32 PriceOf(const UDwUnitDefinition& Unit)
	{
		const FDwUnitTuningRow* Row = Unit.GetTuningRow<FDwUnitTuningRow>();
		return Row ? Row->RecruitCost : 0;
	}

	static int32 PriceOf(const UDwShipUpgradeDefinition& Upgrade)
	{
		const FDwShipUpgradeTuningRow* Row = Upgrade.GetTuningRow<FDwShipUpgradeTuningRow>();
		return Row ? Row->Price : 0;
	}

	static FDwShopOffer& Add(FDwShopInventory& Inventory, EDwShopOfferType Type, int32 Price)
	{
		FDwShopOffer& Offer = Inventory.Offers.AddDefaulted_GetRef();
		Offer.OfferId = Inventory.Offers.Num() - 1;
		Offer.Type = Type;
		Offer.Price = Price;
		return Offer;
	}

	/** Up to Count entries of Candidates, uniformly without repeats; candidate order is kept so the draw is stable. */
	template <typename T>
	static TArray<T*> Draw(TArray<T*> Candidates, int32 Count, FRandomStream& Random)
	{
		TArray<T*> Picked;
		while (Picked.Num() < Count && !Candidates.IsEmpty())
		{
			const int32 Pick = Random.RandHelper(Candidates.Num());
			Picked.Add(Candidates[Pick]);
			Candidates.RemoveAt(Pick);
		}
		return Picked;
	}
}

FDwShopInventory FDwShopRules::GenerateInventory(const FDwRunState& State, const UDwShopDefinition& Shop, const FDwShopContext& Context,
	FRandomStream& Random)
{
	FDwShopInventory Inventory;
	if (!Context.Economy)
	{
		return Inventory;
	}
	const FDwEconomyTuningRow& Economy = *Context.Economy;

	if (Shop.Offers(EDwShopOfferType::Repair) && Economy.RepairCost > 0 && Economy.RepairAmount > 0)
	{
		DwShop::Add(Inventory, EDwShopOfferType::Repair, Economy.RepairCost).Amount = Economy.RepairAmount;
	}
	if (Shop.Offers(EDwShopOfferType::Item))
	{
		for (const UDwItemDefinition* Item : Shop.Items)
		{
			if (Item && DwShop::PriceOf(*Item) > 0)
			{
				DwShop::Add(Inventory, EDwShopOfferType::Item, DwShop::PriceOf(*Item)).Item = Item;
			}
		}
	}

	// The random draws always run in this order, so turning one offer type off never shifts another's.
	if (Shop.Offers(EDwShopOfferType::Ability) && Economy.ShopAbilityOffers > 0)
	{
		for (const FDwAbilityOffer& Choice : FDwRewardRules::RollAbilityChoices(State, Economy.ShopAbilityOffers, Random))
		{
			if (Choice.Ability && DwShop::PriceOf(*Choice.Ability) > 0)
			{
				FDwShopOffer& Offer = DwShop::Add(Inventory, EDwShopOfferType::Ability, DwShop::PriceOf(*Choice.Ability));
				Offer.Ability = Choice.Ability;
				Offer.EligibleCrew = Choice.EligibleCrew;
			}
		}
	}
	if (Shop.Offers(EDwShopOfferType::Recruit) && Economy.ShopRecruitOffers > 0)
	{
		TArray<const UDwUnitDefinition*> Candidates;
		for (const UDwUnitDefinition* Unit : Shop.RecruitPool)
		{
			if (Unit && DwShop::PriceOf(*Unit) > 0)
			{
				Candidates.AddUnique(Unit);
			}
		}
		for (const UDwUnitDefinition* Unit : DwShop::Draw(Candidates, Economy.ShopRecruitOffers, Random))
		{
			FDwShopOffer& Offer = DwShop::Add(Inventory, EDwShopOfferType::Recruit, DwShop::PriceOf(*Unit));
			Offer.Unit = Unit;
			Offer.Level = GetRecruitLevel(State, Economy);
		}
	}
	if (Shop.Offers(EDwShopOfferType::ShipUpgrade) && Economy.ShopUpgradeOffers > 0)
	{
		TArray<const UDwShipUpgradeDefinition*> Candidates;
		for (const UDwShipUpgradeDefinition* Upgrade : Shop.UpgradePool)
		{
			if (Upgrade && !State.Ship.HasUpgrade(Upgrade) && DwShop::PriceOf(*Upgrade) > 0)
			{
				Candidates.AddUnique(Upgrade);
			}
		}
		for (const UDwShipUpgradeDefinition* Upgrade : DwShop::Draw(Candidates, Economy.ShopUpgradeOffers, Random))
		{
			DwShop::Add(Inventory, EDwShopOfferType::ShipUpgrade, DwShop::PriceOf(*Upgrade)).Upgrade = Upgrade;
		}
	}
	if (Shop.Offers(EDwShopOfferType::Disband))
	{
		Inventory.bDisbands = true;
		for (const FDwCrewMemberState& Member : State.Crew)
		{
			if (Member.bAlive && !Member.bIsCaptain)
			{
				FDwShopOffer& Offer = DwShop::Add(Inventory, EDwShopOfferType::Disband, 0);
				Offer.CrewId = Member.Id;
				Offer.Amount = FMath::Max(0, Economy.DisbandRefund);
			}
		}
	}
	return Inventory;
}

EDwPurchaseResult FDwShopRules::CanPurchase(const FDwRunState& State, int32 OfferId, const FGuid& TargetCrew, int32 ReplaceSlot,
	const FDwShopContext& Context)
{
	const FDwShopOffer* Offer = State.Shop.IsOpen() ? State.Shop.Find(OfferId) : nullptr;
	if (!Offer || !Context.Economy)
	{
		return EDwPurchaseResult::NoOffer;
	}
	if (Offer->bSoldOut)
	{
		return EDwPurchaseResult::SoldOut;
	}
	const FDwEconomyTuningRow& Economy = *Context.Economy;

	switch (Offer->Type)
	{
	case EDwShopOfferType::Repair:
		if (State.Ship.Health >= State.Ship.MaxHealth)
		{
			return EDwPurchaseResult::ShipFull;
		}
		break;
	case EDwShopOfferType::Item:
		if (!Offer->Item)
		{
			return EDwPurchaseResult::NoOffer;
		}
		if (State.GetItemRoom(Economy.MaxInventory) <= 0)
		{
			return EDwPurchaseResult::InventoryFull;
		}
		break;
	case EDwShopOfferType::Ability:
	{
		const FDwCrewMemberState* Learner = State.FindCrew(TargetCrew);
		if (!Offer->Ability)
		{
			return EDwPurchaseResult::NoOffer;
		}
		if (!Learner || !Learner->bAlive)
		{
			return EDwPurchaseResult::InvalidTarget;
		}
		if (FDwRewardRules::Knows(*Learner, *Offer->Ability))
		{
			return EDwPurchaseResult::AlreadyKnown;
		}
		if (!FDwRewardRules::IsInPool(*Learner, *Offer->Ability) || !FDwRewardRules::CanLearn(State, *Learner, *Offer->Ability))
		{
			return EDwPurchaseResult::InvalidTarget;
		}
		const bool bFull = Context.MaxAbilitySlots > 0 && Learner->Abilities.Num() >= Context.MaxAbilitySlots;
		if (ReplaceSlot != INDEX_NONE ? !Learner->Abilities.IsValidIndex(ReplaceSlot) : bFull)
		{
			return EDwPurchaseResult::NoSlot;
		}
		break;
	}
	case EDwShopOfferType::Recruit:
		if (!Offer->Unit)
		{
			return EDwPurchaseResult::NoOffer;
		}
		if (Economy.MaxCrew > 0 && State.CountLivingCrew() >= Economy.MaxCrew)
		{
			return EDwPurchaseResult::CrewFull;
		}
		break;
	case EDwShopOfferType::ShipUpgrade:
		if (!Offer->Upgrade)
		{
			return EDwPurchaseResult::NoOffer;
		}
		if (State.Ship.HasUpgrade(Offer->Upgrade))
		{
			return EDwPurchaseResult::SoldOut;
		}
		break;
	case EDwShopOfferType::Disband:
	{
		const FDwCrewMemberState* Member = State.FindCrew(Offer->CrewId);
		if (!Member || !Member->bAlive)
		{
			return EDwPurchaseResult::SoldOut; // already gone
		}
		if (Member->bIsCaptain || CountLivingCrewBesideCaptain(State) <= 1)
		{
			return EDwPurchaseResult::CantDisband;
		}
		break;
	}
	}

	return State.Gold < Offer->Price ? EDwPurchaseResult::NotEnoughGold : EDwPurchaseResult::Ok;
}

EDwPurchaseResult FDwShopRules::ApplyPurchase(FDwRunState& State, int32 OfferId, const FGuid& TargetCrew, int32 ReplaceSlot,
	const FDwShopContext& Context)
{
	const EDwPurchaseResult Result = CanPurchase(State, OfferId, TargetCrew, ReplaceSlot, Context);
	if (Result != EDwPurchaseResult::Ok)
	{
		return Result;
	}
	const FDwEconomyTuningRow& Economy = *Context.Economy;
	FDwShopOffer& Offer = *State.Shop.Find(OfferId);
	State.Gold -= Offer.Price;
	State.Stats.GoldSpent += Offer.Price;
	++State.Stats.Purchases.FindOrAdd(Offer.Type);

	switch (Offer.Type)
	{
	case EDwShopOfferType::Repair:
		State.Ship.Health = FMath::Min(State.Ship.MaxHealth, State.Ship.Health + Offer.Amount);
		break;
	case EDwShopOfferType::Item:
		State.AddItem(Offer.Item, 1, Economy.MaxInventory);
		break;
	case EDwShopOfferType::Ability:
	{
		FDwCrewMemberState& Learner = *State.FindCrew(TargetCrew);
		if (ReplaceSlot != INDEX_NONE)
		{
			Learner.Abilities[ReplaceSlot] = Offer.Ability;
		}
		else
		{
			Learner.Abilities.Add(Offer.Ability);
		}
		break;
	}
	case EDwShopOfferType::Recruit:
		FDwCrewFactory::Create(State, *Offer.Unit, Offer.Level, /*bIsCaptain*/ false);
		break;
	case EDwShopOfferType::ShipUpgrade:
		if (const FDwShipUpgradeTuningRow* Row = Offer.Upgrade->GetTuningRow<FDwShipUpgradeTuningRow>())
		{
			State.Ship.MaxHealth += Row->MaxHealthBonus;
			State.Ship.Health = FMath::Clamp(State.Ship.Health + Row->MaxHealthBonus, 0.f, State.Ship.MaxHealth);
			State.Ship.CannonDamage += Row->CannonDamageBonus;
		}
		State.Ship.Upgrades.Add(Offer.Upgrade);
		break;
	case EDwShopOfferType::Disband:
	{
		const FGuid Leaving = Offer.CrewId;
		State.Crew.RemoveAll([&Leaving](const FDwCrewMemberState& Member) { return Member.Id == Leaving; });
		State.Gold += Offer.Amount;
		break;
	}
	}

	Offer.bSoldOut = SellsOnce(Offer.Type);
	Refresh(State, Context);
	return EDwPurchaseResult::Ok;
}

void FDwShopRules::Refresh(FDwRunState& State, const FDwShopContext& Context)
{
	if (!State.Shop.IsOpen() || !Context.Economy)
	{
		return;
	}
	const FDwEconomyTuningRow& Economy = *Context.Economy;
	TSet<FGuid> HasDisbandOffer;
	for (FDwShopOffer& Offer : State.Shop.Offers)
	{
		if (Offer.Type == EDwShopOfferType::Disband)
		{
			HasDisbandOffer.Add(Offer.CrewId);
		}
		if (Offer.bSoldOut)
		{
			continue;
		}
		switch (Offer.Type)
		{
		case EDwShopOfferType::Ability:
			Offer.EligibleCrew = Offer.Ability ? FDwRewardRules::GetEligibleCrew(State, *Offer.Ability) : TArray<FGuid>();
			break;
		case EDwShopOfferType::Recruit:
			Offer.Level = GetRecruitLevel(State, Economy);
			break;
		case EDwShopOfferType::ShipUpgrade:
			Offer.bSoldOut = State.Ship.HasUpgrade(Offer.Upgrade);
			break;
		case EDwShopOfferType::Disband:
		{
			const FDwCrewMemberState* Member = State.FindCrew(Offer.CrewId);
			Offer.bSoldOut = !Member || !Member->bAlive;
			break;
		}
		default:
			break;
		}
	}
	// Recruits who joined this visit can be disbanded too (when this shop disbands at all).
	if (State.Shop.bDisbands)
	{
		for (const FDwCrewMemberState& Member : State.Crew)
		{
			if (Member.bAlive && !Member.bIsCaptain && !HasDisbandOffer.Contains(Member.Id))
			{
				FDwShopOffer& Offer = DwShop::Add(State.Shop, EDwShopOfferType::Disband, 0);
				Offer.CrewId = Member.Id;
				Offer.Amount = FMath::Max(0, Economy.DisbandRefund);
			}
		}
	}
}

int32 FDwShopRules::GetRecruitLevel(const FDwRunState& State, const FDwEconomyTuningRow& Economy)
{
	int32 Living = 0;
	int32 Levels = 0;
	for (const FDwCrewMemberState& Member : State.Crew)
	{
		if (Member.bAlive)
		{
			++Living;
			Levels += Member.Level;
		}
	}
	const int32 Average = Living > 0 ? Levels / Living : 1; // integer division = floor for positive levels
	return FMath::Max(1, Average + Economy.RecruitLevelOffset);
}

int32 FDwShopRules::CountLivingCrewBesideCaptain(const FDwRunState& State)
{
	int32 Count = 0;
	for (const FDwCrewMemberState& Member : State.Crew)
	{
		Count += Member.bAlive && !Member.bIsCaptain ? 1 : 0;
	}
	return Count;
}
