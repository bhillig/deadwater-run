// Copyright Brandon Hillig 2026

#include "Run/DwRunTypes.h"

#include "Abilities/DwAttributeSets.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwPerkDefinition.h"
#include "Data/DwShipUpgradeDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"

float FDwCrewMemberState::GetMaxHealth() const
{
	const FDwUnitTuningRow* Row = Unit ? Unit->GetTuningRow<FDwUnitTuningRow>() : nullptr;
	if (!Row)
	{
		return 0.f;
	}
	float MaxHealth = static_cast<float>(Row->GetMaxHealthAtLevel(Level));
	for (const UDwPerkDefinition* Perk : Perks)
	{
		MaxHealth += Perk ? Perk->GetAttributeBonus(UDwUnitAttributeSet::GetMaxHealthAttribute()) : 0.f; // Thick Skin, Brawler
	}
	return MaxHealth;
}

FText FDwCrewMemberState::GetDisplayName() const
{
	return Unit ? Unit->DisplayName : FText::GetEmpty();
}

TArray<int32> FDwMapGraph::GetChoices(int32 Current) const
{
	TArray<int32> Choices;
	if (Current == INDEX_NONE)
	{
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			if (Nodes[Index].Layer == 0 && Nodes[Index].Type != EDwNodeType::Boss)
			{
				Choices.Add(Index);
			}
		}
	}
	else if (Nodes.IsValidIndex(Current))
	{
		Choices = Nodes[Current].Next;
	}
	return Choices;
}

const FDwCrewMemberState* FDwRunState::FindCrew(const FGuid& Id) const
{
	return Crew.FindByPredicate([&Id](const FDwCrewMemberState& Member) { return Member.Id == Id; });
}

FDwCrewMemberState* FDwRunState::FindCrew(const FGuid& Id)
{
	return Crew.FindByPredicate([&Id](const FDwCrewMemberState& Member) { return Member.Id == Id; });
}

const FDwCrewMemberState* FDwRunState::GetCaptainMember() const
{
	return Crew.FindByPredicate([](const FDwCrewMemberState& Member) { return Member.bIsCaptain; });
}

int32 FDwRunState::CountLivingCrew() const
{
	int32 Count = 0;
	for (const FDwCrewMemberState& Member : Crew)
	{
		Count += Member.bAlive ? 1 : 0;
	}
	return Count;
}

int32 FDwRunState::CountItem(FName ItemId) const
{
	int32 Count = 0;
	for (const FDwItemStack& Stack : Inventory)
	{
		if (Stack.Item && Stack.Item->GetFName() == ItemId)
		{
			Count += Stack.Count;
		}
	}
	return Count;
}

int32 FDwRunState::CountItems() const
{
	int32 Count = 0;
	for (const FDwItemStack& Stack : Inventory)
	{
		Count += Stack.Item ? FMath::Max(0, Stack.Count) : 0;
	}
	return Count;
}

int32 FDwRunState::GetItemRoom(int32 MaxInventory) const
{
	return MaxInventory > 0 ? FMath::Max(0, MaxInventory - CountItems()) : MAX_int32;
}

int32 FDwRunState::AddItem(const UDwItemDefinition* Item, int32 Count, int32 MaxInventory)
{
	const int32 Added = Item ? FMath::Clamp(Count, 0, GetItemRoom(MaxInventory)) : 0;
	if (Added == 0)
	{
		return 0;
	}
	if (FDwItemStack* Stack = Inventory.FindByPredicate([Item](const FDwItemStack& S) { return S.Item == Item; }))
	{
		Stack->Count += Added;
		return Added;
	}
	FDwItemStack& Stack = Inventory.AddDefaulted_GetRef();
	Stack.Item = Item;
	Stack.Count = Added;
	return Added;
}

FGameplayTagContainer FDwShipState::GetUpgradeTags() const
{
	FGameplayTagContainer Tags;
	for (const UDwShipUpgradeDefinition* Upgrade : Upgrades)
	{
		if (Upgrade)
		{
			Tags.AppendTags(Upgrade->GrantedShipTags);
		}
	}
	return Tags;
}

namespace DwRun
{
	FString PhaseName(EDwRunPhase Phase)
	{
		return StaticEnum<EDwRunPhase>()->GetNameStringByValue(static_cast<int64>(Phase));
	}

	FString EndReasonName(EDwRunEndReason Reason)
	{
		return StaticEnum<EDwRunEndReason>()->GetNameStringByValue(static_cast<int64>(Reason));
	}

	FString PurchaseResultName(EDwPurchaseResult Result)
	{
		return StaticEnum<EDwPurchaseResult>()->GetNameStringByValue(static_cast<int64>(Result));
	}

	FString OfferTypeName(EDwShopOfferType Type)
	{
		return StaticEnum<EDwShopOfferType>()->GetNameStringByValue(static_cast<int64>(Type));
	}

	EDwRunEndReason EndReasonFor(EDwCombatOutcome Outcome)
	{
		switch (Outcome)
		{
		case EDwCombatOutcome::DefeatShipSunk:    return EDwRunEndReason::ShipSunk;
		case EDwCombatOutcome::DefeatCrewDead:    return EDwRunEndReason::CrewDead;
		case EDwCombatOutcome::DefeatCaptainDead: return EDwRunEndReason::CaptainDead;
		default:                                  return EDwRunEndReason::None;
		}
	}
}
