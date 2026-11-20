// Copyright Brandon Hillig 2026

#include "Sim/DwSimPlayerSideController.h"

#include "AI/DwAiProfile.h"
#include "AI/DwCrewAiBrain.h"
#include "Combat/DwCombatSubsystem.h"
#include "Combat/DwCombatUnit.h"
#include "Core/DwLog.h"

namespace DwSimPlayerSide
{
	/** Keeps the player AI stream apart from the enemy AI and combat streams while following the encounter seed. */
	static constexpr uint32 StreamSalt = 0x51A7E2D5u;
	/** Same bound as UDwAiSideController::MaxCommandsPerUnit. */
	static constexpr int32 MaxCommandsPerUnit = 8;
}

void UDwSimPlayerSideController::Configure(const UDwAiProfile& Profile, const FDwEncounterSetup& Setup, int32 InMaxRounds)
{
	if (!Brain)
	{
		Brain = NewObject<UDwCrewAiBrain>(this);
	}
	Brain->Configure(&Profile, Setup);
	Random.Initialize(static_cast<int32>(HashCombineFast(GetTypeHash(Setup.Seed), DwSimPlayerSide::StreamSalt)));
	Queue.Reset();
	TurnContext.Reset();
	MaxRounds = FMath::Max(1, InMaxRounds);
	Actions = 0;
	Rejected = 0;
	bHitRoundLimit = false;
	bTurnOpen = false;
}

void UDwSimPlayerSideController::BeginSideTurn(UDwCombatSubsystem& Combat, EDwSide Side)
{
	if (Side != EDwSide::Player)
	{
		return;
	}
	if (Combat.GetRound() > MaxRounds)
	{
		// Stop playing: the turn loop runs dry and StartEncounter returns with no outcome; the driver reports it.
		bHitRoundLimit = true;
		UE_LOG(LogDwSim, Warning, TEXT("Fight still going after %d rounds; the simulator stops playing it."), MaxRounds);
		return;
	}
	Queue.Reset();
	for (const ADwCombatUnit* Unit : Combat.GetUnits())
	{
		if (Unit && Unit->GetSide() == EDwSide::Player && Unit->IsAlive())
		{
			Queue.Add(Unit->GetHandle());
		}
	}
	Queue.Sort([](const FDwUnitHandle& A, const FDwUnitHandle& B) { return A.Id < B.Id; });
	TurnContext.Reset();
	bTurnOpen = true;
	Act(Combat);
}

void UDwSimPlayerSideController::OnCommandResolved(UDwCombatSubsystem& Combat)
{
	if (bTurnOpen)
	{
		Act(Combat);
	}
}

void UDwSimPlayerSideController::Act(UDwCombatSubsystem& Combat)
{
	while (bTurnOpen)
	{
		if (Combat.IsResolving())
		{
			return; // latent: OnCommandResolved resumes
		}
		if (!Combat.IsActive() || Combat.GetOutcome() != EDwCombatOutcome::None || Combat.GetActiveSide() != EDwSide::Player)
		{
			bTurnOpen = false;
			return;
		}
		if (Queue.IsEmpty())
		{
			bTurnOpen = false;
			Combat.SubmitCommand(FDwCombatCommand::MakeEndTurn(EDwSide::Player));
			return;
		}

		const FDwUnitHandle Unit = Queue[0];
		const FDwCombatSnapshot Snapshot = Combat.GetSnapshot();
		const FDwUnitSnapshot* Self = Snapshot.FindUnit(Unit);
		FDwCombatCommand Command = FDwCombatCommand::MakeEndUnit(Unit);
		if (Self && Self->bAlive && Brain && TurnContext.Commands < DwSimPlayerSide::MaxCommandsPerUnit)
		{
			Command = Brain->ChooseNextCommand(Snapshot, Unit, TurnContext, Random);
		}
		if (Command.Type != EDwCommandType::Move && Command.Type != EDwCommandType::Ability)
		{
			Queue.RemoveAt(0);
			TurnContext.Reset();
			continue;
		}

		FText Reason;
		const EDwCommandResult Result = Combat.SubmitCommand(Command, &Reason);
		if (Result == EDwCommandResult::Accepted)
		{
			++Actions;
			continue;
		}
		// The brain checks with the same rules, so this is a bug: log it, count it, and move on to the next pirate.
		++Rejected;
		UE_LOG(LogDwSim, Warning, TEXT("Sim command %s rejected (%s: %s)."), *Command.ToString(),
			*StaticEnum<EDwCommandResult>()->GetNameStringByValue(static_cast<int64>(Result)), *Reason.ToString());
		Queue.RemoveAt(0);
		TurnContext.Reset();
	}
}
