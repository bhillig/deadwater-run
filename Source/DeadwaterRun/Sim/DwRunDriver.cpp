// Copyright Brandon Hillig 2026

#include "Sim/DwRunDriver.h"

#include "AI/DwAiProfile.h"
#include "Combat/DwCombatSubsystem.h"
#include "Combat/DwCombatUnit.h"
#include "Combat/DwShip.h"
#include "Core/DwGameData.h"
#include "Core/DwLog.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwEventDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Presentation/DwPresentationSubsystem.h"
#include "Run/DwRun.h"
#include "Run/DwRunSubsystem.h"
#include "Run/Events/DwEventResolver.h"
#include "Sim/DwSimPlayerSideController.h"
#include "Sim/DwSimPolicy.h"

namespace DwRunDriver
{
	/** Keeps the policy stream apart from the run's own streams while following the run seed. */
	static constexpr uint32 PolicySalt = 0x9E3F0A11u;

	template <typename TEnum>
	FString Name(TEnum Value)
	{
		return StaticEnum<TEnum>()->GetNameStringByValue(static_cast<int64>(Value));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// World

void UDwRunDriver::Initialize()
{
	if (World)
	{
		return;
	}
	World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false, TEXT("DwSimWorld"));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL()); // or ASCs never register their attribute sets
	World->GetSubsystem<UDwPresentationSubsystem>()->SetInstant(true); // a fight resolves inside StartEncounter

	PlayerSide = NewObject<UDwSimPlayerSideController>(this);
	World->GetSubsystem<UDwCombatSubsystem>()->SetSideController(EDwSide::Player, TScriptInterface<IDwCombatSideController>(PlayerSide));
}

void UDwRunDriver::Shutdown()
{
	if (!World)
	{
		return;
	}
	if (UDwCombatSubsystem* Combat = World->GetSubsystem<UDwCombatSubsystem>())
	{
		Combat->EndEncounter();
		Combat->SetSideController(EDwSide::Player, nullptr);
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	World = nullptr;
	Run = nullptr;
}

FString UDwRunDriver::CaptainName(const UDwCaptainDefinition& Captain)
{
	FString Name = Captain.GetName();
	Name.RemoveFromStart(TEXT("DA_Captain_"));
	return Name;
}

// ---------------------------------------------------------------------------------------------------------------------
// One run

FDwSimRunReport UDwRunDriver::PlayRun(const UDwGameData& GameData, const UDwCaptainDefinition& Captain, int32 Seed, const UDwSimPolicy& Policy)
{
	FDwSimRunReport Report;
	Report.Seed = Seed;
	Report.Captain = CaptainName(Captain);
	Report.Policy = Policy.GetName();
	Initialize();

	FString Stuck;
	Run = NewObject<UDwRun>(this);
	TArray<FString> Errors;
	if (!Policy.PlayerProfile)
	{
		Stuck = FString::Printf(TEXT("%s has no PlayerProfile"), *Policy.GetName());
	}
	else if (!Run->Initialize(GameData, Captain, Seed, UDwRunSubsystem::GatherEncounterPool(), &Errors))
	{
		Stuck = TEXT("Initialize: ") + FString::Join(Errors, TEXT("; "));
	}
	PolicyRandom.Initialize(static_cast<int32>(HashCombineFast(GetTypeHash(Seed), DwRunDriver::PolicySalt)));

	int32 Steps = 0;
	while (Stuck.IsEmpty() && !Run->IsOver())
	{
		if (++Steps > MaxSteps)
		{
			Stuck = FString::Printf(TEXT("no end after %d steps (phase %s)"), MaxSteps, *DwRunDriver::Name(Run->GetPhase()));
			break;
		}
		const EDwRunPhase Phase = Run->GetPhase();
		bool bOk = false;
		switch (Phase)
		{
		case EDwRunPhase::Map:     bOk = ChooseNode(Policy, Stuck); break;
		case EDwRunPhase::Combat:  bOk = PlayFight(Policy, Report, Stuck); break;
		case EDwRunPhase::Rewards: bOk = PlayRewards(Policy, Stuck); break;
		case EDwRunPhase::LevelUp: bOk = PlayLevelUp(Stuck); Report.LevelsGained += bOk ? 1 : 0; break;
		case EDwRunPhase::Shop:    bOk = PlayShop(Policy, Report, Stuck); break;
		case EDwRunPhase::Event:   bOk = PlayEvent(Stuck); break;
		default: break;
		}
		if (!bOk && Stuck.IsEmpty())
		{
			Stuck = FString::Printf(TEXT("%s step refused"), *DwRunDriver::Name(Phase));
		}
	}
	Report.Steps = Steps;
	Report.StuckReason = Stuck;
	UE_CLOG(!Stuck.IsEmpty(), LogDwSim, Warning, TEXT("Run %s seed %d is stuck: %s"), *Report.Captain, Seed, *Stuck);
	FinishReport(Policy, Report);
	return Report;
}

void UDwRunDriver::FinishReport(const UDwSimPolicy& Policy, FDwSimRunReport& Report) const
{
	if (!Run)
	{
		return;
	}
	const FDwRunState& S = Run->GetState();
	const FDwRunStats& Stats = S.Stats;
	Report.Result = !Report.StuckReason.IsEmpty() ? TEXT("Stuck") : S.Phase == EDwRunPhase::Victory ? TEXT("Victory") : TEXT("Defeat");
	Report.EndReason = DwRunDriver::Name(Stats.EndReason);
	Report.LastNode = S.CurrentNode;
	const FDwMapNode* Node = Run->GetCurrentNode();
	Report.LastLayer = Node ? Node->Layer : INDEX_NONE;
	if (Report.Result == TEXT("Defeat") && !Report.Fights.IsEmpty() && Report.Fights.Last().Node == S.CurrentNode)
	{
		Report.DiedTo = Report.Fights.Last().Encounter;
	}
	Report.NodesCleared = Stats.NodesCleared;
	Report.Kills = Stats.Kills;
	Report.Rounds = Stats.Rounds;
	Report.FightsSunk = Stats.FightsSunk;
	Report.FightsPlundered = Stats.FightsPlundered;
	Report.FightsSurrendered = Stats.FightsSurrendered;
	Report.GoldEarned = Stats.GoldEarned;
	Report.GoldSpent = Stats.GoldSpent;
	Report.CrewDeaths = Stats.CrewLost.Num();

	int32 PlayerActions = 0, EnemyActions = 0, Visited = 0;
	for (const FDwSimFightReport& Fight : Report.Fights)
	{
		PlayerActions += Fight.PlayerActions;
		EnemyActions += Fight.EnemyActions;
	}
	for (const FDwMapNode& MapNode : S.Map.Nodes)
	{
		Visited += MapNode.bVisited ? 1 : 0;
	}
	const float Seconds = PlayerActions * Policy.SecondsPerAction + EnemyActions * Policy.SecondsPerEnemyAction + Visited * Policy.SecondsPerNode;
	Report.EstimatedMinutes = FMath::RoundToFloat(Seconds / 6.f) / 10.f; // one decimal: stable in JSON and CSV
}

// ---------------------------------------------------------------------------------------------------------------------
// Phases

bool UDwRunDriver::ChooseNode(const UDwSimPolicy& Policy, FString& OutStuck)
{
	const TArray<int32> Choices = Run->GetChoices();
	if (Choices.IsEmpty())
	{
		OutStuck = TEXT("Map: no route onward");
		return false;
	}
	const FDwRunState& S = Run->GetState();
	const bool bHurt = S.Ship.MaxHealth > 0.f && S.Ship.Health < Policy.RepairBelowHullPct * S.Ship.MaxHealth;
	TArray<float> Weights;
	float Total = 0.f;
	for (const int32 Index : Choices)
	{
		const EDwNodeType Type = S.Map.Nodes[Index].Type;
		const float Weight = Type == EDwNodeType::Shop ? Policy.ShopWeight * (bHurt ? Policy.HurtShopBias : 1.f)
			: Type == EDwNodeType::Event ? Policy.EventWeight : Policy.FightWeight;
		Weights.Add(FMath::Max(0.f, Weight));
		Total += Weights.Last();
	}
	const float Roll = PolicyRandom.GetFraction() * Total; // one draw per choice, whatever the weights
	int32 Pick = Choices[0];
	float Left = Roll;
	for (int32 I = 0; I < Choices.Num() && Total > 0.f; ++I)
	{
		Left -= Weights[I];
		if (Weights[I] > 0.f && Left < 0.f)
		{
			Pick = Choices[I];
			break;
		}
	}
	return Run->ChooseNode(Pick);
}

void UDwRunDriver::StripArt(FDwEncounterSetup& Setup) const
{
	Setup.BoardClass = nullptr; // ADwBoard
	Setup.PlayerShip.ShipClass = ADwShip::StaticClass();
	Setup.EnemyShip.ShipClass = ADwShip::StaticClass();
	for (TArray<FDwCombatantSetup>* Side : { &Setup.Crew, &Setup.Enemies })
	{
		for (FDwCombatantSetup& Unit : *Side)
		{
			Unit.UnitClassOverride = ADwCombatUnit::StaticClass();
		}
	}
}

bool UDwRunDriver::PlayFight(const UDwSimPolicy& Policy, FDwSimRunReport& Report, FString& OutStuck)
{
	FDwEncounterSetup Setup;
	TArray<FString> Errors;
	if (!Run->BuildEncounterSetup(Setup, Errors))
	{
		OutStuck = TEXT("Combat: no setup: ") + FString::Join(Errors, TEXT("; "));
		return false;
	}
	if (bPlainActors)
	{
		StripArt(Setup);
	}
	const FDwRunState& S = Run->GetState();
	const FDwMapNode* Node = Run->GetCurrentNode();
	FDwSimFightReport& Fight = Report.Fights.AddDefaulted_GetRef();
	Fight.Node = S.CurrentNode;
	Fight.Layer = Node ? Node->Layer : INDEX_NONE;
	Fight.Encounter = GetNameSafe(Setup.Encounter);
	Fight.HullBefore = FMath::RoundToInt(S.Ship.Health);
	const int32 CrewBefore = S.CountLivingCrew();

	UDwCombatSubsystem* Combat = World->GetSubsystem<UDwCombatSubsystem>();
	PlayerSide->Configure(*Policy.PlayerProfile, Setup, Policy.MaxRoundsPerFight);
	if (!Combat->StartEncounter(Setup))
	{
		Fight.Outcome = TEXT("Stuck");
		OutStuck = FString::Printf(TEXT("Combat: node %d (%s) refused to start"), S.CurrentNode, *Fight.Encounter);
		return false;
	}
	// Both sides play synchronously inside StartEncounter; by now the fight is over (or stalled).
	Fight.PlayerActions = PlayerSide->GetActions();
	Fight.RejectedCommands = PlayerSide->GetRejected();
	for (const FDwCombatCommand& Command : Combat->GetRecord().Commands)
	{
		const ADwCombatUnit* Unit = (Command.Type == EDwCommandType::Move || Command.Type == EDwCommandType::Ability) ? Combat->FindUnit(Command.Unit) : nullptr;
		Fight.EnemyActions += Unit && Unit->GetSide() == EDwSide::Enemy ? 1 : 0;
	}
	if (Combat->GetOutcome() == EDwCombatOutcome::None)
	{
		Fight.Outcome = TEXT("Stuck");
		Fight.Rounds = Combat->GetRound();
		OutStuck = PlayerSide->HitRoundLimit()
			? FString::Printf(TEXT("Combat: node %d (%s) still going after %d rounds"), S.CurrentNode, *Fight.Encounter, Policy.MaxRoundsPerFight)
			: FString::Printf(TEXT("Combat: node %d (%s) stalled in round %d on the %s side"), S.CurrentNode, *Fight.Encounter, Combat->GetRound(),
				*DwRunDriver::Name(Combat->GetActiveSide()));
		Combat->EndEncounter();
		return false;
	}
	const FDwCombatResult Result = Combat->GetResult();
	Combat->EndEncounter();
	Fight.Outcome = DwRunDriver::Name(Result.Outcome);
	Fight.Rounds = Result.Rounds;
	if (!Run->ApplyCombatResult(Result))
	{
		OutStuck = FString::Printf(TEXT("Combat: node %d result refused"), Fight.Node);
		return false;
	}
	Fight.HullAfter = FMath::RoundToInt(Run->GetState().Ship.Health);
	Fight.CrewLost = CrewBefore - Run->GetState().CountLivingCrew();
	return true;
}

bool UDwRunDriver::PlayRewards(const UDwSimPolicy& Policy, FString& OutStuck)
{
	const FDwRunState& S = Run->GetState();
	const int32 MaxSlots = Run->GetMaxAbilitySlots();
	if (Policy.bTakeAbilities)
	{
		const TArray<FDwAbilityOffer> Offers = S.PendingRewards.AbilityChoices;
		for (int32 Choice = 0; Choice < Offers.Num(); ++Choice)
		{
			for (const FGuid& CrewId : Offers[Choice].EligibleCrew)
			{
				const FDwCrewMemberState* Crew = S.FindCrew(CrewId);
				if (Crew && Crew->bAlive && (MaxSlots <= 0 || Crew->Abilities.Num() < MaxSlots))
				{
					return Run->ClaimRewards(Choice, CrewId);
				}
			}
		}
	}
	return Run->ClaimRewards(INDEX_NONE, FGuid());
}

bool UDwRunDriver::PlayLevelUp(FString& OutStuck)
{
	const FDwPendingLevelUp* LevelUp = Run->GetCurrentLevelUp();
	const int32 Choices = LevelUp ? LevelUp->PerkChoices.Num() : 0;
	return Run->ChoosePerk(Choices > 0 ? PolicyRandom.RandRange(0, Choices - 1) : INDEX_NONE);
}

bool UDwRunDriver::PlayEvent(FString& OutStuck)
{
	if (Run->GetEvent().Event && !Run->GetEvent().IsResolved())
	{
		FGuid Crew;
		const int32 Choice = FDwEventResolver::PickDefaultChoice(Run->GetState(), Crew);
		if (Choice != INDEX_NONE && !Run->ChooseEventOption(Choice, Crew))
		{
			OutStuck = FString::Printf(TEXT("Event: %s choice %d refused"), *GetNameSafe(Run->GetEvent().Event.Get()), Choice);
			return false;
		}
	}
	return Run->LeaveEvent();
}

bool UDwRunDriver::PlayShop(const UDwSimPolicy& Policy, FDwSimRunReport& Report, FString& OutStuck)
{
	const FDwRunState& S = Run->GetState();
	const FDwMapNode* Node = Run->GetCurrentNode();
	FDwSimShopReport& Shop = Report.Shops.AddDefaulted_GetRef();
	Shop.Node = S.CurrentNode;
	Shop.Layer = Node ? Node->Layer : INDEX_NONE;
	Shop.GoldOnArrival = S.Gold;
	Shop.HullPctOnArrival = S.Ship.MaxHealth > 0.f ? FMath::RoundToInt(100.f * S.Ship.Health / S.Ship.MaxHealth) : 0;

	auto OffersOf = [this](EDwShopOfferType Type)
	{
		TArray<FDwShopOffer> Out;
		for (const FDwShopOffer& Offer : Run->GetShop().Offers)
		{
			if (Offer.Type == Type && !Offer.bSoldOut)
			{
				Out.Add(Offer);
			}
		}
		return Out;
	};
	auto Buy = [this, &Shop](const FDwShopOffer& Offer, const FGuid& Crew = FGuid())
	{
		if (Run->CanPurchase(Offer.OfferId, Crew) != EDwPurchaseResult::Ok || Run->TryPurchase(Offer.OfferId, Crew) != EDwPurchaseResult::Ok)
		{
			return false;
		}
		Shop.Bought.Add(DwRunDriver::Name(Offer.Type));
		return true;
	};
	for (const FDwShopOffer& Offer : OffersOf(EDwShopOfferType::Repair))
	{
		Shop.bCouldAffordRepair |= Run->CanPurchase(Offer.OfferId, FGuid()) == EDwPurchaseResult::Ok;
	}

	// 1. Repairs while the hull is low.
	for (int32 Guard = 0; Guard < 10 && S.Ship.Health < Policy.RepairBelowHullPct * S.Ship.MaxHealth; ++Guard)
	{
		const TArray<FDwShopOffer> Repairs = OffersOf(EDwShopOfferType::Repair);
		if (Repairs.IsEmpty() || !Buy(Repairs[0]))
		{
			break;
		}
	}
	// 2. A recruit while the crew is short.
	if (S.CountLivingCrew() < Policy.RecruitBelowCrew)
	{
		for (const FDwShopOffer& Offer : OffersOf(EDwShopOfferType::Recruit))
		{
			if (Buy(Offer))
			{
				break;
			}
		}
	}
	// 3. Ship upgrades, 4. a move for whoever has room, 5. items.
	if (Policy.bBuyShipUpgrades)
	{
		for (const FDwShopOffer& Offer : OffersOf(EDwShopOfferType::ShipUpgrade))
		{
			Buy(Offer);
		}
	}
	if (Policy.bBuyAbilities)
	{
		const int32 MaxSlots = Run->GetMaxAbilitySlots();
		for (const FDwShopOffer& Offer : OffersOf(EDwShopOfferType::Ability))
		{
			for (const FGuid& CrewId : Offer.EligibleCrew)
			{
				const FDwCrewMemberState* Crew = S.FindCrew(CrewId);
				if (Crew && Crew->bAlive && (MaxSlots <= 0 || Crew->Abilities.Num() < MaxSlots) && Buy(Offer, CrewId))
				{
					break;
				}
			}
		}
	}
	if (Policy.bBuyItems)
	{
		for (const FDwShopOffer& Offer : OffersOf(EDwShopOfferType::Item))
		{
			for (int32 Guard = 0; Guard < 5 && Buy(Offer); ++Guard)
			{
			}
		}
	}
	Shop.GoldOnLeaving = S.Gold;
	if (!Run->LeaveShop())
	{
		OutStuck = FString::Printf(TEXT("Shop: node %d would not let us leave"), Shop.Node);
		return false;
	}
	return true;
}
