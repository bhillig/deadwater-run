// Copyright Brandon Hillig 2026

#include "AI/DwAiSideController.h"

#include "AI/DwAiProfile.h"
#include "AI/DwCrewAiBrain.h"
#include "Combat/DwCombatSubsystem.h"
#include "Core/DwLog.h"

namespace DwAiSideController
{
	/** Keeps the AI stream independent of the combat stream while still following the encounter seed. */
	static constexpr uint32 StreamSalt = 0xD7A1C0DEu;
}

UDwAiSideController* UDwAiSideController::CreateForEncounter(UObject* Outer, const FDwEncounterSetup& Setup, TConstArrayView<FDwUnitHandle> EnemyHandles)
{
	UDwAiSideController* Controller = nullptr;
	TMap<const UDwAiProfile*, UDwCrewAiBrain*> BrainsByProfile;
	for (int32 Index = 0; Index < Setup.Enemies.Num() && Index < EnemyHandles.Num(); ++Index)
	{
		const UDwAiProfile* Profile = Setup.Enemies[Index].AiProfile;
		if (!Profile)
		{
			continue;
		}
		if (!Controller)
		{
			Controller = NewObject<UDwAiSideController>(Outer);
			Controller->SetSeed(Setup.Seed);
		}
		UDwCrewAiBrain*& Brain = BrainsByProfile.FindOrAdd(Profile);
		if (!Brain)
		{
			Brain = NewObject<UDwCrewAiBrain>(Controller);
			Brain->Profile = Profile;
		}
		Controller->SetBrain(EnemyHandles[Index], Brain);
	}
	return Controller;
}

void UDwAiSideController::SetBrain(FDwUnitHandle Unit, UDwAiBrain* Brain)
{
	if (Brain)
	{
		BrainsByUnit.Add(Unit, Brain);
	}
	else
	{
		BrainsByUnit.Remove(Unit);
	}
}

UDwAiBrain* UDwAiSideController::GetBrain(FDwUnitHandle Unit) const
{
	return BrainsByUnit.FindRef(Unit);
}

void UDwAiSideController::SetSeed(int32 EncounterSeed)
{
	Random.Initialize(static_cast<int32>(HashCombineFast(GetTypeHash(EncounterSeed), DwAiSideController::StreamSalt)));
}

void UDwAiSideController::BeginUnitTurn(UDwCombatSubsystem& Combat, FDwUnitHandle Unit)
{
	ActingUnit = Unit;
	TurnContext.Reset();
	ActForUnit(Combat);
}

void UDwAiSideController::OnCommandResolved(UDwCombatSubsystem& Combat)
{
	if (ActingUnit.IsValid())
	{
		ActForUnit(Combat);
	}
}

void UDwAiSideController::ActForUnit(UDwCombatSubsystem& Combat)
{
	while (ActingUnit.IsValid())
	{
		if (Combat.IsResolving())
		{
			return; // latent: OnCommandResolved resumes
		}
		if (!Combat.IsActive() || Combat.GetOutcome() != EDwCombatOutcome::None)
		{
			ActingUnit = FDwUnitHandle();
			return;
		}
		const FDwCombatSnapshot Snapshot = Combat.GetSnapshot();
		const FDwUnitHandle Unit = ActingUnit;
		if (Snapshot.ActingUnit != Unit)
		{
			ActingUnit = FDwUnitHandle(); // the sequence moved on without us
			return;
		}

		const UDwAiBrain* Brain = GetBrain(Unit);
		if (!Brain || TurnContext.Commands >= MaxCommandsPerUnit)
		{
			if (Brain)
			{
				UE_LOG(LogDwAI, Warning, TEXT("Unit %d hit the %d-command limit; ending its turn."), Unit.Id, MaxCommandsPerUnit);
			}
			FinishUnit(Combat, Unit);
			return;
		}
		const FDwCombatCommand Command = Brain->ChooseNextCommand(Snapshot, Unit, TurnContext, Random);
		if (Command.Type == EDwCommandType::EndUnit || Command.Type == EDwCommandType::EndTurn || Command.Unit != Unit)
		{
			FinishUnit(Combat, Unit);
			return;
		}

		UE_LOG(LogDwAI, Log, TEXT("AI %s"), *Command.ToString());
		FText Reason;
		const EDwCommandResult Result = Combat.SubmitCommand(Command, &Reason);
		if (Result != EDwCommandResult::Accepted)
		{
			// The brain checks with the same rules, so this is a bug: log it and keep the fight moving.
			UE_LOG(LogDwAI, Warning, TEXT("AI command %s rejected (%s: %s); ending the unit's turn."), *Command.ToString(),
				*StaticEnum<EDwCommandResult>()->GetNameStringByValue(static_cast<int64>(Result)), *Reason.ToString());
			FinishUnit(Combat, Unit);
			return;
		}
	}
}

void UDwAiSideController::FinishUnit(UDwCombatSubsystem& Combat, FDwUnitHandle Unit)
{
	// Clear first: EndUnit can start the next enemy's turn on this same controller before SubmitCommand returns.
	ActingUnit = FDwUnitHandle();
	TurnContext.Reset();
	Combat.SubmitCommand(FDwCombatCommand::MakeEndUnit(Unit));
}
