// Copyright Brandon Hillig 2026

// DW-106: UDwAiSideController in a real fight (test world, GAS, turn manager). The enemy side acts only through
// UDwCombatSubsystem::SubmitCommand; fights end; recorded AI fights replay from seed + commands.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AI/DwAiProfile.h"
#include "AI/DwAiSideController.h"
#include "AI/DwCrewAiBrain.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tests/Combat/DwCombatTestFixture.h"

BEGIN_DEFINE_SPEC(FDwAiSideControllerSpec, "DeadwaterRun.AI.SideController",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

	FDwCombatFixture F;
	UDwAiProfile* Profile = nullptr;

	UDwAiProfile* MakeProfile()
	{
		UDataTable* Table = F.Root(NewObject<UDataTable>(GetTransientPackage()));
		Table->RowStruct = FDwAiTuningRow::StaticStruct();
		FString Json;
		FFileHelper::LoadFileToString(Json, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Tuning"), TEXT("AiTuning.json")));
		Table->CreateTableFromJSONString(Json);
		UDwAiProfile* NewProfile = F.Root(NewObject<UDwAiProfile>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UDwAiProfile::StaticClass(), TEXT("DA_AiProfile_Test"))));
		NewProfile->Tuning.DataTable = Table;
		NewProfile->Tuning.RowName = TEXT("Default");
		return NewProfile;
	}

	void GiveEnemiesProfile()
	{
		for (FDwCombatantSetup& Enemy : F.Setup.Enemies)
		{
			Enemy.AiProfile = Profile;
		}
	}

	bool IsEnemy(FDwUnitHandle Unit) const { return Unit.Id > F.Setup.Crew.Num(); }

	TArray<FDwCombatCommand> EnemyCommands() const
	{
		return F.Combat->GetRecord().Commands.FilterByPredicate([this](const FDwCombatCommand& C)
		{
			return C.Type != EDwCommandType::EndTurn && IsEnemy(C.Unit);
		});
	}

	/** The player side played by the same crew brain (the simulator's policy, Docs/Architecture/07). */
	void PlayPlayerTurn(UDwCrewAiBrain& Brain, FRandomStream& Rng)
	{
		for (int32 Index = 0; Index < F.Setup.Crew.Num(); ++Index)
		{
			const FDwUnitHandle Unit = FDwCombatFixture::CrewHandle(Index);
			FDwAiTurnContext Context;
			for (int32 Step = 0; Step < UDwAiSideController::MaxCommandsPerUnit && F.Combat->GetOutcome() == EDwCombatOutcome::None; ++Step)
			{
				const FDwCombatCommand Command = Brain.ChooseNextCommand(F.Combat->GetSnapshot(), Unit, Context, Rng);
				if (Command.Type == EDwCommandType::EndUnit || F.Combat->SubmitCommand(Command) != EDwCommandResult::Accepted)
				{
					break;
				}
			}
		}
		if (F.Combat->GetOutcome() == EDwCombatOutcome::None)
		{
			F.EndTurn(EDwSide::Player);
		}
	}

	/** AI against AI until someone wins (or 40 rounds). */
	EDwCombatOutcome Fight(int32 Seed)
	{
		UDwCrewAiBrain* PlayerBrain = F.Root(NewObject<UDwCrewAiBrain>());
		PlayerBrain->Profile = Profile;
		FRandomStream Rng(Seed);
		F.Setup.Seed = Seed;
		if (!F.Start())
		{
			return EDwCombatOutcome::None;
		}
		while (F.Combat->GetOutcome() == EDwCombatOutcome::None && F.Combat->GetRound() <= 40)
		{
			PlayPlayerTurn(*PlayerBrain, Rng);
		}
		return F.Combat->GetOutcome();
	}

END_DEFINE_SPEC(FDwAiSideControllerSpec)

void FDwAiSideControllerSpec::Define()
{
	BeforeEach([this]()
	{
		F.Create();
		F.UseRealActions();
		Profile = MakeProfile();
	});
	AfterEach([this]() { F.Destroy(); });

	It("plays the enemy side through SubmitCommand when enemies have an AI profile", [this]()
	{
		GiveEnemiesProfile();
		if (!TestTrue(TEXT("Started"), F.Start())) { return; }
		TestEqual(TEXT("Player ends turn"), F.EndTurn(), EDwCommandResult::Accepted);

		const TArray<FDwCombatCommand> Commands = EnemyCommands();
		const int32 Actions = Commands.FilterByPredicate([](const FDwCombatCommand& C) { return C.Type == EDwCommandType::Ability || C.Type == EDwCommandType::Move; }).Num();
		const int32 Ends = Commands.FilterByPredicate([](const FDwCombatCommand& C) { return C.Type == EDwCommandType::EndUnit; }).Num();
		TestTrue(*FString::Printf(TEXT("Enemies moved or acted (%d commands)"), Actions), Actions > 0);
		TestEqual(TEXT("Each enemy ended its turn once"), Ends, 3);
		TestEqual(TEXT("Back to the player"), F.Combat->GetActiveSide(), EDwSide::Player);
		TestEqual(TEXT("Round 2"), F.Combat->GetRound(), 2);

		float CrewHealth = 0.f;
		for (int32 Index = 0; Index < 3; ++Index) { CrewHealth += F.Health(FDwCombatFixture::CrewHandle(Index)); }
		TestTrue(*FString::Printf(TEXT("The crew took damage (%.0f / 300)"), CrewHealth), CrewHealth < 300.f);

		const FDwCombatSnapshot Snapshot = F.Combat->GetSnapshot();
		TestEqual(TEXT("Enemies never touch the player's potions"), Snapshot.Items.FindRef(F.Potion->GetFName()), 2);
		TestFalse(TEXT("No enemy item command"), Commands.ContainsByPredicate([](const FDwCombatCommand& C) { return C.AbilityTag == DwTags::Ability_Item; }));
	});

	It("stays passive when no enemy has a profile", [this]()
	{
		if (!TestTrue(TEXT("Started"), F.Start())) { return; }
		F.EndTurn();
		const TArray<FDwCombatCommand> Commands = EnemyCommands();
		TestTrue(TEXT("Only EndUnit"), Commands.Num() == 3 && !Commands.ContainsByPredicate([](const FDwCombatCommand& C) { return C.Type != EDwCommandType::EndUnit; }));
	});

	It("waits for latent abilities to finish and carries on in OnCommandResolved", [this]()
	{
		GiveEnemiesProfile();
		F.World->GetSubsystem<UDwPresentationSubsystem>()->SetInstant(false);
		if (!TestTrue(TEXT("Started"), F.Start())) { return; }
		F.EndTurn();
		TestTrue(TEXT("The first enemy's action is playing"), F.Combat->IsResolving());
		TestEqual(TEXT("Still the enemy turn"), F.Combat->GetActiveSide(), EDwSide::Enemy);

		// Drive ability tasks the way the game's tick would (FTimerManager is unreliable in tests).
		for (float Time = 0.f; Time < 60.f && F.Combat->GetActiveSide() == EDwSide::Enemy && F.Combat->GetOutcome() == EDwCombatOutcome::None; Time += 0.05f)
		{
			for (ADwCombatUnit* Unit : F.Combat->GetUnits())
			{
				Unit->GetDwAbilitySystem()->TickComponent(0.05f, LEVELTICK_All, nullptr);
			}
		}
		TestEqual(TEXT("The enemy turn finished"), F.Combat->GetActiveSide(), EDwSide::Player);
		const TArray<FDwCombatCommand> Commands = EnemyCommands();
		TestTrue(TEXT("Enemies acted"), Commands.ContainsByPredicate([](const FDwCombatCommand& C) { return C.Type == EDwCommandType::Ability; }));
		TestEqual(TEXT("Each enemy ended its turn once"), Commands.FilterByPredicate([](const FDwCombatCommand& C) { return C.Type == EDwCommandType::EndUnit; }).Num(), 3);
	});

	It("fights to an outcome against the same brain on the player side, with no stuck turn", [this]()
	{
		GiveEnemiesProfile();
		const EDwCombatOutcome Outcome = Fight(7);
		TestNotEqual(*FString::Printf(TEXT("Ended in round %d: %s"), F.Combat->GetRound(), *StaticEnum<EDwCombatOutcome>()->GetNameStringByValue(static_cast<int64>(Outcome))),
			Outcome, EDwCombatOutcome::None);
	});

	It("is deterministic per seed, and an AI fight replays from seed + commands alone", [this]()
	{
		GiveEnemiesProfile();
		Fight(21);
		const FDwCombatRecord First = F.Combat->GetRecord();
		const FDwCombatResult FirstResult = F.Combat->GetResult();

		Fight(21);
		bool bSame = F.Combat->GetRecord().Commands.Num() == First.Commands.Num();
		for (int32 Index = 0; bSame && Index < First.Commands.Num(); ++Index)
		{
			bSame = F.Combat->GetRecord().Commands[Index].ToString() == First.Commands[Index].ToString();
		}
		TestTrue(TEXT("Same seed: the same commands"), bSame);

		// Replay with nobody deciding: the AI's own stream never touched the combat stream, so the result matches.
		UDwTestScriptedController* Silent = F.Root(NewObject<UDwTestScriptedController>());
		Silent->bAutoEndUnit = false;
		F.Combat->SetSideController(EDwSide::Enemy, Silent);
		if (!TestTrue(TEXT("Restarted"), F.Start())) { return; }
		for (const FDwCombatCommand& Command : First.Commands)
		{
			if (F.Combat->SubmitCommand(Command) != EDwCommandResult::Accepted)
			{
				AddError(FString::Printf(TEXT("Replay rejected %s"), *Command.ToString()));
				break;
			}
		}
		TestTrue(*FString::Printf(TEXT("Same result: %s vs %s"), *FirstResult.ToString(), *F.Combat->GetResult().ToString()), F.Combat->GetResult() == FirstResult);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
