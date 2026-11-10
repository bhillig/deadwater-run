// Copyright Brandon Hillig 2026

#include "Run/DwRun.h"

#include "Combat/DwEncounterSetupBuilder.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Core/DwLog.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Run/DwRunRandom.h"
#include "Run/Map/DwPlaceholderMap.h"

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
		AddCrewMember(*Captain.CaptainUnit, /*bIsCaptain*/ true);
	}
	for (const UDwUnitDefinition* Unit : Captain.StartingCrew)
	{
		if (Unit)
		{
			AddCrewMember(*Unit, /*bIsCaptain*/ false);
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

	// Map (DW-202 swaps this for FDwMapGenerator + FDwMapValidator with ForMap(Seed, Attempt) retries).
	State.Map = FDwPlaceholderMap::Build(FDwRunRandom::ForMap(Seed, 0), InGameData.MapRules, EncounterPool, InGameData.KrakenEncounter);

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

FGuid UDwRun::MakeCrewId()
{
	return FGuid(static_cast<uint32>(FDwRunRandom::ForCrew(State.Seed)), 0x44575255u /* "DWRU" */, 0u, static_cast<uint32>(++State.LastCrewSerial));
}

void UDwRun::AddCrewMember(const UDwUnitDefinition& Unit, bool bIsCaptain)
{
	FDwCrewMemberState& Member = State.Crew.AddDefaulted_GetRef();
	Member.Id = MakeCrewId();
	Member.Unit = &Unit;
	Member.bIsCaptain = bIsCaptain;
	Member.Level = 1;
	Member.Health = Member.GetMaxHealth();
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
	Changed(Old);
	return true;
}

bool UDwRun::LeaveShop()
{
	if (!RequirePhase(EDwRunPhase::Shop, TEXT("LeaveShop")))
	{
		return false;
	}
	CompleteNode();
	return true;
}

bool UDwRun::LeaveEvent()
{
	if (!RequirePhase(EDwRunPhase::Event, TEXT("LeaveEvent")))
	{
		return false;
	}
	CompleteNode();
	return true;
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
			? FString(TEXT("The boss node has no encounter: DA_GameData.KrakenEncounter is not set (DW-209)."))
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

	if (!FDwEncounterSetupBuilder::BuildShipBattle(*GameData, *Node->Encounter, Crew, FDwRunRandom::ForNode(State.Seed, State.CurrentNode), Out, OutErrors))
	{
		return false;
	}
	Out.PlayerShip.MaxHealth = State.Ship.MaxHealth;
	Out.PlayerShip.Health = State.Ship.Health;
	Out.PlayerShip.CannonDamage = State.Ship.CannonDamage;
	for (const FDwItemStack& Stack : State.Inventory)
	{
		if (Stack.Item && Stack.Count > 0)
		{
			Out.Items.Add(Stack);
		}
	}
	Out.CaptainRuleTags = Captain->RuleTags;
	Out.OutcomeConfig.bSurrenderEnabled = Captain->RuleTags.HasTagExact(DwTags::Rule_Captain_Surrender);
	Out.OutcomeConfig.bIsBossFight = Node->Type == EDwNodeType::Boss;
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
	CompleteNode(); // DW-203: Rewards (and LevelUp) come between the fight and the map
	return true;
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
