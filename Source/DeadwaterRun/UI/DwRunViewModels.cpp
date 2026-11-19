// Copyright Brandon Hillig 2026

#include "UI/DwRunViewModels.h"

#include "Data/DwAbilityDefinition.h"
#include "Data/DwEventDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwPerkDefinition.h"
#include "Data/DwShipUpgradeDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Run/DwRun.h"
#include "Run/Events/DwEventOutcome.h"
#include "Run/Events/DwEventResolver.h"

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

	/** The run header's crew lines (map and shop). */
	static void CrewLines(const FDwRunState& State, TArray<FDwCrewLineView>& Out)
	{
		Out.Reset();
		for (const FDwCrewMemberState& Member : State.Crew)
		{
			FDwCrewLineView& Line = Out.AddDefaulted_GetRef();
			Line.Name = Member.GetDisplayName();
			Line.bAlive = Member.bAlive;
			Line.bIsCaptain = Member.bIsCaptain;
			Line.Detail = Member.bAlive
				? FText::Format(LOCTEXT("CrewDetail", "L{0}   {1} / {2} HP"), Member.Level, FMath::RoundToInt(Member.Health), FMath::RoundToInt(Member.GetMaxHealth()))
				: LOCTEXT("CrewLost", "lost");
		}
	}

	static FText ShipLine(const FDwRunState& State)
	{
		return FText::Format(LOCTEXT("Ship", "Hull {0} / {1}"), FMath::RoundToInt(State.Ship.Health), FMath::RoundToInt(State.Ship.MaxHealth));
	}

	/** "<price> gold" and, when there is one, the description on the next line. */
	static FText PriceAndText(int32 Price, const FText& Text)
	{
		return Text.IsEmpty() ? FText::Format(LOCTEXT("Price", "{0} gold"), Price)
			: FText::Format(LOCTEXT("PriceText", "{0} gold\n{1}"), Price, Text);
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
	ShipText = DwRunScreens::ShipLine(S);
	Title = Current
		? FText::Format(LOCTEXT("TitleAt", "Sea chart - {0} of {1} stops behind you"), S.Stats.NodesCleared, S.Map.LayerCount)
		: LOCTEXT("TitleStart", "Sea chart - choose your first heading");
	DwRunScreens::CrewLines(S, Crew);
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
		case EDwCombatOutcome::None:                Headline = LOCTEXT("EventLearn", "Something to learn"); break; // an event's Ability Choice
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
// Shop

FText UDwShopViewModel::DescribeResult(EDwPurchaseResult Result, const UDwRun& Run, const FDwShopOffer* Offer, const FGuid& Crew)
{
	const FDwRunState& S = Run.GetState();
	const FText Who = DwRunScreens::CrewName(S, Crew);
	switch (Result)
	{
	case EDwPurchaseResult::Ok:            return FText::GetEmpty();
	case EDwPurchaseResult::NoOffer:       return LOCTEXT("NoOffer", "Not for sale here.");
	case EDwPurchaseResult::SoldOut:       return Offer && Offer->Type == EDwShopOfferType::Disband ? LOCTEXT("Gone", "Already gone.") : LOCTEXT("SoldOut", "Sold out.");
	case EDwPurchaseResult::InvalidTarget: return Who.IsEmpty() ? LOCTEXT("NoLearner", "Nobody aboard can learn it.") : FText::Format(LOCTEXT("CantLearn", "{0} can't learn it."), Who);
	case EDwPurchaseResult::AlreadyKnown:  return FText::Format(LOCTEXT("Known", "{0} knows it already."), Who);
	case EDwPurchaseResult::NoSlot:        return FText::Format(LOCTEXT("NoSlot", "{0} knows {1} moves already: pick one to forget."), Who, Run.GetMaxAbilitySlots());
	case EDwPurchaseResult::CantDisband:   return LOCTEXT("CantDisband", "The captain keeps at least one crew member.");
	case EDwPurchaseResult::CrewFull:      return LOCTEXT("CrewFull", "No room aboard: the crew is full.");
	case EDwPurchaseResult::ShipFull:      return LOCTEXT("ShipFull", "The hull is already sound.");
	case EDwPurchaseResult::InventoryFull: return FText::Format(LOCTEXT("HoldFull", "The hold is full ({0} / {1})."), S.CountItems(), Run.GetMaxInventory());
	case EDwPurchaseResult::NotEnoughGold: return FText::Format(LOCTEXT("NeedsGold", "Needs {0} gold - you have {1}."), Offer ? Offer->Price : 0, S.Gold);
	}
	return LOCTEXT("Refused", "Can't buy this now.");
}

const FDwShopOffer* UDwShopViewModel::FindOffer(int32 OfferId) const
{
	const UDwRun* TheRun = GetRun();
	return TheRun ? TheRun->GetShop().Offers.FindByPredicate([OfferId](const FDwShopOffer& Offer) { return Offer.OfferId == OfferId; }) : nullptr;
}

void UDwShopViewModel::ClearPick()
{
	SelectedMove = INDEX_NONE;
	SelectedCrew.Invalidate();
	SelectedSlot = INDEX_NONE;
	bMovePicked = false;
}

EDwPurchaseResult UDwShopViewModel::CheckMoveCard(const FDwShopOffer& Offer, FGuid& OutCrew) const
{
	const UDwRun* TheRun = GetRun();
	OutCrew.Invalidate();
	if (!TheRun)
	{
		return EDwPurchaseResult::NoOffer;
	}
	if (Offer.EligibleCrew.IsEmpty())
	{
		return TheRun->CanPurchase(Offer.OfferId, FGuid()); // sold out / nobody can learn it, in the rules' order
	}
	// Any slot will do for the card: which move to forget is picked later.
	const int32 Slots = TheRun->GetMaxAbilitySlots();
	EDwPurchaseResult First = EDwPurchaseResult::InvalidTarget;
	for (int32 Index = 0; Index < Offer.EligibleCrew.Num(); ++Index)
	{
		const FDwCrewMemberState* Member = TheRun->GetState().FindCrew(Offer.EligibleCrew[Index]);
		const int32 Slot = Member && Slots > 0 && Member->Abilities.Num() >= Slots ? 0 : INDEX_NONE;
		const EDwPurchaseResult Result = TheRun->CanPurchase(Offer.OfferId, Offer.EligibleCrew[Index], Slot);
		if (Result == EDwPurchaseResult::Ok)
		{
			OutCrew = Offer.EligibleCrew[Index];
			return Result;
		}
		if (Index == 0)
		{
			First = Result;
			OutCrew = Offer.EligibleCrew[Index];
		}
	}
	return First;
}

EDwPurchaseResult UDwShopViewModel::CheckMove(FText& OutReason) const
{
	const UDwRun* TheRun = GetRun();
	const FDwShopOffer* Offer = bMovePicked ? FindOffer(SelectedMove) : nullptr;
	if (!TheRun || !Offer)
	{
		OutReason = LOCTEXT("PickMove", "Pick a move.");
		return EDwPurchaseResult::NoOffer;
	}
	const FDwCrewMemberState* Learner = SelectedCrew.IsValid() ? TheRun->GetState().FindCrew(SelectedCrew) : nullptr;
	if (!Learner)
	{
		OutReason = FText::Format(LOCTEXT("PickShopLearner", "Pick who learns {0}."), DwRunScreens::NameOf(Offer->Ability));
		return EDwPurchaseResult::InvalidTarget;
	}
	const EDwPurchaseResult Result = TheRun->CanPurchase(Offer->OfferId, SelectedCrew, SelectedSlot);
	OutReason = DescribeResult(Result, *TheRun, Offer, SelectedCrew);
	return Result;
}

bool UDwShopViewModel::Purchase(int32 OfferId, const FGuid& CrewId, int32 Slot)
{
	UDwRun* TheRun = GetRun();
	if (!TheRun)
	{
		return false;
	}
	// Checked first so a refusal never reaches TryPurchase (it would log); the count moves before TryPurchase's
	// broadcast so the screen sees this purchase in the refresh it triggers.
	LastResult = TheRun->CanPurchase(OfferId, CrewId, Slot);
	++PurchaseCount;
	if (LastResult != EDwPurchaseResult::Ok)
	{
		NotifyChanged();
		return false;
	}
	return TheRun->TryPurchase(OfferId, CrewId, Slot) == EDwPurchaseResult::Ok;
}

bool UDwShopViewModel::Buy(int32 OfferId)
{
	const FDwShopOffer* Offer = FindOffer(OfferId);
	return Offer && Offer->Type != EDwShopOfferType::Ability && Purchase(OfferId, FGuid(), INDEX_NONE);
}

bool UDwShopViewModel::SelectMove(int32 OfferId)
{
	const FDwShopOffer* Offer = FindOffer(OfferId);
	FGuid Learner;
	if (!Offer || Offer->Type != EDwShopOfferType::Ability || CheckMoveCard(*Offer, Learner) != EDwPurchaseResult::Ok)
	{
		return false;
	}
	const bool bDrop = bMovePicked && SelectedMove == OfferId; // a second click drops the pick
	ClearPick();
	if (!bDrop)
	{
		bMovePicked = true;
		SelectedMove = OfferId;
		if (Offer->EligibleCrew.Num() == 1)
		{
			SelectedCrew = Offer->EligibleCrew[0]; // nobody else can learn it
		}
	}
	Refresh();
	return true;
}

bool UDwShopViewModel::SelectLearner(int32 Index)
{
	const FDwShopOffer* Offer = bMovePicked ? FindOffer(SelectedMove) : nullptr;
	if (!Offer || !Offer->EligibleCrew.IsValidIndex(Index))
	{
		return false;
	}
	SelectedCrew = Offer->EligibleCrew[Index];
	SelectedSlot = INDEX_NONE;
	Refresh();
	return true;
}

bool UDwShopViewModel::SelectForget(int32 Index)
{
	const UDwRun* TheRun = GetRun();
	const FDwCrewMemberState* Learner = TheRun && bMovePicked && SelectedCrew.IsValid() ? TheRun->GetState().FindCrew(SelectedCrew) : nullptr;
	if (!Learner || !Learner->Abilities.IsValidIndex(Index))
	{
		return false;
	}
	SelectedSlot = Index;
	Refresh();
	return true;
}

bool UDwShopViewModel::BuySelectedMove()
{
	FText Reason;
	if (CheckMove(Reason) != EDwPurchaseResult::Ok)
	{
		return false;
	}
	// Bought: the offer sells out, and the refresh TryPurchase triggers drops the pick.
	return Purchase(SelectedMove, SelectedCrew, SelectedSlot);
}

void UDwShopViewModel::Refresh()
{
	UDwRun* TheRun = GetRun();
	const bool bActive = TheRun && TheRun->GetPhase() == EDwRunPhase::Shop && TheRun->GetShop().IsOpen();
	const FDwShopInventory* Shop = bActive ? &TheRun->GetShop() : nullptr;
	if (!bActive || ShopNode != Shop->Node)
	{
		ClearPick();
		LastResult = EDwPurchaseResult::Ok;
		PurchaseCount = 0;
		ShopNode = bActive ? Shop->Node : INDEX_NONE;
	}

	int32 SupplyCount = 0, MoveCount = 0, LearnerCount = 0, ForgetCount = 0, RecruitCount = 0, UpgradeCount = 0, DisbandCount = 0;
	Crew.Reset();
	MoveHint = FText::GetEmpty();
	if (bActive)
	{
		const FDwRunState& S = TheRun->GetState();
		Gold = S.Gold;
		ShipText = DwRunScreens::ShipLine(S);
		const int32 MaxInventory = TheRun->GetMaxInventory();
		HoldText = MaxInventory > 0 ? FText::Format(LOCTEXT("Hold", "Hold {0} / {1}"), S.CountItems(), MaxInventory)
			: FText::Format(LOCTEXT("HoldNoCap", "Hold {0}"), S.CountItems());
		DwRunScreens::CrewLines(S, Crew);

		// A picked move that sold out, or a learner who can no longer learn it, is dropped.
		if (bMovePicked)
		{
			const FDwShopOffer* Picked = FindOffer(SelectedMove);
			FGuid Unused;
			if (!Picked || Picked->Type != EDwShopOfferType::Ability || CheckMoveCard(*Picked, Unused) != EDwPurchaseResult::Ok)
			{
				ClearPick();
			}
			else if (SelectedCrew.IsValid() && !Picked->EligibleCrew.Contains(SelectedCrew))
			{
				SelectedCrew.Invalidate();
				SelectedSlot = INDEX_NONE;
			}
		}

		auto Priced = [this, TheRun](FDwChoiceView& View, const FDwShopOffer& Offer, EDwPurchaseResult Result)
		{
			View.bEnabled = Result == EDwPurchaseResult::Ok;
			View.DisabledReason = DescribeResult(Result, *TheRun, &Offer, FGuid());
			View.SelectSound = DwTags::UI_Sound_Purchase;
		};
		for (const FDwShopOffer& Offer : Shop->Offers)
		{
			const int32 Id = Offer.OfferId;
			FDwChoiceView View;
			switch (Offer.Type)
			{
			case EDwShopOfferType::Repair:
				View.Title = LOCTEXT("Repair", "Repair the hull");
				View.Body = FText::Format(LOCTEXT("RepairBody", "{0} gold\n+{1} hull"), Offer.Price, Offer.Amount);
				Priced(View, Offer, TheRun->CanPurchase(Id, FGuid()));
				Choice(Supplies, SupplyCount++)->Set(View, [this, Id]() { return Buy(Id); });
				break;
			case EDwShopOfferType::Item:
				View.Title = DwRunScreens::NameOf(Offer.Item);
				View.Body = DwRunScreens::PriceAndText(Offer.Price, Offer.Item ? Offer.Item->Description : FText::GetEmpty());
				Priced(View, Offer, TheRun->CanPurchase(Id, FGuid()));
				Choice(Supplies, SupplyCount++)->Set(View, [this, Id]() { return Buy(Id); });
				break;
			case EDwShopOfferType::Ability:
			{
				FGuid Learner;
				const EDwPurchaseResult Result = CheckMoveCard(Offer, Learner);
				TArray<FString> Names;
				for (const FGuid& Eligible : Offer.EligibleCrew)
				{
					Names.Add(DwRunScreens::CrewName(S, Eligible).ToString());
				}
				const FText Who = Names.IsEmpty() ? LOCTEXT("Nobody", "nobody aboard can learn it")
					: FText::Format(LOCTEXT("WhoLearns", "for {0}"), FText::FromString(FString::Join(Names, TEXT(", "))));
				View.Title = DwRunScreens::NameOf(Offer.Ability);
				View.Body = FText::Format(LOCTEXT("MoveBody", "{0} gold, {1}\n{2}"), Offer.Price, Who, Offer.Ability ? Offer.Ability->Description : FText::GetEmpty());
				View.bSelected = bMovePicked && Id == SelectedMove;
				View.bEnabled = Result == EDwPurchaseResult::Ok;
				View.DisabledReason = DescribeResult(Result, *TheRun, &Offer, Offer.EligibleCrew.Num() == 1 ? Learner : FGuid());
				Choice(Moves, MoveCount++)->Set(View, [this, Id]() { return SelectMove(Id); });
				break;
			}
			case EDwShopOfferType::Recruit:
				View.Title = FText::Format(LOCTEXT("Recruit", "{0}, level {1}"), DwRunScreens::NameOf(Offer.Unit), Offer.Level);
				View.Body = DwRunScreens::PriceAndText(Offer.Price, Offer.Unit ? Offer.Unit->Description : FText::GetEmpty());
				Priced(View, Offer, TheRun->CanPurchase(Id, FGuid()));
				Choice(Recruits, RecruitCount++)->Set(View, [this, Id]() { return Buy(Id); });
				break;
			case EDwShopOfferType::ShipUpgrade:
				View.Title = DwRunScreens::NameOf(Offer.Upgrade);
				View.Body = DwRunScreens::PriceAndText(Offer.Price, Offer.Upgrade ? Offer.Upgrade->GetFormattedDescription() : FText::GetEmpty());
				Priced(View, Offer, TheRun->CanPurchase(Id, FGuid()));
				Choice(Upgrades, UpgradeCount++)->Set(View, [this, Id]() { return Buy(Id); });
				break;
			case EDwShopOfferType::Disband:
			{
				if (Offer.bSoldOut)
				{
					break; // they left: nothing to show
				}
				const FText Name = DwRunScreens::CrewName(S, Offer.CrewId);
				View.Title = FText::Format(LOCTEXT("Disband", "Disband {0}"), Name);
				View.Body = Offer.Amount > 0 ? FText::Format(LOCTEXT("Refund", "Refund {0} gold"), Offer.Amount) : LOCTEXT("NoRefund", "No refund");
				Priced(View, Offer, TheRun->CanPurchase(Id, FGuid()));
				View.SelectSound = DwTags::UI_Sound_Confirm;
				View.ConfirmTitle = FText::Format(LOCTEXT("DisbandTitle", "Disband {0}?"), Name);
				View.ConfirmBody = LOCTEXT("DisbandBody", "They leave the crew for good, with their level and their moves.");
				View.ConfirmYes = LOCTEXT("DisbandYes", "Disband");
				Choice(Disbands, DisbandCount++)->Set(View, [this, Id]() { return Buy(Id); });
				break;
			}
			}
		}

		// The move pick: who learns it, then what they forget.
		const FDwShopOffer* Picked = bMovePicked ? FindOffer(SelectedMove) : nullptr;
		const int32 Slots = TheRun->GetMaxAbilitySlots();
		if (Picked)
		{
			for (int32 Index = 0; Index < Picked->EligibleCrew.Num(); ++Index)
			{
				const FGuid& Id = Picked->EligibleCrew[Index];
				const FDwCrewMemberState* Member = S.FindCrew(Id);
				const bool bFull = Member && Slots > 0 && Member->Abilities.Num() >= Slots;
				FDwChoiceView View;
				View.Title = DwRunScreens::CrewName(S, Id);
				View.Body = Member ? (Slots > 0 ? FText::Format(LOCTEXT("LearnerSlots", "L{0}, knows {1} / {2} moves"), Member->Level, Member->Abilities.Num(), Slots)
					: FText::Format(LOCTEXT("ShopLearnerBody", "L{0}, knows {1} moves"), Member->Level, Member->Abilities.Num())) : FText::GetEmpty();
				View.bSelected = Id == SelectedCrew;
				const EDwPurchaseResult Result = TheRun->CanPurchase(Picked->OfferId, Id, bFull ? 0 : INDEX_NONE);
				View.bEnabled = Result == EDwPurchaseResult::Ok;
				View.DisabledReason = DescribeResult(Result, *TheRun, Picked, Id);
				Choice(Learners, LearnerCount++)->Set(View, [this, Index]() { return SelectLearner(Index); });
			}
			const FDwCrewMemberState* Learner = SelectedCrew.IsValid() ? S.FindCrew(SelectedCrew) : nullptr;
			if (Learner && Slots > 0 && Learner->Abilities.Num() >= Slots)
			{
				for (int32 Index = 0; Index < Learner->Abilities.Num(); ++Index)
				{
					FDwChoiceView View;
					View.Title = FText::Format(LOCTEXT("ShopForget", "Forget {0}"), DwRunScreens::NameOf(Learner->Abilities[Index]));
					View.bSelected = Index == SelectedSlot;
					Choice(Forget, ForgetCount++)->Set(View, [this, Index]() { return SelectForget(Index); });
				}
			}
		}

		if (!BuyMove)
		{
			BuyMove = NewObject<UDwChoiceViewModel>(this);
		}
		FDwChoiceView BuyView;
		FText Blocker;
		const EDwPurchaseResult MoveResult = CheckMove(Blocker);
		if (Picked)
		{
			const FDwCrewMemberState* Learner = SelectedCrew.IsValid() ? S.FindCrew(SelectedCrew) : nullptr;
			BuyView.Title = Learner
				? FText::Format(LOCTEXT("Teach", "Teach {0}"), DwRunScreens::NameOf(Picked->Ability))
				: FText::Format(LOCTEXT("BuyMove", "Buy {0}"), DwRunScreens::NameOf(Picked->Ability));
			BuyView.Body = Learner && Learner->Abilities.IsValidIndex(SelectedSlot)
				? FText::Format(LOCTEXT("TeachForget", "To {0}, {1} gold, forgets {2}"), Learner->GetDisplayName(), Picked->Price, DwRunScreens::NameOf(Learner->Abilities[SelectedSlot]))
				: Learner ? FText::Format(LOCTEXT("TeachTo", "To {0}, {1} gold"), Learner->GetDisplayName(), Picked->Price)
				: FText::Format(LOCTEXT("Price", "{0} gold"), Picked->Price);
			MoveHint = MoveResult == EDwPurchaseResult::Ok ? LOCTEXT("ReadyToTeach", "Ready: buy to teach it now.") : Blocker;
		}
		else
		{
			MoveHint = MoveCount > 0 ? LOCTEXT("MovesHint", "Pick a move, then who learns it.") : LOCTEXT("NoMoves", "No new moves for sale here.");
		}
		BuyView.bEnabled = MoveResult == EDwPurchaseResult::Ok;
		BuyView.DisabledReason = BuyView.bEnabled ? FText::GetEmpty() : Blocker;
		BuyView.SelectSound = DwTags::UI_Sound_Purchase;
		BuyMove->Set(BuyView, [this]() { return BuySelectedMove(); });
	}
	else
	{
		Gold = 0;
		ShipText = HoldText = FText::GetEmpty();
	}
	Supplies.SetNum(SupplyCount);
	Moves.SetNum(MoveCount);
	Learners.SetNum(LearnerCount);
	Forget.SetNum(ForgetCount);
	Recruits.SetNum(RecruitCount);
	Upgrades.SetNum(UpgradeCount);
	Disbands.SetNum(DisbandCount);

	if (!Leave)
	{
		Leave = NewObject<UDwChoiceViewModel>(this);
	}
	FDwChoiceView LeaveView;
	LeaveView.Title = LOCTEXT("LeaveShop", "Leave the shop");
	LeaveView.bEnabled = bActive;
	LeaveView.DisabledReason = bActive ? FText::GetEmpty() : LOCTEXT("NoShop", "No shop here.");
	Leave->Set(LeaveView, [this]() { UDwRun* R = GetRun(); return R && R->GetPhase() == EDwRunPhase::Shop && R->LeaveShop(); });
	NotifyChanged();
}

// ---------------------------------------------------------------------------------------------------------------------
// Event shell

void UDwNodeShellViewModel::Refresh()
{
	UDwRun* TheRun = GetRun();
	const EDwRunPhase Phase = TheRun ? TheRun->GetPhase() : EDwRunPhase::Map;
	int32 Count = 0;
	if (Phase == EDwRunPhase::Event)
	{
		RefreshEvent(*TheRun, Count);
	}
	else
	{
		Headline = Body = FText::GetEmpty();
		PendingChoice = PendingNode = INDEX_NONE;
	}
	Actions.SetNum(Count);
	NotifyChanged();
}

void UDwNodeShellViewModel::RefreshEvent(const UDwRun& TheRun, int32& Count)
{
	const FDwRunState& S = TheRun.GetState();
	const FDwActiveEvent& Active = TheRun.GetEvent();
	const UDwEventDefinition* Event = Active.Event;
	if (PendingNode != Active.Node)
	{
		PendingNode = Active.Node;
		PendingChoice = INDEX_NONE;
	}
	auto Continue = [this, &Count](const FText& Title)
	{
		FDwChoiceView View;
		View.Title = Title;
		Choice(Actions, Count++)->Set(View, [this]() { UDwRun* R = GetRun(); return R && R->LeaveEvent(); });
	};

	// No event fits this layer: calm water.
	if (!Event)
	{
		Headline = LOCTEXT("CalmTitle", "Calm water");
		Body = LOCTEXT("CalmBody", "The sea is quiet. Nothing stirs on the horizon.");
		Continue(LOCTEXT("SailOn", "Sail on"));
		return;
	}
	Headline = Event->Title;

	// 3. Resolved: what happened.
	if (Active.IsResolved())
	{
		TArray<FString> Lines;
		if (Event->Choices.IsValidIndex(Active.ChosenIndex))
		{
			Lines.Add(Event->Choices[Active.ChosenIndex].ResultText.ToString());
		}
		Lines.Add(FString());
		for (const FText& Change : Active.Changes)
		{
			Lines.Add(Change.ToString());
		}
		Body = FText::FromString(FString::Join(Lines, TEXT("\n")).TrimEnd());
		Continue(LOCTEXT("Continue", "Continue"));
		return;
	}

	// 2. Picking who the choice lands on.
	if (Event->Choices.IsValidIndex(PendingChoice))
	{
		const FDwEventChoice& Pending = Event->Choices[PendingChoice];
		Body = FText::Format(LOCTEXT("PickWho", "{0}\n\n{1}: choose a crew member."), Event->Body, Pending.Label);
		for (const FGuid& Id : FDwEventResolver::GetChoosableCrew(S, Pending))
		{
			const FDwCrewMemberState* Member = S.FindCrew(Id);
			FDwChoiceView View;
			View.Title = DwRunScreens::CrewName(S, Id);
			View.Body = Member ? FText::Format(LOCTEXT("CrewHp", "{0} / {1} HP"), FMath::RoundToInt(Member->Health), FMath::RoundToInt(Member->GetMaxHealth()))
				: FText::GetEmpty();
			const int32 Index = PendingChoice;
			Choice(Actions, Count++)->Set(View, [this, Index, Id]() { UDwRun* R = GetRun(); return R && R->ChooseEventOption(Index, Id); });
		}
		FDwChoiceView Back;
		Back.Title = LOCTEXT("Back", "Back");
		Choice(Actions, Count++)->Set(Back, [this]() { return CancelCrewPick(); });
		return;
	}

	// 1. The choices.
	Body = Event->Body;
	for (int32 Index = 0; Index < Event->Choices.Num(); ++Index)
	{
		const FDwEventChoice& EventChoice = Event->Choices[Index];
		FDwChoiceView View;
		View.Title = EventChoice.Label;
		TArray<FString> Parts;
		for (const UDwEventOutcome* Outcome : EventChoice.Outcomes)
		{
			const FText Line = Outcome ? Outcome->Describe() : FText::GetEmpty();
			if (!Line.IsEmpty())
			{
				Parts.Add(Line.ToString());
			}
		}
		View.Body = FText::FromString(FString::Join(Parts, TEXT("\n")));
		const FText Blocker = FDwEventResolver::GetRequirementBlocker(S, EventChoice);
		View.bEnabled = Blocker.IsEmpty();
		View.DisabledReason = Blocker;
		Choice(Actions, Count++)->Set(View, [this, Index]() { return SelectEventChoice(Index); });
	}
}

bool UDwNodeShellViewModel::SelectEventChoice(int32 Index)
{
	UDwRun* TheRun = GetRun();
	const UDwEventDefinition* Event = TheRun ? TheRun->GetEvent().Event.Get() : nullptr;
	if (!Event || !Event->Choices.IsValidIndex(Index) || TheRun->GetEvent().IsResolved())
	{
		return false;
	}
	if (FDwEventResolver::NeedsChosenCrew(Event->Choices[Index]))
	{
		if (!FDwEventResolver::GetRequirementBlocker(TheRun->GetState(), Event->Choices[Index]).IsEmpty())
		{
			return false;
		}
		PendingChoice = Index;
		Refresh();
		return true;
	}
	return TheRun->ChooseEventOption(Index, FGuid());
}

bool UDwNodeShellViewModel::CancelCrewPick()
{
	if (PendingChoice == INDEX_NONE)
	{
		return false;
	}
	PendingChoice = INDEX_NONE;
	Refresh();
	return true;
}

#undef LOCTEXT_NAMESPACE
