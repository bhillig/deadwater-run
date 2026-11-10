// Copyright Brandon Hillig 2026

// DW-106: the crew brain is deterministic for a seed and plans a full enemy turn within the simulator budget
// (Docs/Architecture/07-Enemy-AI.md: 1,000 seeded calls agree; a full enemy turn plans in under 5 ms on a 12x24 board).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/PlatformTime.h"
#include "Tests/AI/DwAiTestBench.h"

namespace DwAiPerformanceSpec
{
	/** A 12 x 12 deck facing right: crates, masts, three cannons, a ladder and nest by the boarding edge. Two make a 24 x 12 board. */
	static TArray<FString> BigDeck()
	{
		return {
			TEXT("S.....c..LNE"),
			TEXT("S..C......cE"),
			TEXT("S....M.....E"),
			TEXT("S..c....C..E"),
			TEXT("S..........E"),
			TEXT("S...M..c...E"),
			TEXT("S..........E"),
			TEXT("S.c....C...E"),
			TEXT("S....c.....E"),
			TEXT("S..C...M...E"),
			TEXT("S.......c..E"),
			TEXT("S..........E"),
		};
	}

	static constexpr double BudgetMs = 5.0;
}

BEGIN_DEFINE_SPEC(FDwAiPerformanceSpec, "DeadwaterRun.AI.Performance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

	FDwAiTestBench B;
	TArray<FDwUnitHandle> Enemies;

	/** 6 v 6, swords and rifles alternating. bEngaged: both crews at the boarding edges; otherwise at their spawns. */
	void Populate(bool bEngaged)
	{
		B.SetBoard(DwAiPerformanceSpec::BigDeck(), DwAiPerformanceSpec::BigDeck());
		const TArray<FIntPoint> Player = bEngaged
			? TArray<FIntPoint>{ { 9, 1 }, { 10, 3 }, { 9, 5 }, { 10, 7 }, { 9, 9 }, { 10, 11 } }
			: TArray<FIntPoint>{ { 1, 1 }, { 1, 3 }, { 1, 5 }, { 1, 7 }, { 1, 9 }, { 1, 11 } };
		const TArray<FIntPoint> Enemy = bEngaged
			? TArray<FIntPoint>{ { 14, 1 }, { 13, 3 }, { 14, 5 }, { 13, 7 }, { 14, 9 }, { 13, 11 } }
			: TArray<FIntPoint>{ { 22, 1 }, { 22, 3 }, { 22, 5 }, { 22, 7 }, { 22, 9 }, { 22, 11 } };
		for (int32 Index = 0; Index < Player.Num(); ++Index)
		{
			FDwUnitSnapshot& Crew = B.AddUnit(EDwSide::Player, Player[Index], Index % 2 ? B.Rifle : B.Sword);
			if (Index == 0)
			{
				Crew.Tags.AddTag(DwTags::Unit_Role_Captain);
			}
		}
		Enemies.Reset();
		for (int32 Index = 0; Index < Enemy.Num(); ++Index)
		{
			Enemies.Add(B.AddUnit(EDwSide::Enemy, Enemy[Index], Index % 2 ? B.Rifle : B.Sword).Handle);
		}
	}

	/** Every enemy plays its turn in order on the snapshot. Returns the planning time in ms and the commands. */
	double PlayEnemyTurn(int32 Seed, TArray<FDwCombatCommand>* OutCommands = nullptr)
	{
		FRandomStream Rng(Seed);
		const double Start = FPlatformTime::Seconds();
		for (const FDwUnitHandle Unit : Enemies)
		{
			if (!B.Unit(Unit).bAlive)
			{
				continue;
			}
			const TArray<FDwCombatCommand> Commands = B.PlayTurn(Unit, Rng);
			if (OutCommands)
			{
				OutCommands->Append(Commands);
			}
		}
		return (FPlatformTime::Seconds() - Start) * 1000.0;
	}

	void Measure(bool bEngaged, const TCHAR* Label)
	{
		Populate(bEngaged);
		const FDwCombatSnapshot Start = B.Snapshot;
		TArray<double> Times;
		int32 Commands = 0;
		for (int32 Trial = 0; Trial < 21; ++Trial)
		{
			B.Snapshot = Start;
			TArray<FDwCombatCommand> Played;
			Times.Add(PlayEnemyTurn(Trial + 1, &Played));
			Commands = Played.Num();
		}
		Times.Sort();
		const double Median = Times[Times.Num() / 2];
		AddInfo(FString::Printf(TEXT("%s: 6 enemies, %d commands, enemy turn planned in %.3f ms median (min %.3f, max %.3f) on a %dx%d board"),
			Label, Commands, Median, Times[0], Times.Last(), B.Snapshot.Grid.Size.Y, B.Snapshot.Grid.Size.X));
		TestTrue(*FString::Printf(TEXT("%s: median %.3f ms under %.1f ms"), Label, Median, DwAiPerformanceSpec::BudgetMs), Median < DwAiPerformanceSpec::BudgetMs);
	}

END_DEFINE_SPEC(FDwAiPerformanceSpec)

void FDwAiPerformanceSpec::Define()
{
	BeforeEach([this]()
	{
		B.Create();
		TestTrue(TEXT("Tuning/AiTuning.json has a Default row"), B.HasDefaultTuning());
	});
	AfterEach([this]() { B.Destroy(); });

	Describe("Budget", [this]()
	{
		It("plans a full enemy turn in under 5 ms on a 12x24 board (crews at their spawns)", [this]() { Measure(false, TEXT("Opening")); });
		It("plans a full enemy turn in under 5 ms on a 12x24 board (crews engaged at the edge)", [this]() { Measure(true, TEXT("Engaged")); });
	});

	Describe("Determinism", [this]()
	{
		It("gives the same command for 1,000 seeded calls on the same snapshot", [this]()
		{
			Populate(true);
			B.SetActing(Enemies[0]);
			for (const float Temperature : { 0.f, 4.f })
			{
				FDwAiTuningRow Tuning = B.DefaultTuning();
				Tuning.Temperature = Temperature;
				B.Brain->SetTuningOverride(Tuning);
				const FDwCombatSnapshot Before = B.Snapshot;
				FString First;
				int32 Same = 0;
				for (int32 Call = 0; Call < 1000; ++Call)
				{
					FDwAiTurnContext Context;
					FRandomStream Rng(1234);
					const FString Command = B.Brain->ChooseNextCommand(B.Snapshot, Enemies[0], Context, Rng).ToString();
					First = Call == 0 ? Command : First;
					Same += Command == First ? 1 : 0;
				}
				TestEqual(*FString::Printf(TEXT("Temperature %.0f: 1000/1000 identical (%s)"), Temperature, *First), Same, 1000);
				TestTrue(TEXT("The snapshot is only read"), B.Snapshot.Units.Num() == Before.Units.Num() && B.Snapshot.Units[0].Tile == Before.Units[0].Tile);
			}
		});

		It("plays the same whole enemy turn from the same snapshot and seed", [this]()
		{
			Populate(true);
			const FDwCombatSnapshot Start = B.Snapshot;
			TArray<FDwCombatCommand> First;
			PlayEnemyTurn(99, &First);
			B.Snapshot = Start;
			TArray<FDwCombatCommand> Second;
			PlayEnemyTurn(99, &Second);
			TestEqual(TEXT("Same number of commands"), Second.Num(), First.Num());
			bool bSame = First.Num() == Second.Num();
			for (int32 Index = 0; bSame && Index < First.Num(); ++Index)
			{
				bSame = First[Index].ToString() == Second[Index].ToString();
			}
			TestTrue(TEXT("Identical commands"), bSame);
			TestTrue(TEXT("The enemies did something"), First.ContainsByPredicate([](const FDwCombatCommand& C) { return C.Type == EDwCommandType::Ability; }));
		});

		It("draws only from the stream it is given: with Temperature > 0, other seeds can choose differently", [this]()
		{
			Populate(true);
			B.SetActing(Enemies[0]);
			FDwAiTuningRow Tuning = B.DefaultTuning();
			Tuning.Temperature = 50.f; // nearly uniform: many candidates are plausible
			B.Brain->SetTuningOverride(Tuning);
			TSet<FString> Seen;
			for (int32 Seed = 1; Seed <= 50; ++Seed)
			{
				FDwAiTurnContext Context;
				FRandomStream Rng(Seed);
				Seen.Add(B.Brain->ChooseNextCommand(B.Snapshot, Enemies[0], Context, Rng).ToString());
			}
			TestTrue(*FString::Printf(TEXT("%d distinct choices over 50 seeds"), Seen.Num()), Seen.Num() > 1);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
