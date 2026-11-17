// Copyright Brandon Hillig 2026

#include "Run/DwRun.h"

#include "Board/DwBoardBuilder.h"
#include "Combat/DwEncounterSetupBuilder.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Core/DwLog.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwEventDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwMapRules.h"
#include "Data/DwPerkDefinition.h"
#include "Data/DwShipUpgradeDefinition.h"
#include "Data/DwShopDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Run/DwCaptainRules.h"
#include "Run/DwCrewFactory.h"
#include "Run/DwProgressionRules.h"
#include "Run/DwRunRandom.h"
#include "Run/Economy/DwRewardRules.h"
#include "Run/Events/DwEventOutcome.h"
#include "Run/Events/DwEventResolver.h"
#include "Run/Map/DwMapGenerator.h"

const FName UDwRun::DefaultTuningRow(TEXT("Default"));

// ---------------------------------------------------------------------------------------------------------------------
// Setup

bool UDwRun::Initialize(const UDwGameData& InGameData, const UDwCaptainDefinition& Captain, int32 Seed,
	TConstArrayView<const UDwEncounterDefinition*> EncounterPool, TArray<FString>* OutErrors)
{
	TArray<FString> Errors;
	GameData = &InGameData;
	State = FDwRunState();
	State.Seed = Seed;
	State.Phase = EDwRunPhase::Map;
	State.Captain = &Captain;
	State.Stats.Captain = &Captain;
	State.Stats.Seed = Seed;

	// Roster: captain first, then the starting crew.
	if (!Captain.CaptainUnit)
	{
		Errors.Add(FString::Printf(TEXT("%s has no CaptainUnit."), *Captain.GetName()));
	}
	else
	{
		FDwCrewFactory::Create(State, *Captain.CaptainUnit, /*Level*/ 1, /*bIsCaptain*/ true);
	}
	for (const UDwUnitDefinition* Unit : Captain.StartingCrew)
	{
		if (Unit)
		{
			FDwCrewFactory::Create(State, *Unit, /*Level*/ 1, /*bIsCaptain*/ false);
		}
	}
	for (const FDwCrewMemberState& Member : State.Crew)
	{
		if (Member.Health <= 0.f)
		{
			Errors.Add(FString::Printf(TEXT("%s has no DT_UnitTuning row (or MaxHealth 0)."), *GetNameSafe(Member.Unit)));
		}
	}

	// Ship and gold from tuning.
	if (const FDwShipTuningRow* Ship = InGameData.PlayerShipTuning.DataTable
		? InGameData.PlayerShipTuning.GetRow<FDwShipTuningRow>(TEXT("UDwRun")) : nullptr)
	{
		State.Ship.MaxHealth = Ship->MaxHealth;
		State.Ship.Health = Ship->MaxHealth;
		State.Ship.CannonDamage = Ship->CannonDamage;
	}
	else
	{
		Errors.Add(TEXT("DA_GameData.PlayerShipTuning does not point at a DT_ShipTuning row."));
	}
	if (const FDwEconomyTuningRow* Economy = InGameData.TuningTables.Economy
		? InGameData.TuningTables.Economy->FindRow<FDwEconomyTuningRow>(DefaultTuningRow, TEXT("UDwRun"), /*bWarnIfRowMissing*/ false) : nullptr)
	{
		State.Gold = Economy->StartingGold;
	}
	else
	{
		Errors.Add(TEXT("DT_EconomyTuning has no 'Default' row."));
	}

	// Map: generate + validate, retried with ForMap(Seed, Attempt) (09 "Map generation").
	const UDwMapRules& Rules = InGameData.MapRules ? *InGameData.MapRules : *GetDefault<UDwMapRules>();
	TArray<FString> MapProblems;
	int32 MapAttempts = 0;
	State.Map = FDwMapGenerator::Build(Seed, Rules, EncounterPool, InGameData.KrakenEncounter, &MapProblems, &MapAttempts);
	for (const FString& Problem : MapProblems)
	{
		Errors.Add(FString::Printf(TEXT("Map after %d attempts: %s"), MapAttempts, *Problem));
	}
	UE_CLOG(MapAttempts > 1 && MapProblems.IsEmpty(), LogDwRun, Log, TEXT("Map valid on attempt %d."), MapAttempts);

	for (const FString& Error : Errors)
	{
		UE_LOG(LogDwRun, Error, TEXT("Run setup: %s"), *Error);
	}
	if (OutErrors)
	{
		OutErrors->Append(Errors);
	}
	UE_LOG(LogDwRun, Log, TEXT("Run started: %s, seed %d, %d crew, %d gold, %d map nodes."), *Captain.GetName(), Seed, State.Crew.Num(), State.Gold,
		State.Map.Nodes.Num());
	OnStateChanged.Broadcast();
	return Errors.IsEmpty();
}

// ---------------------------------------------------------------------------------------------------------------------
// Map

const FDwMapNode* UDwRun::GetCurrentNode() const
{
	return State.Map.Nodes.IsValidIndex(State.CurrentNode) ? &State.Map.Nodes[State.CurrentNode] : nullptr;
}

TArray<int32> UDwRun::GetChoices() const
{
	return State.Phase == EDwRunPhase::Map ? State.Map.GetChoices(State.CurrentNode) : TArray<int32>();
}

bool UDwRun::ChooseNode(int32 NodeIndex)
{
	if (!RequirePhase(EDwRunPhase::Map, TEXT("ChooseNode")))
	{
		return false;
	}
	if (!State.Map.GetChoices(State.CurrentNode).Contains(NodeIndex))
	{
		UE_LOG(LogDwRun, Warning, TEXT("ChooseNode(%d) refused: not reachable from node %d."), NodeIndex, State.CurrentNode);
		return false;
	}
	const EDwRunPhase Old = State.Phase;
	FDwMapNode& Node = State.Map.Nodes[NodeIndex];
	Node.bVisited = true;
	State.CurrentNode = NodeIndex;
	switch (Node.Type)
	{
	case EDwNodeType::Fight:
	case EDwNodeType::Boss:  State.Phase = EDwRunPhase::Combat; break;
	case EDwNodeType::Shop:  State.Phase = EDwRunPhase::Shop; break;
	case EDwNodeType::Event: State.Phase = EDwRunPhase::Event; break;
	}
	UE_LOG(LogDwRun, Log, TEXT("Sailed to node %d (layer %d, lane %d, %s, %s)."), NodeIndex, Node.Layer, Node.Lane,
		*StaticEnum<EDwNodeType>()->GetNameStringByValue(static_cast<int64>(Node.Type)), *GetNameSafe(Node.Encounter));
	if (State.Phase == EDwRunPhase::Shop)
	{
		OpenShop(NodeIndex);
	}
	else if (State.Phase == EDwRunPhase::Event)
	{
		OpenEvent(NodeIndex);
	}
	Changed(Old);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Shop

void UDwRun::OpenShop(int32 NodeIndex)
{
	const UDwShopDefinition* Shop = GameData ? GameData->DefaultShop.Get() : nullptr;
	const FDwShopContext Context = GetShopContext();
	FRandomStream Random(FDwRunRandom::ForNode(State.Seed, NodeIndex));
	State.Shop = Shop ? FDwShopRules::GenerateInventory(State, *Shop, Context, Random) : FDwShopInventory();
	State.Shop.Node = NodeIndex;
	UE_CLOG(!Shop || !Context.Economy, LogDwRun, Error, TEXT("Shop at node %d has nothing to sell: DA_GameData.DefaultShop or DT_EconomyTuning 'Default' is missing."),
		NodeIndex);
	UE_LOG(LogDwRun, Log, TEXT("Shop at node %d: %d offer(s), %d gold to spend."), NodeIndex, State.Shop.Offers.Num(), State.Gold);
}

EDwPurchaseResult UDwRun::CanPurchase(int32 OfferId, FGuid CrewId, int32 ReplaceSlot) const
{
	return State.Phase == EDwRunPhase::Shop ? FDwShopRules::CanPurchase(State, OfferId, CrewId, ReplaceSlot, GetShopContext()) : EDwPurchaseResult::NoOffer;
}

EDwPurchaseResult UDwRun::TryPurchase(int32 OfferId, FGuid CrewId, int32 ReplaceSlot)
{
	if (!RequirePhase(EDwRunPhase::Shop, TEXT("TryPurchase")))
	{
		return EDwPurchaseResult::NoOffer;
	}
	// A copy: buying can append offers (a recruit's disband offer). Names are read before buying: a disband removes the
	// crew member and an ability replace overwrites the slot.
	const FDwShopOffer* Found = State.Shop.Find(OfferId);
	const FDwShopOffer Bought = Found ? *Found : FDwShopOffer();
	const FDwShopOffer* Offer = &Bought;
	const FString What = Found ? FString::Printf(TEXT("%s offer %d (%d gold)"), *DwRun::OfferTypeName(Offer->Type), OfferId, Offer->Price)
		: FString::Printf(TEXT("offer %d"), OfferId);
	const FDwCrewMemberState* Target = Offer->Type == EDwShopOfferType::Disband ? State.FindCrew(Offer->CrewId) : State.FindCrew(CrewId);
	const FString Who = Target ? Target->GetDisplayName().ToString() : FString();
	const FString Forgets = Target && Target->Abilities.IsValidIndex(ReplaceSlot) ? GetNameSafe(Target->Abilities[ReplaceSlot]) : FString();

	const EDwPurchaseResult Result = FDwShopRules::ApplyPurchase(State, OfferId, CrewId, ReplaceSlot, GetShopContext());
	if (Result != EDwPurchaseResult::Ok)
	{
		UE_LOG(LogDwRun, Warning, TEXT("TryPurchase refused: %s - %s."), *What, *DwRun::PurchaseResultName(Result));
		return Result;
	}
	FString Detail;
	switch (Offer->Type)
	{
	case EDwShopOfferType::Repair:      Detail = FString::Printf(TEXT("hull %.0f / %.0f"), State.Ship.Health, State.Ship.MaxHealth); break;
	case EDwShopOfferType::Item:        Detail = FString::Printf(TEXT("%s, %d of %d carried"), *GetNameSafe(Offer->Item), State.CountItems(), GetMaxInventory()); break;
	case EDwShopOfferType::Ability:     Detail = FString::Printf(TEXT("%s learns %s%s"), *Who, *GetNameSafe(Offer->Ability),
		Forgets.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(", forgets %s"), *Forgets)); break;
	case EDwShopOfferType::Recruit:     Detail = FString::Printf(TEXT("%s joins at level %d"), *GetNameSafe(Offer->Unit), State.Crew.Last().Level); break;
	case EDwShopOfferType::ShipUpgrade: Detail = FString::Printf(TEXT("%s: hull %.0f / %.0f, cannons %.0f"), *GetNameSafe(Offer->Upgrade), State.Ship.Health,
		State.Ship.MaxHealth, State.Ship.CannonDamage); break;
	case EDwShopOfferType::Disband:     Detail = FString::Printf(TEXT("%s leaves the crew"), *Who); break;
	}
	UE_LOG(LogDwRun, Log, TEXT("Bought %s: %s; %d gold left."), *What, *Detail, State.Gold);
	Changed(State.Phase);
	return Result;
}

int32 UDwRun::GetMaxInventory() const
{
	const FDwEconomyTuningRow* Economy = GetEconomyTuning();
	return Economy ? FMath::Max(0, Economy->MaxInventory) : 0;
}

FDwShopContext UDwRun::GetShopContext() const
{
	FDwShopContext Context;
	Context.Economy = GetEconomyTuning();
	Context.MaxAbilitySlots = GetMaxAbilitySlots();
	return Context;
}

bool UDwRun::LeaveShop()
{
	if (!RequirePhase(EDwRunPhase::Shop, TEXT("LeaveShop")))
	{
		return false;
	}
	State.Shop = FDwShopInventory();
	CompleteNode();
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Events

void UDwRun::OpenEvent(int32 NodeIndex)
{
	State.Event = FDwActiveEvent();
	State.Event.Node = NodeIndex;
	const FDwRunTuningRow* RunRow = GetRunTuning();
	UE_CLOG(!RunRow, LogDwRun, Error, TEXT("Event at node %d: DT_RunTuning 'Default' is missing; using GoodEventChance 0.6."), NodeIndex);
	const float GoodChance = RunRow ? RunRow->GoodEventChance : 0.6f;
	TArray<const UDwEventDefinition*> Pool;
	if (GameData)
	{
		for (const UDwEventDefinition* Event : GameData->Events)
		{
			Pool.Add(Event);
		}
	}
	FRandomStream Random(FDwRunRandom::ForNode(State.Seed, NodeIndex));
	const UDwEventDefinition* Event = FDwEventResolver::Roll(State, Pool, State.Map.Nodes[NodeIndex].Layer, GoodChance, Random);
	State.Event.Event = Event;
	if (Event)
	{
		State.SeenEvents.Add(Event);
		UE_LOG(LogDwRun, Log, TEXT("Event at node %d: %s (%s, %d choice(s))."), NodeIndex, *Event->GetName(),
			*StaticEnum<EDwEventValence>()->GetNameStringByValue(static_cast<int64>(Event->Valence)), Event->Choices.Num());
	}
	else if (Pool.IsEmpty())
	{
		// Only transient test content has no pool: the content script fails without a Good and a Bad event.
		UE_LOG(LogDwRun, Log, TEXT("Event at node %d: DA_GameData.Events is empty; the player sails on."), NodeIndex);
	}
	else
	{
		UE_LOG(LogDwRun, Warning, TEXT("Event at node %d: no event in DA_GameData.Events fits layer %d; the player sails on."), NodeIndex,
			State.Map.Nodes[NodeIndex].Layer);
	}
}

bool UDwRun::CanChooseEventOption(int32 ChoiceIndex, FGuid CrewId, FText* OutReason) const
{
	auto Refuse = [OutReason](const FText& Reason)
	{
		if (OutReason)
		{
			*OutReason = Reason;
		}
		return false;
	};
	const FDwActiveEvent& Active = State.Event;
	if (State.Phase != EDwRunPhase::Event || !Active.Event)
	{
		return Refuse(NSLOCTEXT("DwRun", "NoEvent", "There is no event here."));
	}
	if (Active.IsResolved())
	{
		return Refuse(NSLOCTEXT("DwRun", "EventDone", "You've already chosen."));
	}
	if (!Active.Event->Choices.IsValidIndex(ChoiceIndex))
	{
		return Refuse(NSLOCTEXT("DwRun", "NoChoice", "No such choice."));
	}
	const FDwEventChoice& Choice = Active.Event->Choices[ChoiceIndex];
	const FText Blocker = FDwEventResolver::GetRequirementBlocker(State, Choice);
	if (!Blocker.IsEmpty())
	{
		return Refuse(Blocker);
	}
	if (FDwEventResolver::NeedsChosenCrew(Choice) && !FDwEventResolver::GetChoosableCrew(State, Choice).Contains(CrewId))
	{
		return Refuse(NSLOCTEXT("DwRun", "PickCrew", "Choose a crew member."));
	}
	return true;
}

bool UDwRun::ChooseEventOption(int32 ChoiceIndex, FGuid CrewId)
{
	if (!RequirePhase(EDwRunPhase::Event, TEXT("ChooseEventOption")))
	{
		return false;
	}
	FText Reason;
	if (!CanChooseEventOption(ChoiceIndex, CrewId, &Reason))
	{
		UE_LOG(LogDwRun, Warning, TEXT("ChooseEventOption(%d) refused: %s"), ChoiceIndex, *Reason.ToString());
		return false;
	}
	FDwActiveEvent& Active = State.Event;
	const FDwEventChoice& Choice = Active.Event->Choices[ChoiceIndex];
	const FDwRunTuningRow* RunRow = GetRunTuning();

	FRandomStream Random(FDwRunRandom::ForEvent(State.Seed, Active.Node));
	FDwEventContext Context(State, Random);
	Context.ChosenCrew = CrewId;
	Context.Economy = GetEconomyTuning();
	Context.Progression = GameData ? GameData->TuningTables.Progression.Get() : nullptr;
	Context.MaxLevel = RunRow ? RunRow->MaxLevel : 0;
	Context.XpMultiplier = FDwCaptainRules::GetXpMultiplier(State.Captain);
	Context.ThresholdMultiplier = FDwCaptainRules::GetLevelThresholdMultiplier(State.Captain);
	FDwEventResolver::Apply(Choice, Context);

	Active.ChosenIndex = ChoiceIndex;
	Active.ChosenCrew = CrewId;
	Active.Changes = MoveTemp(Context.Changes);
	State.PendingLevelUps = MoveTemp(Context.LevelUps);
	NumberLevelUps();
	State.PendingRewards = FDwRewardBundle();
	State.PendingRewards.AbilityChoices = MoveTemp(Context.AbilityChoices);

	TArray<FString> Lines;
	for (const FText& Line : Active.Changes)
	{
		Lines.Add(Line.ToString());
	}
	UE_LOG(LogDwRun, Log, TEXT("Event %s: chose %d (%s): %s."), *Active.Event->GetName(), ChoiceIndex, *Choice.Label.ToString(),
		Lines.IsEmpty() ? TEXT("nothing happens") : *FString::Join(Lines, TEXT("; ")));
	Changed(State.Phase);
	return true;
}

bool UDwRun::LeaveEvent()
{
	if (!RequirePhase(EDwRunPhase::Event, TEXT("LeaveEvent")))
	{
		return false;
	}
	if (State.Event.Event && !State.Event.IsResolved())
	{
		UE_LOG(LogDwRun, Warning, TEXT("LeaveEvent refused: choose one of %s's choices first."), *State.Event.Event->GetName());
		return false;
	}
	State.Event = FDwActiveEvent();
	if (!State.PendingRewards.AbilityChoices.IsEmpty())
	{
		const EDwRunPhase Old = State.Phase;
		State.Phase = EDwRunPhase::Rewards;
		RollCurrentLevelUp();
		Changed(Old);
		return true;
	}
	if (!State.PendingLevelUps.IsEmpty())
	{
		const EDwRunPhase Old = State.Phase;
		RollCurrentLevelUp();
		State.Phase = EDwRunPhase::LevelUp;
		Changed(Old);
		return true;
	}
	CompleteNode();
	return true;
}

void UDwRun::NumberLevelUps()
{
	for (int32 Index = 0; Index < State.PendingLevelUps.Num(); ++Index)
	{
		State.PendingLevelUps[Index].Ordinal = Index;
	}
}

void UDwRun::CompleteNode()
{
	const EDwRunPhase Old = State.Phase;
	++State.Stats.NodesCleared;
	State.Phase = EDwRunPhase::Map;
	Changed(Old);
}

// ---------------------------------------------------------------------------------------------------------------------
// Combat

bool UDwRun::BuildEncounterSetup(FDwEncounterSetup& Out, TArray<FString>& OutErrors) const
{
	if (!RequirePhase(EDwRunPhase::Combat, TEXT("BuildEncounterSetup")))
	{
		OutErrors.Add(TEXT("Not in the Combat phase."));
		return false;
	}
	const FDwMapNode* Node = GetCurrentNode();
	const UDwCaptainDefinition* Captain = State.Captain;
	if (!GameData || !Node || !Captain)
	{
		OutErrors.Add(TEXT("The run is not initialized."));
		return false;
	}
	if (!Node->Encounter)
	{
		OutErrors.Add(Node->Type == EDwNodeType::Boss
			? FString(TEXT("The boss node has no encounter: DA_GameData.KrakenEncounter is not set."))
			: FString::Printf(TEXT("Node %d has no encounter (no ship battle allowed on layer %d)."), State.CurrentNode, Node->Layer));
		return false;
	}

	TArray<FDwCombatantSetup> Crew;
	for (const FDwCrewMemberState& Member : State.Crew)
	{
		if (!Member.bAlive)
		{
			continue;
		}
		FDwCombatantSetup& Combatant = Crew.AddDefaulted_GetRef();
		Combatant.Definition = Member.Unit;
		Combatant.RosterId = Member.Id;
		Combatant.Level = Member.Level;
		Combatant.Health = Member.Health;
		Combatant.bIsCaptain = Member.bIsCaptain;
		Combatant.ExtraAbilities = Member.Abilities;
		Combatant.Perks = Member.Perks;
		if (Member.bIsCaptain)
		{
			for (const UDwAbilityDefinition* Ability : Captain->CaptainAbilities)
			{
				Combatant.ExtraAbilities.Add(Ability);
			}
			Combatant.SpawnEffects = Captain->CaptainCombatEffects;
		}
		else
		{
			Combatant.SpawnEffects = Captain->CrewCombatEffects;
		}
		Combatant.ExtraAbilities.Remove(nullptr);
	}

	if (!FDwEncounterSetupBuilder::Build(*GameData, *Node->Encounter, Crew, FDwRunRandom::ForNode(State.Seed, State.CurrentNode), Out, OutErrors))
	{
		return false;
	}
	// Ship upgrades (06): the stat bonuses are already in the run's numbers; extra GEs go to the spawned ship, granted tags
	// unlock the deck tiles that need them (the third cannon).
	Out.PlayerShip.MaxHealth = State.Ship.MaxHealth;
	Out.PlayerShip.Health = State.Ship.Health;
	Out.PlayerShip.CannonDamage = State.Ship.CannonDamage;
	for (const UDwShipUpgradeDefinition* Upgrade : State.Ship.Upgrades)
	{
		if (Upgrade && Upgrade->Effect)
		{
			Out.PlayerShip.UpgradeEffects.Add(Upgrade->Effect);
		}
	}
	FDwBoardBuilder::ApplyShipUpgrades(Out.Grid, EDwSide::Player, State.Ship.GetUpgradeTags());
	for (const FDwItemStack& Stack : State.Inventory)
	{
		if (Stack.Item && Stack.Count > 0)
		{
			Out.Items.Add(Stack);
		}
	}
	Out.CaptainRuleTags = Captain->RuleTags;
	Out.OutcomeConfig.bSurrenderEnabled = Captain->RuleTags.HasTagExact(DwTags::Rule_Captain_Surrender);
	// The Kraken encounter is a boss fight wherever it is played (BuildKraken); the boss node is one by definition.
	Out.OutcomeConfig.bIsBossFight |= Node->Type == EDwNodeType::Boss;
	return true;
}

bool UDwRun::ApplyCombatResult(const FDwCombatResult& Result)
{
	if (!RequirePhase(EDwRunPhase::Combat, TEXT("ApplyCombatResult")))
	{
		return false;
	}
	if (Result.Outcome == EDwCombatOutcome::None)
	{
		UE_LOG(LogDwRun, Warning, TEXT("ApplyCombatResult refused: the fight has no outcome."));
		return false;
	}

	// Crew: Health back, permadeath.
	for (const FDwCrewResult& CrewResult : Result.Crew)
	{
		FDwCrewMemberState* Member = State.FindCrew(CrewResult.RosterId);
		if (!Member)
		{
			UE_LOG(LogDwRun, Warning, TEXT("ApplyCombatResult: unknown roster id %s ignored."), *CrewResult.RosterId.ToString());
			continue;
		}
		if (!Member->bAlive)
		{
			continue; // never revived
		}
		if (CrewResult.bAlive)
		{
			Member->Health = FMath::Clamp(CrewResult.Health, 1.f, Member->GetMaxHealth());
		}
		else
		{
			Member->bAlive = false;
			Member->Health = 0.f;
			FDwCrewLoss& Loss = State.Stats.CrewLost.AddDefaulted_GetRef();
			Loss.CrewId = Member->Id;
			Loss.Name = Member->GetDisplayName();
			Loss.Node = State.CurrentNode;
		}
	}

	// Ship and items.
	State.Ship.Health = FMath::Clamp(Result.PlayerShipHealth, 0.f, State.Ship.MaxHealth);
	for (const TPair<FName, int32>& Used : Result.ItemsUsed)
	{
		int32 Left = Used.Value;
		for (FDwItemStack& Stack : State.Inventory)
		{
			if (Left > 0 && Stack.Item && Stack.Item->GetFName() == Used.Key)
			{
				const int32 Spent = FMath::Min(Left, Stack.Count);
				Stack.Count -= Spent;
				Left -= Spent;
			}
		}
		UE_CLOG(Left > 0, LogDwRun, Warning, TEXT("ApplyCombatResult: %d x %s used but not in the inventory."), Left, *Used.Key.ToString());
	}
	State.Inventory.RemoveAll([](const FDwItemStack& Stack) { return !Stack.Item || Stack.Count <= 0; });

	// Stats.
	for (const FDwKillRecord& Kill : Result.Kills)
	{
		State.Stats.Kills += Kill.VictimSide == EDwSide::Enemy && Kill.KillerRosterId.IsValid() ? 1 : 0;
	}
	State.Stats.Rounds += Result.Rounds;

	UE_LOG(LogDwRun, Log, TEXT("Fight at node %d: %s after %d rounds; %d of %d crew alive, ship %.0f/%.0f."), State.CurrentNode,
		*StaticEnum<EDwCombatOutcome>()->GetNameStringByValue(static_cast<int64>(Result.Outcome)), Result.Rounds, State.CountLivingCrew(),
		State.Crew.Num(), State.Ship.Health, State.Ship.MaxHealth);

	// Where the run goes next.
	if (DwCombat::IsDefeat(Result.Outcome))
	{
		EndRun(EDwRunPhase::Defeat, DwRun::EndReasonFor(Result.Outcome));
		return true;
	}
	const FDwMapNode* Node = GetCurrentNode();
	if (Result.Outcome == EDwCombatOutcome::VictoryBoss || (Node && Node->Type == EDwNodeType::Boss))
	{
		++State.Stats.NodesCleared;
		EndRun(EDwRunPhase::Victory, EDwRunEndReason::Victory);
		return true;
	}
	State.Stats.FightsSunk += Result.Outcome == EDwCombatOutcome::VictorySunk ? 1 : 0;
	State.Stats.FightsPlundered += Result.Outcome == EDwCombatOutcome::VictoryCrewDefeated ? 1 : 0;
	State.Stats.FightsSurrendered += Result.Outcome == EDwCombatOutcome::VictorySurrender ? 1 : 0;
	const EDwRunPhase Old = State.Phase;
	QueueRewards(Result);
	State.Phase = EDwRunPhase::Rewards;
	Changed(Old);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Rewards and level-ups

void UDwRun::QueueRewards(const FDwCombatResult& Result)
{
	const FDwMapNode* Node = GetCurrentNode();
	const FDwEncounterTuningRow* EncounterRow = Node && Node->Encounter ? Node->Encounter->GetTuningRow<FDwEncounterTuningRow>() : nullptr;
	const FDwRunTuningRow* RunRow = GetRunTuning();
	const UDataTable* Progression = GameData ? GameData->TuningTables.Progression.Get() : nullptr;
	const float XpMultiplier = FDwCaptainRules::GetXpMultiplier(State.Captain);
	const float ThresholdMultiplier = FDwCaptainRules::GetLevelThresholdMultiplier(State.Captain);
	const int32 ClearXp = EncounterRow ? EncounterRow->ClearXp : 0;

	// Gold, plunder and ability choices, from the node's own reward stream.
	FDwRewardContext Context;
	Context.Outcome = Result.Outcome;
	Context.Encounter = EncounterRow;
	Context.Economy = GetEconomyTuning();
	Context.State = &State;
	if (GameData && GameData->DefaultShop)
	{
		for (const UDwItemDefinition* Item : GameData->DefaultShop->Items)
		{
			Context.PlunderItems.Add(Item);
		}
	}
	FRandomStream Random(FDwRunRandom::ForRewards(State.Seed, State.CurrentNode));
	State.PendingRewards = FDwRewardRules::ComputeRewards(Context, Random);

	// XP (12 "Flow"): kill XP to each living killer, clear XP to every living crew member; sinking credits nobody (C11).
	State.PendingLevelUps.Reset();
	for (FDwCrewMemberState& Member : State.Crew)
	{
		if (!Member.bAlive)
		{
			continue;
		}
		int32 Xp = ClearXp;
		for (const FDwKillRecord& Kill : Result.Kills)
		{
			Xp += Kill.KillerRosterId == Member.Id && Kill.VictimSide == EDwSide::Enemy ? Kill.Xp : 0;
		}
		const int32 OldLevel = Member.Level;
		const int32 Gained = FDwProgressionRules::GrantXp(Member, Xp, XpMultiplier, ThresholdMultiplier, Progression, RunRow ? RunRow->MaxLevel : 0);

		FDwXpGain& Gain = State.PendingRewards.Xp.AddDefaulted_GetRef();
		Gain.CrewId = Member.Id;
		Gain.Xp = FMath::RoundToInt(Xp * XpMultiplier);
		Gain.LevelsGained = Gained;

		FDwProgressionRules::AppendLevelUps(State.PendingLevelUps, Member, OldLevel);
		UE_CLOG(Gained > 0, LogDwRun, Log, TEXT("%s gained %d XP: level %d -> %d."), *Member.GetDisplayName().ToString(), Gain.Xp, OldLevel, Member.Level);
	}
	NumberLevelUps();
	RollCurrentLevelUp(); // the rewards screen can preview the first one

	const FDwRewardBundle& Bundle = State.PendingRewards;
	UE_LOG(LogDwRun, Log, TEXT("Rewards at node %d: %d gold, %d item(s), %d ability choice(s), %d level-up(s)."), State.CurrentNode, Bundle.Gold,
		Bundle.Items.Num(), Bundle.AbilityChoices.Num(), State.PendingLevelUps.Num());
}

bool UDwRun::ClaimRewards(int32 AbilityChoice, FGuid CrewId, int32 ReplaceSlot)
{
	if (!RequirePhase(EDwRunPhase::Rewards, TEXT("ClaimRewards")))
	{
		return false;
	}
	FDwRewardBundle& Bundle = State.PendingRewards;

	// Validate the ability pick before changing anything.
	FDwCrewMemberState* Learner = nullptr;
	const UDwAbilityDefinition* Ability = nullptr;
	if (AbilityChoice != INDEX_NONE)
	{
		if (!Bundle.AbilityChoices.IsValidIndex(AbilityChoice))
		{
			UE_LOG(LogDwRun, Warning, TEXT("ClaimRewards refused: no ability choice %d (%d offered)."), AbilityChoice, Bundle.AbilityChoices.Num());
			return false;
		}
		const FDwAbilityOffer& Offer = Bundle.AbilityChoices[AbilityChoice];
		Learner = State.FindCrew(CrewId);
		if (!Learner || !Learner->bAlive || !Offer.EligibleCrew.Contains(CrewId))
		{
			UE_LOG(LogDwRun, Warning, TEXT("ClaimRewards refused: %s can't learn %s."), Learner ? *Learner->GetDisplayName().ToString() : *CrewId.ToString(),
				*GetNameSafe(Offer.Ability));
			return false;
		}
		const int32 MaxSlots = GetMaxAbilitySlots();
		const bool bFull = MaxSlots > 0 && Learner->Abilities.Num() >= MaxSlots;
		if (ReplaceSlot != INDEX_NONE && !Learner->Abilities.IsValidIndex(ReplaceSlot))
		{
			UE_LOG(LogDwRun, Warning, TEXT("ClaimRewards refused: %s has no ability slot %d."), *Learner->GetDisplayName().ToString(), ReplaceSlot);
			return false;
		}
		if (bFull && ReplaceSlot == INDEX_NONE)
		{
			UE_LOG(LogDwRun, Warning, TEXT("ClaimRewards refused: %s already has %d abilities; name the one to replace."),
				*Learner->GetDisplayName().ToString(), MaxSlots);
			return false;
		}
		Ability = Offer.Ability;
	}

	// Apply.
	const EDwRunPhase Old = State.Phase;
	State.Gold += Bundle.Gold;
	State.Stats.GoldEarned += Bundle.Gold;
	const int32 MaxInventory = GetMaxInventory();
	for (const FDwItemStack& Stack : Bundle.Items)
	{
		const int32 Added = State.AddItem(Stack.Item, Stack.Count, MaxInventory);
		State.Stats.ItemsLeftBehind += Stack.Count - Added; // 0: the bundle only holds what fit when it was rolled
	}
	for (const FDwItemStack& Stack : Bundle.ItemsNoRoom)
	{
		State.Stats.ItemsLeftBehind += Stack.Count;
		UE_LOG(LogDwRun, Log, TEXT("Inventory full (%d of %d): %d x %s left behind."), State.CountItems(), MaxInventory, Stack.Count, *GetNameSafe(Stack.Item));
	}
	if (Learner && Ability)
	{
		if (ReplaceSlot != INDEX_NONE)
		{
			UE_LOG(LogDwRun, Log, TEXT("%s forgets %s."), *Learner->GetDisplayName().ToString(), *GetNameSafe(Learner->Abilities[ReplaceSlot]));
			Learner->Abilities[ReplaceSlot] = Ability;
		}
		else
		{
			Learner->Abilities.Add(Ability);
		}
		UE_LOG(LogDwRun, Log, TEXT("%s learns %s."), *Learner->GetDisplayName().ToString(), *GetNameSafe(Ability));
	}
	State.PendingRewards = FDwRewardBundle();

	if (State.PendingLevelUps.IsEmpty())
	{
		CompleteNode();
		return true;
	}
	RollCurrentLevelUp();
	State.Phase = EDwRunPhase::LevelUp;
	Changed(Old);
	return true;
}

bool UDwRun::ChoosePerk(int32 ChoiceIndex)
{
	if (!RequirePhase(EDwRunPhase::LevelUp, TEXT("ChoosePerk")) || !ensure(!State.PendingLevelUps.IsEmpty()))
	{
		return false;
	}
	RollCurrentLevelUp();
	const FDwPendingLevelUp& LevelUp = State.PendingLevelUps[0];
	FDwCrewMemberState* Member = State.FindCrew(LevelUp.CrewId);
	const bool bHasChoices = !LevelUp.PerkChoices.IsEmpty();
	if (bHasChoices ? !LevelUp.PerkChoices.IsValidIndex(ChoiceIndex) : ChoiceIndex != INDEX_NONE)
	{
		UE_LOG(LogDwRun, Warning, TEXT("ChoosePerk(%d) refused: %d perk choice(s) offered."), ChoiceIndex, LevelUp.PerkChoices.Num());
		return false;
	}
	if (bHasChoices && Member)
	{
		const UDwPerkDefinition* Perk = LevelUp.PerkChoices[ChoiceIndex];
		if (!Perk || !FDwProgressionRules::ApplyPerk(*Member, *Perk))
		{
			UE_LOG(LogDwRun, Warning, TEXT("ChoosePerk(%d) refused: %s can't take %s."), ChoiceIndex, *Member->GetDisplayName().ToString(), *GetNameSafe(Perk));
			return false;
		}
		UE_LOG(LogDwRun, Log, TEXT("%s (level %d) takes %s."), *Member->GetDisplayName().ToString(), LevelUp.NewLevel, *GetNameSafe(Perk));
	}

	State.PendingLevelUps.RemoveAt(0);
	if (State.PendingLevelUps.IsEmpty())
	{
		CompleteNode();
		return true;
	}
	RollCurrentLevelUp();
	Changed(State.Phase);
	return true;
}

const FDwPendingLevelUp* UDwRun::GetCurrentLevelUp() const
{
	return State.PendingLevelUps.IsEmpty() ? nullptr : &State.PendingLevelUps[0];
}

int32 UDwRun::GetMaxAbilitySlots() const
{
	const FDwRunTuningRow* RunRow = GetRunTuning();
	return RunRow ? FMath::Max(0, RunRow->MaxAbilitySlots) : 0;
}

void UDwRun::RollCurrentLevelUp()
{
	if (State.PendingLevelUps.IsEmpty() || State.PendingLevelUps[0].bPerksRolled)
	{
		return;
	}
	FDwPendingLevelUp& LevelUp = State.PendingLevelUps[0];
	LevelUp.bPerksRolled = true;
	const FDwCrewMemberState* Member = State.FindCrew(LevelUp.CrewId);
	const FDwRunTuningRow* RunRow = GetRunTuning();
	if (!Member || !Member->Unit || !RunRow)
	{
		return;
	}
	FRandomStream Random(FDwRunRandom::ForPerks(State.Seed, State.CurrentNode, LevelUp.Ordinal));
	for (const UDwPerkDefinition* Perk : FDwProgressionRules::RollPerkChoices(*Member, Member->Unit->PerkPool, RunRow->PerkChoices, Random))
	{
		LevelUp.PerkChoices.Add(Perk);
	}
}

const FDwEconomyTuningRow* UDwRun::GetEconomyTuning() const
{
	const UDataTable* Table = GameData ? GameData->TuningTables.Economy.Get() : nullptr;
	return Table ? Table->FindRow<FDwEconomyTuningRow>(DefaultTuningRow, TEXT("UDwRun"), /*bWarnIfRowMissing*/ false) : nullptr;
}

const FDwRunTuningRow* UDwRun::GetRunTuning() const
{
	const UDataTable* Table = GameData ? GameData->TuningTables.Run.Get() : nullptr;
	return Table ? Table->FindRow<FDwRunTuningRow>(DefaultTuningRow, TEXT("UDwRun"), /*bWarnIfRowMissing*/ false) : nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
// End

bool UDwRun::Abandon()
{
	if (State.IsOver())
	{
		UE_LOG(LogDwRun, Warning, TEXT("Abandon refused: the run is already over (%s)."), *DwRun::PhaseName(State.Phase));
		return false;
	}
	EndRun(EDwRunPhase::Defeat, EDwRunEndReason::Abandoned);
	return true;
}

void UDwRun::EndRun(EDwRunPhase Phase, EDwRunEndReason Reason)
{
	const EDwRunPhase Old = State.Phase;
	State.Phase = Phase;
	State.Stats.EndReason = Reason;
	UE_LOG(LogDwRun, Log, TEXT("Run over: %s (%s) after %d nodes, %d kills, %d crew lost."), *DwRun::PhaseName(Phase), *DwRun::EndReasonName(Reason),
		State.Stats.NodesCleared, State.Stats.Kills, State.Stats.CrewLost.Num());
	Changed(Old);
}

void UDwRun::SetElapsedSeconds(float Seconds)
{
	State.Stats.ElapsedSeconds = FMath::Max(0.f, Seconds);
}

// ---------------------------------------------------------------------------------------------------------------------
// Helpers

bool UDwRun::RequirePhase(EDwRunPhase Expected, const TCHAR* What) const
{
	if (State.Phase == Expected)
	{
		return true;
	}
	UE_LOG(LogDwRun, Warning, TEXT("%s refused: needs phase %s, run is in %s."), What, *DwRun::PhaseName(Expected), *DwRun::PhaseName(State.Phase));
	return false;
}

void UDwRun::Changed(EDwRunPhase OldPhase)
{
	OnStateChanged.Broadcast();
	if (State.Phase != OldPhase)
	{
		OnPhaseChanged.Broadcast(State.Phase, OldPhase);
	}
}
