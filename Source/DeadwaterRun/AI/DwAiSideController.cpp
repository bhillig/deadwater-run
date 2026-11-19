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
	TMap<const UDwAiProfile*, UDwAiBrain*> BrainsByProfile;
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
		UDwAiBrain*& Brain = BrainsByProfile.FindOrAdd(Profile);
		if (!Brain)
		{
			// The profile names its brain (the Kraken's); crews use the utility planner.
			const TSubclassOf<UDwAiBrain> BrainClass = Profile->BrainClass ? Profile->BrainClass : TSubclassOf<UDwAiBrain>(UDwCrewAiBrain::StaticClass());
			Brain = NewObject<UDwAiBrain>(Controller, BrainClass);
			Brain->Configure(Profile, Setup);
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

void UDwAiSideController::OnCombatStarted(UDwCombatSubsystem& Combat)
{
	// First intents, so the player sees the Kraken's opening moves before their first turn (08 "Intents").
	DeclareAllIntents(Combat);
}

void UDwAiSideController::OnSideTurnEnded(UDwCombatSubsystem& Combat, EDwSide Side)
{
	// After the enemy turn's durations ticked: next turn's budget is known (Eyes of the Sea's extra action included).
	if (Side == EDwSide::Enemy)
	{
		DeclareAllIntents(Combat);
	}
}

void UDwAiSideController::DeclareAllIntents(UDwCombatSubsystem& Combat)
{
	TArray<FDwUnitHandle> Handles;
	BrainsByUnit.GetKeys(Handles);
	Handles.Sort([](const FDwUnitHandle& A, const FDwUnitHandle& B) { return A.Id < B.Id; }); // stream order = unit order
	for (const FDwUnitHandle Unit : Handles)
	{
		DeclareIntentsFor(Combat, Unit);
	}
}

void UDwAiSideController::BeginUnitTurn(UDwCombatSubsystem& Combat, FDwUnitHandle Unit)
{
	ActingUnit = Unit;
	TurnContext.Reset();
	if (const UDwAiBrain* Brain = GetBrain(Unit); Brain && Brain->ShouldRedeclare(Combat.GetSnapshot(), Unit))
	{
		UE_LOG(LogDwAI, Log, TEXT("Unit %d re-declares its intents at turn start"), Unit.Id);
		DeclareIntentsFor(Combat, Unit);
	}
	ActForUnit(Combat);
}

void UDwAiSideController::OnCommandResolved(UDwCombatSubsystem& Combat)
{
	if (ActingUnit.IsValid())
	{
		ActForUnit(Combat);
	}
}

void UDwAiSideController::DeclareIntentsFor(UDwCombatSubsystem& Combat, FDwUnitHandle Unit)
{
	const UDwAiBrain* Brain = GetBrain(Unit);
	if (!Brain || !Combat.IsActive())
	{
		return;
	}
	const TArray<FDwAiIntent> Declared = Brain->DeclareIntents(Combat.GetSnapshot(), Unit, Random);
	// Crews declare nothing: don't touch storage (or emit events) for them.
	if (!Declared.IsEmpty() || Combat.GetIntents(Unit))
	{
		Combat.DeclareIntents(Unit, Declared);
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
