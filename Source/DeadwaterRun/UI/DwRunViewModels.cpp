// Copyright Brandon Hillig 2026

#include "UI/DwRunViewModels.h"

#include "Data/DwAbilityDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwPerkDefinition.h"
#include "Run/DwRun.h"

#define LOCTEXT_NAMESPACE "DwRunScreens"

// ---------------------------------------------------------------------------------------------------------------------
// Choice

bool UDwChoiceViewModel::Select()
{
	if (!View.bEnabled || !OnSelect)
	{
		return false;
	}
	// The intent usually refreshes the owning view model, which Set()s this choice again: call a copy, not the
	// function being replaced.
	const TFunction<bool()> Intent = OnSelect;
	return Intent();
}

void UDwChoiceViewModel::Set(const FDwChoiceView& InView, TFunction<bool()> InOnSelect)
{
	View = InView;
	OnSelect = MoveTemp(InOnSelect);
	NotifyChanged();
}

// ---------------------------------------------------------------------------------------------------------------------
// Base

void UDwRunScreenViewModel::Bind(UDwRun* InRun)
{
	Unbind();
	Run = InRun;
	if (InRun)
	{
		InRun->OnStateChanged.AddDynamic(this, &ThisClass::HandleRunChanged);
	}
	Refresh();
}

void UDwRunScreenViewModel::Unbind()
{
	if (UDwRun* Old = Run.Get())
	{
		Old->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleRunChanged);
	}
	Run = nullptr;
}

void UDwRunScreenViewModel::BeginDestroy()
{
	Unbind();
	Super::BeginDestroy();
}

void UDwRunScreenViewModel::HandleRunChanged()
{
	Refresh();
}

UDwChoiceViewModel* UDwRunScreenViewModel::Choice(TArray<TObjectPtr<UDwChoiceViewModel>>& Pool, int32 Index)
{
	while (Pool.Num() <= Index)
	{
		Pool.Add(NewObject<UDwChoiceViewModel>(this));
	}
	return Pool[Index];
}

namespace DwRunScreens
{
	static FText CrewName(const FDwRunState& State, const FGuid& Id)
	{
		const FDwCrewMemberState* Member = State.FindCrew(Id);
		return Member ? Member->GetDisplayName() : FText::GetEmpty();
	}

	static FText NameOf(const UDwDefinitionBase* Definition)
	{
		return Definition ? Definition->DisplayName : LOCTEXT("Unknown", "?");
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Map

const FDwMapNodeView* UDwMapViewModel::FindNode(int32 Index) const
{
	return Nodes.FindByPredicate([Index](const FDwMapNodeView& Node) { return Node.Index == Index; });
}

void UDwMapViewModel::Refresh()
{
	Nodes.Reset();
	Crew.Reset();
	const UDwRun* TheRun = GetRun();
	if (!TheRun)
	{
		NotifyChanged();
		return;
	}
	const FDwRunState& S = TheRun->GetState();
	const TArray<int32> Choices = TheRun->GetChoices();
	const FDwMapNode* Current = TheRun->GetCurrentNode();
	LayerCount = S.Map.LayerCount;
	LaneCount = 0;
	for (int32 Index = 0; Index < S.Map.Nodes.Num(); ++Index)
	{
		const FDwMapNode& Node = S.Map.Nodes[Index];
		FDwMapNodeView& View = Nodes.AddDefaulted_GetRef();
		View.Index = Index;
		View.Layer = Node.Layer;
		View.Lane = Node.Lane;
		View.Type = Node.Type;
		View.Next = Node.Next;
		LaneCount = FMath::Max(LaneCount, Node.Lane + 1);
		View.State = Index == S.CurrentNode ? EDwMapNodeState::Current
			: Node.bVisited ? EDwMapNodeState::Sailed
			: Choices.Contains(Index) ? EDwMapNodeState::Choosable
			: Current && Node.Layer <= Current->Layer ? EDwMapNodeState::Missed
			: EDwMapNodeState::Ahead;
		switch (Node.Type)
		{
		case EDwNodeType::Fight: View.Label = LOCTEXT("Fight", "Fight"); View.Tooltip = LOCTEXT("FightTip", "A pirate crew to fight."); break;
		case EDwNodeType::Shop:  View.Label = LOCTEXT("Shop", "Shop"); View.Tooltip = LOCTEXT("ShopTip", "Repairs, new moves, supplies and crew."); break;
		case EDwNodeType::Event: View.Label = LOCTEXT("Event", "?"); View.Tooltip = LOCTEXT("EventTip", "Something waits out there. Good or bad, you'll see."); break;
		case EDwNodeType::Boss:  View.Label = LOCTEXT("Boss", "Kraken"); View.Tooltip = LOCTEXT("BossTip", "The Kraken. The end of the voyage."); break;
		}
	}

	Gold = S.Gold;
	ShipText = FText::Format(LOCTEXT("Ship", "Hull {0} / {1}"), FMath::RoundToInt(S.Ship.Health), FMath::RoundToInt(S.Ship.MaxHealth));
	Title = Current
		? FText::Format(LOCTEXT("TitleAt", "Sea chart - {0} of {1} stops behind you"), S.Stats.NodesCleared, S.Map.LayerCount)
		: LOCTEXT("TitleStart", "Sea chart - choose your first heading");
	for (const FDwCrewMemberState& Member : S.Crew)
	{
		FDwCrewLineView& Line = Crew.AddDefaulted_GetRef();
		Line.Name = Member.GetDisplayName();
		Line.bAlive = Member.bAlive;
		Line.bIsCaptain = Member.bIsCaptain;
		Line.Detail = Member.bAlive
			? FText::Format(LOCTEXT("CrewDetail", "L{0}   {1} / {2} HP"), Member.Level, FMath::RoundToInt(Member.Health), FMath::RoundToInt(Member.GetMaxHealth()))
			: LOCTEXT("CrewLost", "lost");
	}
	NotifyChanged();
}

bool UDwMapViewModel::CanChoose(int32 NodeIndex, FText* OutReason) const
{
	const UDwRun* TheRun = GetRun();
	const FDwMapNodeView* Node = FindNode(NodeIndex);
	FText Reason;
	if (!TheRun || !Node)
	{
		Reason = LOCTEXT("NoNode", "No such route.");
	}
	else if (TheRun->GetPhase() != EDwRunPhase::Map)
	{
		Reason = LOCTEXT("NotOnMap", "Finish this stop first.");
	}
	else
	{
		switch (Node->State)
		{
		case EDwMapNodeState::Choosable: break;
		case EDwMapNodeState::Current:   Reason = LOCTEXT("Here", "You are here."); break;
		case EDwMapNodeState::Sailed:    Reason = LOCTEXT("Sailed", "Already sailed."); break;
		case EDwMapNodeState::Missed:    Reason = LOCTEXT("Missed", "Left behind: the voyage only goes forward."); break;
		case EDwMapNodeState::Ahead:     Reason = LOCTEXT("Ahead", "Too far: sail to one of the routes next to you."); break;
		}
	}
	if (OutReason)
	{
		*OutReason = Reason;
	}
	return Reason.IsEmpty();
}

bool UDwMapViewModel::ChooseNode(int32 NodeIndex)
{
	UDwRun* TheRun = GetRun();
	return CanChoose(NodeIndex) && TheRun && TheRun->ChooseNode(NodeIndex);
}

// ---------------------------------------------------------------------------------------------------------------------
// Rewards

int32 UDwRewardsViewModel::MaxSlots() const
{
	const UDwRun* TheRun = GetRun();
	return TheRun ? TheRun->GetMaxAbilitySlots() : 0;
}

const FDwCrewMemberState* UDwRewardsViewModel::SelectedLearner() const
{
	const UDwRun* TheRun = GetRun();
	return TheRun && SelectedCrew.IsValid() ? TheRun->GetState().FindCrew(SelectedCrew) : nullptr;
}

FText UDwRewardsViewModel::GetClaimBlocker() const
{
	const UDwRun* TheRun = GetRun();
	if (!TheRun || TheRun->GetPhase() != EDwRunPhase::Rewards)
	{
		return LOCTEXT("NoRewards", "No rewards waiting.");
	}
	const TArray<FDwAbilityOffer>& Choices = TheRun->GetState().PendingRewards.AbilityChoices;
	if (!Choices.IsValidIndex(SelectedOffer))
	{
		return FText::GetEmpty(); // no new move: always fine
	}
	const FDwCrewMemberState* Learner = SelectedLearner();
	if (!Learner)
	{
		return FText::Format(LOCTEXT("PickLearner", "Pick who learns {0}."), DwRunScreens::NameOf(Choices[SelectedOffer].Ability));
	}
	const int32 Slots = MaxSlots();
	if (Slots > 0 && Learner->Abilities.Num() >= Slots && !Learner->Abilities.IsValidIndex(SelectedSlot))
	{
		return FText::Format(LOCTEXT("PickForget", "{0} knows {1} moves already: pick one to forget."), Learner->GetDisplayName(), Slots);
	}
	return FText::GetEmpty();
}

bool UDwRewardsViewModel::SelectOffer(int32 Index)
{
	const UDwRun* TheRun = GetRun();
	if (!TheRun || !TheRun->GetState().PendingRewards.AbilityChoices.IsValidIndex(Index))
	{
		return false;
	}
	SelectedOffer = SelectedOffer == Index ? INDEX_NONE : Index; // a second click skips the move again
	SelectedCrew.Invalidate();
	SelectedSlot = INDEX_NONE;
	if (SelectedOffer != INDEX_NONE)
	{
		const TArray<FGuid>& Eligible = TheRun->GetState().PendingRewards.AbilityChoices[Index].EligibleCrew;
		if (Eligible.Num() == 1)
		{
			SelectedCrew = Eligible[0]; // nobody else can learn it
		}
	}
	Refresh();
	return true;
}

bool UDwRewardsViewModel::SelectLearner(int32 Index)
{
	const UDwRun* TheRun = GetRun();
	const TArray<FDwAbilityOffer>& Choices = TheRun ? TheRun->GetState().PendingRewards.AbilityChoices : TArray<FDwAbilityOffer>();
	if (!Choices.IsValidIndex(SelectedOffer) || !Choices[SelectedOffer].EligibleCrew.IsValidIndex(Index))
	{
		return false;
	}
	SelectedCrew = Choices[SelectedOffer].EligibleCrew[Index];
	SelectedSlot = INDEX_NONE;
	Refresh();
	return true;
}

bool UDwRewardsViewModel::SelectForget(int32 Index)
{
	const FDwCrewMemberState* Learner = SelectedLearner();
	if (!Learner || !Learner->Abilities.IsValidIndex(Index))
	{
		return false;
	}
	SelectedSlot = Index;
	Refresh();
	return true;
}

bool UDwRewardsViewModel::ClaimRewards()
{
	UDwRun* TheRun = GetRun();
	if (!TheRun || !GetClaimBlocker().IsEmpty())
	{
		return false;
	}
	const bool bAbility = TheRun->GetState().PendingRewards.AbilityChoices.IsValidIndex(SelectedOffer);
	const int32 Offer = bAbility ? SelectedOffer : INDEX_NONE;
	const FGuid Crew = bAbility ? SelectedCrew : FGuid();
	const int32 Slot = bAbility ? SelectedSlot : INDEX_NONE;
	SelectedOffer = INDEX_NONE;
	SelectedCrew.Invalidate();
	SelectedSlot = INDEX_NONE;
	return TheRun->ClaimRewards(Offer, Crew, Slot);
}

void UDwRewardsViewModel::Refresh()
{
	Lines.Reset();
	const UDwRun* TheRun = GetRun();
	const bool bActive = TheRun && TheRun->GetPhase() == EDwRunPhase::Rewards;
	const FDwRunState* S = TheRun ? &TheRun->GetState() : nullptr;
	if (!bActive || BundleNode != S->CurrentNode)
	{
		SelectedOffer = INDEX_NONE;
		SelectedCrew.Invalidate();
		SelectedSlot = INDEX_NONE;
		BundleNode = bActive ? S->CurrentNode : INDEX_NONE;
	}

	int32 OfferCount = 0, LearnerCount = 0, ForgetCount = 0;
	if (bActive)
	{
		const FDwRewardBundle& R = S->PendingRewards;
		switch (R.Outcome)
		{
		case EDwCombatOutcome::VictorySunk:         Headline = LOCTEXT("Sunk", "Their ship is sunk"); break;
		case EDwCombatOutcome::VictoryCrewDefeated: Headline = LOCTEXT("Plunder", "Their crew is beaten - plunder!"); break;
		case EDwCombatOutcome::VictorySurrender:    Headline = LOCTEXT("Surrender", "They surrendered - plunder!"); break;
		default:                                    Headline = LOCTEXT("Victory", "Victory"); break;
		}
		Lines.Add(FText::Format(LOCTEXT("Gold", "+{0} gold"), R.Gold));
		for (const FDwItemStack& Stack : R.Items)
		{
			Lines.Add(Stack.Count > 1 ? FText::Format(LOCTEXT("Items", "+ {0} x{1}"), DwRunScreens::NameOf(Stack.Item), Stack.Count)
				: FText::Format(LOCTEXT("Item", "+ {0}"), DwRunScreens::NameOf(Stack.Item)));
		}
		for (const FDwXpGain& Gain : R.Xp)
		{
			const FText Name = DwRunScreens::CrewName(*S, Gain.CrewId);
			Lines.Add(Gain.LevelsGained == 0 ? FText::Format(LOCTEXT("Xp", "{0} +{1} XP"), Name, Gain.Xp)
				: Gain.LevelsGained == 1 ? FText::Format(LOCTEXT("XpLevel", "{0} +{1} XP - level up!"), Name, Gain.Xp)
				: FText::Format(LOCTEXT("XpLevels", "{0} +{1} XP - {2} levels up!"), Name, Gain.Xp, Gain.LevelsGained));
		}

		// A new move: the offers, then who learns the picked one, then what they forget if they're full (P4, P5).
		for (int32 Index = 0; Index < R.AbilityChoices.Num(); ++Index)
		{
			const UDwAbilityDefinition* Ability = R.AbilityChoices[Index].Ability;
			FDwChoiceView View;
			View.Title = DwRunScreens::NameOf(Ability);
			View.Body = Ability ? Ability->Description : FText::GetEmpty();
			View.bSelected = Index == SelectedOffer;
			Choice(Offers, OfferCount++)->Set(View, [this, Index]() { return SelectOffer(Index); });
		}
		if (R.AbilityChoices.IsValidIndex(SelectedOffer))
		{
			const TArray<FGuid>& Eligible = R.AbilityChoices[SelectedOffer].EligibleCrew;
			for (int32 Index = 0; Index < Eligible.Num(); ++Index)
			{
				const FDwCrewMemberState* Member = S->FindCrew(Eligible[Index]);
				FDwChoiceView View;
				View.Title = DwRunScreens::CrewName(*S, Eligible[Index]);
				View.Body = Member ? FText::Format(LOCTEXT("LearnerBody", "Level {0}, knows {1} moves"), Member->Level, Member->Abilities.Num()) : FText::GetEmpty();
				View.bSelected = Eligible[Index] == SelectedCrew;
				Choice(Learners, LearnerCount++)->Set(View, [this, Index]() { return SelectLearner(Index); });
			}
		}
		const FDwCrewMemberState* Learner = SelectedLearner();
		if (Learner && MaxSlots() > 0 && Learner->Abilities.Num() >= MaxSlots())
		{
			for (int32 Index = 0; Index < Learner->Abilities.Num(); ++Index)
			{
				FDwChoiceView View;
				View.Title = FText::Format(LOCTEXT("Forget", "Forget {0}"), DwRunScreens::NameOf(Learner->Abilities[Index]));
				View.bSelected = Index == SelectedSlot;
				Choice(Forget, ForgetCount++)->Set(View, [this, Index]() { return SelectForget(Index); });
			}
		}
		NoOffersText = R.AbilityChoices.IsEmpty() ? LOCTEXT("NoOffers", "No new moves to learn this time.") : LOCTEXT("PickOffer", "Learn a new move? (optional)");
	}
	Offers.SetNum(OfferCount);
	Learners.SetNum(LearnerCount);
	Forget.SetNum(ForgetCount);

	if (!Claim)
	{
		Claim = NewObject<UDwChoiceViewModel>(this);
	}
	FDwChoiceView ClaimView;
	const bool bLearning = bActive && S->PendingRewards.AbilityChoices.IsValidIndex(SelectedOffer);
	ClaimView.Title = bLearning
		? FText::Format(LOCTEXT("ClaimLearn", "Take the rewards and learn {0}"), DwRunScreens::NameOf(S->PendingRewards.AbilityChoices[SelectedOffer].Ability))
		: LOCTEXT("Claim", "Take the rewards");
	ClaimView.DisabledReason = GetClaimBlocker();
	ClaimView.bEnabled = ClaimView.DisabledReason.IsEmpty();
	Claim->Set(ClaimView, [this]() { return ClaimRewards(); });
	NotifyChanged();
}

// ---------------------------------------------------------------------------------------------------------------------
// Level-up

void UDwLevelUpViewModel::Refresh()
{
	UDwRun* TheRun = GetRun();
	const FDwPendingLevelUp* LevelUp = TheRun && TheRun->GetPhase() == EDwRunPhase::LevelUp ? TheRun->GetCurrentLevelUp() : nullptr;
	int32 CardCount = 0;
	if (LevelUp)
	{
		const FDwRunState& S = TheRun->GetState();
		Headline = FText::Format(LOCTEXT("LevelUp", "{0} reaches level {1}!"), DwRunScreens::CrewName(S, LevelUp->CrewId), LevelUp->NewLevel);
		TArray<FString> Gains;
		const FDwLevelStats& Gain = LevelUp->StatGain;
		if (Gain.MaxHealth) { Gains.Add(FText::Format(LOCTEXT("GainHp", "+{0} max HP"), Gain.MaxHealth).ToString()); }
		if (Gain.MaxMana) { Gains.Add(FText::Format(LOCTEXT("GainMp", "+{0} max MP"), Gain.MaxMana).ToString()); }
		if (Gain.AttackDamage) { Gains.Add(FText::Format(LOCTEXT("GainAttack", "+{0} attack"), Gain.AttackDamage).ToString()); }
		StatText = Gains.IsEmpty() ? LOCTEXT("NoGain", "No stat gain at this level.") : FText::FromString(FString::Join(Gains, TEXT("   ")));
		const int32 More = S.PendingLevelUps.Num() - 1;
		QueueText = More > 0 ? FText::Format(LOCTEXT("More", "{0} more level-up(s) after this one"), More) : FText::GetEmpty();
		for (int32 Index = 0; Index < LevelUp->PerkChoices.Num(); ++Index)
		{
			const UDwPerkDefinition* Perk = LevelUp->PerkChoices[Index];
			FDwChoiceView View;
			View.Title = DwRunScreens::NameOf(Perk);
			View.Body = Perk ? Perk->GetFormattedDescription() : FText::GetEmpty();
			Choice(Cards, CardCount++)->Set(View, [this, Index]() { UDwRun* R = GetRun(); return R && R->ChoosePerk(Index); });
		}
		if (LevelUp->PerkChoices.IsEmpty())
		{
			FDwChoiceView View;
			View.Title = LOCTEXT("Continue", "Continue");
			View.Body = LOCTEXT("NoPerks", "No perks left to offer.");
			Choice(Cards, CardCount++)->Set(View, [this]() { UDwRun* R = GetRun(); return R && R->ChoosePerk(INDEX_NONE); });
		}
	}
	else
	{
		Headline = StatText = QueueText = FText::GetEmpty();
	}
	Cards.SetNum(CardCount);
	NotifyChanged();
}

// ---------------------------------------------------------------------------------------------------------------------
// Shop and event shells

void UDwNodeShellViewModel::Refresh()
{
	UDwRun* TheRun = GetRun();
	const EDwRunPhase Phase = TheRun ? TheRun->GetPhase() : EDwRunPhase::Map;
	if (!Leave)
	{
		Leave = NewObject<UDwChoiceViewModel>(this);
	}
	FDwChoiceView View;
	if (Phase == EDwRunPhase::Shop)
	{
		Headline = LOCTEXT("ShopTitle", "Shop");
		Body = FText::Format(LOCTEXT("ShopBody", "You have {0} gold.\nThe shopkeeper is still unpacking (shop: DW-204)."), TheRun->GetState().Gold);
		View.Title = LOCTEXT("LeaveShop", "Leave the shop");
		Leave->Set(View, [this]() { UDwRun* R = GetRun(); return R && R->LeaveShop(); });
	}
	else if (Phase == EDwRunPhase::Event)
	{
		Headline = LOCTEXT("EventTitle", "Something on the horizon");
		Body = LOCTEXT("EventBody", "Nothing happens... yet (events: DW-205).");
		View.Title = LOCTEXT("LeaveEvent", "Sail on");
		Leave->Set(View, [this]() { UDwRun* R = GetRun(); return R && R->LeaveEvent(); });
	}
	else
	{
		Headline = Body = FText::GetEmpty();
		View.bEnabled = false;
		View.DisabledReason = LOCTEXT("NotHere", "Not at a shop or event.");
		Leave->Set(View, nullptr);
	}
	NotifyChanged();
}

#undef LOCTEXT_NAMESPACE
