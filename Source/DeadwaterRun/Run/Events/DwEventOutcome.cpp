// Copyright Brandon Hillig 2026

#include "Run/Events/DwEventOutcome.h"

#include "Data/DwCaptainDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Run/DwCrewFactory.h"
#include "Run/DwProgressionRules.h"
#include "Run/Economy/DwRewardRules.h"
#include "Run/Economy/DwShopRules.h"

#define LOCTEXT_NAMESPACE "DwEvents"

namespace DwEventText
{
	FText Name(const UDwDefinitionBase* Definition)
	{
		return Definition && !Definition->DisplayName.IsEmpty() ? Definition->DisplayName : FText::FromString(GetNameSafe(Definition));
	}

	FText Signed(int32 Value)
	{
		return FText::FromString(FString::Printf(TEXT("%+d"), Value));
	}

	FText Who(EDwCrewSelector Selector)
	{
		switch (Selector)
		{
		case EDwCrewSelector::All:     return LOCTEXT("WhoAll", "Every crew member");
		case EDwCrewSelector::Random:  return LOCTEXT("WhoRandom", "A random crew member");
		case EDwCrewSelector::Captain: return LOCTEXT("WhoCaptain", "Your captain");
		case EDwCrewSelector::Chosen:  return LOCTEXT("WhoChosen", "A crew member you choose");
		}
		return FText::GetEmpty();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Context

TArray<FDwCrewMemberState*> FDwEventContext::Select(EDwCrewSelector Selector, bool bAllowCaptain)
{
	TArray<FDwCrewMemberState*> Living;
	for (FDwCrewMemberState& Member : State.Crew)
	{
		if (Member.bAlive && (bAllowCaptain || !Member.bIsCaptain))
		{
			Living.Add(&Member);
		}
	}
	switch (Selector)
	{
	case EDwCrewSelector::All:
		return Living;
	case EDwCrewSelector::Random:
		if (Living.IsEmpty())
		{
			return {};
		}
		return { Living[Random.RandRange(0, Living.Num() - 1)] };
	case EDwCrewSelector::Captain:
		for (FDwCrewMemberState* Member : Living)
		{
			if (Member->bIsCaptain)
			{
				return { Member };
			}
		}
		return {};
	case EDwCrewSelector::Chosen:
		for (FDwCrewMemberState* Member : Living)
		{
			if (Member->Id == ChosenCrew)
			{
				return { Member };
			}
		}
		return {};
	}
	return {};
}

int32 FDwEventContext::GetMaxInventory() const
{
	return Economy ? FMath::Max(0, Economy->MaxInventory) : 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// Outcomes

FText UDwEventOutcome_Nothing::Describe() const
{
	return LOCTEXT("Nothing", "Nothing happens.");
}

void UDwEventOutcome_Gold::Apply(FDwEventContext& Context) const
{
	FDwRunState& State = Context.State;
	const int32 Before = State.Gold;
	State.Gold = FMath::Max(0, State.Gold + Delta);
	const int32 Change = State.Gold - Before;
	State.Stats.GoldEarned += FMath::Max(0, Change);
	Context.Changes.Add(Change == Delta
		? FText::Format(LOCTEXT("GoldChange", "{0} gold"), DwEventText::Signed(Change))
		: FText::Format(LOCTEXT("GoldFloor", "{0} gold (all you had)"), DwEventText::Signed(Change)));
}

FText UDwEventOutcome_Gold::Describe() const
{
	return FText::Format(LOCTEXT("GoldDescribe", "{0} gold"), DwEventText::Signed(Delta));
}

void UDwEventOutcome_Gold::Validate(TArray<FText>& OutProblems) const
{
	if (Delta == 0)
	{
		OutProblems.Add(LOCTEXT("GoldZero", "Gold outcome with Delta 0."));
	}
}

void UDwEventOutcome_ShipHealth::Apply(FDwEventContext& Context) const
{
	FDwShipState& Ship = Context.State.Ship;
	const float Before = Ship.Health;
	// C10a a): never sinks the ship. A hull already at 1 stays at 1.
	Ship.Health = FMath::Clamp(Ship.Health + Delta, FMath::Min(1.f, Ship.Health), Ship.MaxHealth);
	const int32 Change = FMath::RoundToInt(Ship.Health - Before);
	Context.Changes.Add(Change == 0
		? FText::Format(LOCTEXT("ShipSame", "Hull unchanged ({0} / {1})"), FMath::RoundToInt(Ship.Health), FMath::RoundToInt(Ship.MaxHealth))
		: FText::Format(LOCTEXT("ShipChange", "Hull {0} ({1} / {2})"), DwEventText::Signed(Change), FMath::RoundToInt(Ship.Health), FMath::RoundToInt(Ship.MaxHealth)));
}

FText UDwEventOutcome_ShipHealth::Describe() const
{
	return FText::Format(LOCTEXT("ShipDescribe", "{0} hull"), DwEventText::Signed(Delta));
}

void UDwEventOutcome_ShipHealth::Validate(TArray<FText>& OutProblems) const
{
	if (Delta == 0)
	{
		OutProblems.Add(LOCTEXT("ShipZero", "Ship Health outcome with Delta 0."));
	}
}

void UDwEventOutcome_CrewHealth::Apply(FDwEventContext& Context) const
{
	for (FDwCrewMemberState* Member : Context.Select(Selector))
	{
		const float Before = Member->Health;
		// C10a a): event damage never kills anyone.
		Member->Health = FMath::Clamp(Member->Health + Delta, FMath::Min(1.f, Member->Health), Member->GetMaxHealth());
		const int32 Change = FMath::RoundToInt(Member->Health - Before);
		Context.Changes.Add(Change == 0
			? FText::Format(LOCTEXT("CrewSame", "{0} unchanged ({1} / {2} HP)"), Member->GetDisplayName(), FMath::RoundToInt(Member->Health),
				FMath::RoundToInt(Member->GetMaxHealth()))
			: FText::Format(LOCTEXT("CrewChange", "{0} {1} HP ({2} / {3})"), Member->GetDisplayName(), DwEventText::Signed(Change),
				FMath::RoundToInt(Member->Health), FMath::RoundToInt(Member->GetMaxHealth())));
	}
}

FText UDwEventOutcome_CrewHealth::Describe() const
{
	return Delta >= 0
		? FText::Format(LOCTEXT("CrewHeal", "{0} recovers {1} HP"), DwEventText::Who(Selector), Delta)
		: FText::Format(LOCTEXT("CrewHurt", "{0} loses {1} HP"), DwEventText::Who(Selector), -Delta);
}

void UDwEventOutcome_CrewHealth::Validate(TArray<FText>& OutProblems) const
{
	if (Delta == 0)
	{
		OutProblems.Add(LOCTEXT("CrewZero", "Crew Health outcome with Delta 0."));
	}
}

void UDwEventOutcome_GiveItem::Apply(FDwEventContext& Context) const
{
	if (!Item || Count <= 0)
	{
		return;
	}
	FDwRunState& State = Context.State;
	const int32 Added = State.AddItem(Item, Count, Context.GetMaxInventory());
	const int32 Left = Count - Added;
	State.Stats.ItemsLeftBehind += Left;
	if (Added > 0)
	{
		Context.Changes.Add(FText::Format(LOCTEXT("ItemGain", "+{0} {1}"), Added, DwEventText::Name(Item)));
	}
	if (Left > 0)
	{
		Context.Changes.Add(FText::Format(LOCTEXT("ItemNoRoom", "No room in the hold: {0} {1} left behind"), Left, DwEventText::Name(Item)));
	}
}

FText UDwEventOutcome_GiveItem::Describe() const
{
	return FText::Format(LOCTEXT("ItemDescribe", "+{0} {1}"), Count, DwEventText::Name(Item));
}

void UDwEventOutcome_GiveItem::Validate(TArray<FText>& OutProblems) const
{
	if (!Item)
	{
		OutProblems.Add(LOCTEXT("ItemNone", "Give Item outcome has no Item."));
	}
}

void UDwEventOutcome_AbilityChoice::Apply(FDwEventContext& Context) const
{
	for (const FDwAbilityOffer& Offer : FDwRewardRules::RollAbilityChoices(Context.State, Count, Context.Random))
	{
		if (!Context.AbilityChoices.ContainsByPredicate([&Offer](const FDwAbilityOffer& Existing) { return Existing.Ability == Offer.Ability; }))
		{
			Context.AbilityChoices.Add(Offer);
		}
	}
	Context.Changes.Add(Context.AbilityChoices.IsEmpty()
		? LOCTEXT("NoAbility", "Nothing your crew could learn")
		: FText::Format(LOCTEXT("AbilityGain", "A chance to learn a new move ({0} to choose from)"), Context.AbilityChoices.Num()));
}

FText UDwEventOutcome_AbilityChoice::Describe() const
{
	return LOCTEXT("AbilityDescribe", "Learn a new move");
}

void UDwEventOutcome_AbilityChoice::Validate(TArray<FText>& OutProblems) const
{
	if (Count <= 0)
	{
		OutProblems.Add(LOCTEXT("AbilityZero", "Ability Choice outcome offers 0 abilities."));
	}
}

void UDwEventOutcome_GrantXp::Apply(FDwEventContext& Context) const
{
	for (FDwCrewMemberState* Member : Context.Select(Selector))
	{
		const int32 OldLevel = Member->Level;
		const int32 Gained = FDwProgressionRules::GrantXp(*Member, Amount, Context.XpMultiplier, Context.ThresholdMultiplier, Context.Progression,
			Context.MaxLevel);
		FDwProgressionRules::AppendLevelUps(Context.LevelUps, *Member, OldLevel);
		const int32 Xp = FMath::RoundToInt(Amount * Context.XpMultiplier);
		Context.Changes.Add(Gained > 0
			? FText::Format(LOCTEXT("XpLevel", "{0} +{1} XP - level up!"), Member->GetDisplayName(), Xp)
			: FText::Format(LOCTEXT("Xp", "{0} +{1} XP"), Member->GetDisplayName(), Xp));
	}
}

FText UDwEventOutcome_GrantXp::Describe() const
{
	return FText::Format(LOCTEXT("XpDescribe", "{0} gains {1} XP"), DwEventText::Who(Selector), Amount);
}

void UDwEventOutcome_GrantXp::Validate(TArray<FText>& OutProblems) const
{
	if (Amount <= 0)
	{
		OutProblems.Add(LOCTEXT("XpZero", "Grant XP outcome with Amount 0."));
	}
}

void UDwEventOutcome_Recruit::Apply(FDwEventContext& Context) const
{
	if (!Unit)
	{
		return;
	}
	FDwRunState& State = Context.State;
	const int32 MaxCrew = Context.Economy ? Context.Economy->MaxCrew : 0;
	if (MaxCrew > 0 && State.CountLivingCrew() >= MaxCrew)
	{
		Context.Changes.Add(FText::Format(LOCTEXT("RecruitFull", "No bunk to spare: the {0} stays behind"), DwEventText::Name(Unit)));
		return;
	}
	const int32 Level = Context.Economy ? FDwShopRules::GetRecruitLevel(State, *Context.Economy) : 1;
	const FDwCrewMemberState& Member = FDwCrewFactory::Create(State, *Unit, Level, /*bIsCaptain*/ false);
	Context.Changes.Add(FText::Format(LOCTEXT("RecruitJoin", "{0} joins the crew (level {1})"), Member.GetDisplayName(), Member.Level));
}

FText UDwEventOutcome_Recruit::Describe() const
{
	return FText::Format(LOCTEXT("RecruitDescribe", "A {0} joins the crew"), DwEventText::Name(Unit));
}

void UDwEventOutcome_Recruit::Validate(TArray<FText>& OutProblems) const
{
	if (!Unit)
	{
		OutProblems.Add(LOCTEXT("RecruitNone", "Recruit outcome has no Unit."));
	}
}

void UDwEventOutcome_LoseCrew::Apply(FDwEventContext& Context) const
{
	if (Selector == EDwCrewSelector::Captain)
	{
		return; // never the captain (validated)
	}
	FDwRunState& State = Context.State;
	for (FDwCrewMemberState* Member : Context.Select(Selector, /*bAllowCaptain*/ false))
	{
		Member->bAlive = false;
		Member->Health = 0.f;
		FDwCrewLoss& Loss = State.Stats.CrewLost.AddDefaulted_GetRef();
		Loss.CrewId = Member->Id;
		Loss.Name = Member->GetDisplayName();
		Loss.Node = State.CurrentNode;
		Context.Changes.Add(FText::Format(LOCTEXT("Lost", "{0} is lost"), Member->GetDisplayName()));
	}
}

FText UDwEventOutcome_LoseCrew::Describe() const
{
	return Selector == EDwCrewSelector::All ? LOCTEXT("LoseAll", "Your whole crew is lost (not the captain)")
		: FText::Format(LOCTEXT("LoseOne", "{0} is lost for good"), DwEventText::Who(Selector));
}

void UDwEventOutcome_LoseCrew::Validate(TArray<FText>& OutProblems) const
{
	if (Selector == EDwCrewSelector::Captain)
	{
		OutProblems.Add(LOCTEXT("LoseCaptain", "Lose Crew can't take the captain (captain death ends the run in fights only)."));
	}
}

int32 UDwEventOutcome_RandomOf::PickOption(FRandomStream& Random) const
{
	float Total = 0.f;
	for (const FDwEventRandomOption& Option : Options)
	{
		Total += FMath::Max(0.f, Option.Weight);
	}
	if (Total <= 0.f)
	{
		return INDEX_NONE;
	}
	float Roll = Random.FRand() * Total;
	int32 Last = INDEX_NONE;
	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		const float Weight = FMath::Max(0.f, Options[Index].Weight);
		if (Weight <= 0.f)
		{
			continue;
		}
		Last = Index;
		if (Roll < Weight)
		{
			return Index;
		}
		Roll -= Weight;
	}
	return Last; // float rounding at the very top
}

void UDwEventOutcome_RandomOf::Apply(FDwEventContext& Context) const
{
	const int32 Picked = PickOption(Context.Random);
	if (!Options.IsValidIndex(Picked))
	{
		return;
	}
	const int32 Before = Context.Changes.Num();
	for (const UDwEventOutcome* Outcome : Options[Picked].Outcomes)
	{
		if (Outcome)
		{
			Outcome->Apply(Context);
		}
	}
	if (Context.Changes.Num() == Before)
	{
		Context.Changes.Add(LOCTEXT("LuckNothing", "Nothing comes of it"));
	}
}

FText UDwEventOutcome_RandomOf::Describe() const
{
	TArray<FString> Branches;
	float Total = 0.f;
	for (const FDwEventRandomOption& Option : Options)
	{
		Total += FMath::Max(0.f, Option.Weight);
	}
	for (const FDwEventRandomOption& Option : Options)
	{
		TArray<FString> Parts;
		for (const UDwEventOutcome* Outcome : Option.Outcomes)
		{
			if (Outcome)
			{
				Parts.Add(Outcome->Describe().ToString());
			}
		}
		const int32 Pct = Total > 0.f ? FMath::RoundToInt(100.f * FMath::Max(0.f, Option.Weight) / Total) : 0;
		Branches.Add(FString::Printf(TEXT("%d%%: %s"), Pct, Parts.IsEmpty() ? *LOCTEXT("BranchNothing", "nothing").ToString() : *FString::Join(Parts, TEXT(", "))));
	}
	return FText::Format(LOCTEXT("RandomDescribe", "A gamble ({0})"), FText::FromString(FString::Join(Branches, TEXT("; "))));
}

bool UDwEventOutcome_RandomOf::NeedsChosenCrew() const
{
	for (const FDwEventRandomOption& Option : Options)
	{
		for (const UDwEventOutcome* Outcome : Option.Outcomes)
		{
			if (Outcome && Outcome->NeedsChosenCrew())
			{
				return true;
			}
		}
	}
	return false;
}

bool UDwEventOutcome_RandomOf::AcceptsChosen(const FDwCrewMemberState& Member) const
{
	for (const FDwEventRandomOption& Option : Options)
	{
		for (const UDwEventOutcome* Outcome : Option.Outcomes)
		{
			if (Outcome && !Outcome->AcceptsChosen(Member))
			{
				return false;
			}
		}
	}
	return true;
}

void UDwEventOutcome_RandomOf::Validate(TArray<FText>& OutProblems) const
{
	bool bAnyWeight = false;
	for (const FDwEventRandomOption& Option : Options)
	{
		bAnyWeight |= Option.Weight > 0.f;
		for (const UDwEventOutcome* Outcome : Option.Outcomes)
		{
			if (!Outcome)
			{
				OutProblems.Add(LOCTEXT("RandomNull", "Random Of has an empty outcome."));
				continue;
			}
			Outcome->Validate(OutProblems);
		}
	}
	if (Options.Num() < 2 || !bAnyWeight)
	{
		OutProblems.Add(LOCTEXT("RandomOptions", "Random Of needs at least 2 options and a weight above 0."));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Requirements

FText UDwEventRequirement_MinGold::Describe() const
{
	return FText::Format(LOCTEXT("ReqGold", "Requires {0} gold"), Amount);
}

void UDwEventRequirement_MinGold::Validate(TArray<FText>& OutProblems) const
{
	if (Amount <= 0)
	{
		OutProblems.Add(LOCTEXT("ReqGoldZero", "Min Gold requirement with Amount 0 always passes."));
	}
}

FText UDwEventRequirement_CrewCountAtLeast::Describe() const
{
	return FText::Format(LOCTEXT("ReqCrewMin", "Requires {0} or more crew"), Count);
}

FText UDwEventRequirement_CrewCountAtMost::Describe() const
{
	return FText::Format(LOCTEXT("ReqCrewMax", "Requires {0} or fewer crew"), Count);
}

bool UDwEventRequirement_HasItem::IsMet(const FDwRunState& State) const
{
	return Item && State.CountItem(Item->GetFName()) >= Count;
}

FText UDwEventRequirement_HasItem::Describe() const
{
	return FText::Format(LOCTEXT("ReqItem", "Requires {0} {1}"), Count, DwEventText::Name(Item));
}

void UDwEventRequirement_HasItem::Validate(TArray<FText>& OutProblems) const
{
	if (!Item)
	{
		OutProblems.Add(LOCTEXT("ReqItemNone", "Has Item requirement has no Item."));
	}
}

FText UDwEventRequirement_CaptainIs::Describe() const
{
	return FText::Format(LOCTEXT("ReqCaptain", "Only {0}"), DwEventText::Name(Captain));
}

void UDwEventRequirement_CaptainIs::Validate(TArray<FText>& OutProblems) const
{
	if (!Captain)
	{
		OutProblems.Add(LOCTEXT("ReqCaptainNone", "Captain Is requirement has no Captain."));
	}
}

#undef LOCTEXT_NAMESPACE
