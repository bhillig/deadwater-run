// Copyright Brandon Hillig 2026

// DW-201: run state and flow without a world (Docs/Architecture/09-Run-Map.md "How it's tested", 05 "Spawn and write-back").
// Transient content only; the real captains and a real fight are DeadwaterRun.Run.Content.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/DwGameData.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Engine/DataTable.h"
#include "Run/DwRun.h"
#include "Run/DwRunRandom.h"
#include "Run/Map/DwPlaceholderMap.h"
#include "Tests/Run/DwRunTestListener.h"

namespace DwRunSpec
{
	/** Transient captain, crew, tables and encounters: Initialize and the pure flow need nothing else. */
	struct FContent
	{
		TArray<UObject*> Rooted;
		UDwGameData* GameData = nullptr;
		UDwCaptainDefinition* Captain = nullptr;
		UDwUnitDefinition* CaptainUnit = nullptr;
		UDwUnitDefinition* Deckhand = nullptr;
		UDwEncounterDefinition* Early = nullptr;  // layers 0-99
		UDwEncounterDefinition* Late = nullptr;   // layers 4-99
		UDwEncounterDefinition* Kraken = nullptr;
		UDataTable* Economy = nullptr;

		template <typename T>
		T* Make(const TCHAR* Name)
		{
			T* Object = NewObject<T>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), T::StaticClass(), Name));
			Object->AddToRoot();
			Rooted.Add(Object);
			return Object;
		}

		template <typename TRow>
		UDataTable* Table(const TCHAR* RowName, const TRow& Row)
		{
			UDataTable* Result = Make<UDataTable>(TEXT("DT_Test"));
			Result->RowStruct = TRow::StaticStruct();
			Result->AddRow(RowName, Row);
			return Result;
		}

		void Create()
		{
			UDataTable* Units = Make<UDataTable>(TEXT("DT_TestUnits"));
			Units->RowStruct = FDwUnitTuningRow::StaticStruct();
			FDwUnitTuningRow Row;
			Row.MaxHealth = 100; Row.MaxMana = 20; Row.HealthPerLevel = 10;
			Units->AddRow(TEXT("Cap"), Row);
			Row.MaxHealth = 80;
			Units->AddRow(TEXT("Hand"), Row);

			CaptainUnit = Make<UDwUnitDefinition>(TEXT("DA_Unit_TestCaptain"));
			CaptainUnit->DisplayName = FText::FromString(TEXT("Test Captain"));
			CaptainUnit->Tuning.DataTable = Units;
			CaptainUnit->Tuning.RowName = TEXT("Cap");
			Deckhand = Make<UDwUnitDefinition>(TEXT("DA_Unit_TestHand"));
			Deckhand->DisplayName = FText::FromString(TEXT("Deckhand"));
			Deckhand->Tuning.DataTable = Units;
			Deckhand->Tuning.RowName = TEXT("Hand");

			Captain = Make<UDwCaptainDefinition>(TEXT("DA_Captain_Test"));
			Captain->CaptainUnit = CaptainUnit;
			Captain->StartingCrew = { Deckhand, Deckhand };

			FDwEconomyTuningRow EconomyRow;
			EconomyRow.StartingGold = 100;
			Economy = Table(TEXT("Default"), EconomyRow);
			FDwShipTuningRow ShipRow;
			ShipRow.MaxHealth = 250; ShipRow.CannonDamage = 70;

			GameData = Make<UDwGameData>(TEXT("DA_GameData_Test"));
			GameData->TuningTables.Economy = Economy;
			GameData->PlayerShipTuning.DataTable = Table(TEXT("PlayerShip"), ShipRow);
			GameData->PlayerShipTuning.RowName = TEXT("PlayerShip");

			Early = Make<UDwEncounterDefinition>(TEXT("DA_Encounter_TestEarly"));
			Late = Make<UDwEncounterDefinition>(TEXT("DA_Encounter_TestLate"));
			Late->MinLayer = 4;
			Kraken = Make<UDwEncounterDefinition>(TEXT("DA_Encounter_TestKraken"));
			Kraken->Kind = EDwEncounterKind::Kraken;
			GameData->KrakenEncounter = Kraken;
		}

		TArray<const UDwEncounterDefinition*> Pool() const { return { Early, Late, Kraken }; }

		void Destroy()
		{
			for (UObject* Object : Rooted)
			{
				Object->RemoveFromRoot();
			}
			Rooted.Reset();
		}
	};

	/** A fight result where everyone survives at the given Health, the enemy crew dead. */
	static FDwCombatResult Won(const FDwRunState& State, float Health, EDwCombatOutcome Outcome = EDwCombatOutcome::VictoryCrewDefeated)
	{
		FDwCombatResult Result;
		Result.Outcome = Outcome;
		Result.Rounds = 4;
		Result.PlayerShipHealth = State.Ship.Health;
		for (const FDwCrewMemberState& Member : State.Crew)
		{
			if (Member.bAlive)
			{
				FDwCrewResult& Crew = Result.Crew.AddDefaulted_GetRef();
				Crew.RosterId = Member.Id;
				Crew.Health = Health;
			}
		}
		return Result;
	}
}

BEGIN_DEFINE_SPEC(FDwRunSpec, "DeadwaterRun.Rules.Run",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	DwRunSpec::FContent C;
	UDwRun* Run = nullptr;
	UDwRunTestListener* Listener = nullptr;

	UDwRun* NewRun(int32 Seed = 7)
	{
		UDwRun* Result = NewObject<UDwRun>();
		Result->AddToRoot();
		C.Rooted.Add(Result);
		Result->Initialize(*C.GameData, *C.Captain, Seed, C.Pool());
		return Result;
	}

	/** Refused calls log one "... refused: ..." warning each; a test declares exactly how many it causes. */
	void ExpectRefusals(int32 Count)
	{
		AddExpectedError(TEXT("refused"), EAutomationExpectedErrorFlags::Contains, Count);
	}

	/** Sails to the first choice; true if the run is now in Combat. */
	bool SailOn()
	{
		const TArray<int32> Choices = Run->GetChoices();
		return TestTrue(TEXT("Has a route"), !Choices.IsEmpty()) && TestTrue(TEXT("Chosen"), Run->ChooseNode(Choices[0]));
	}
END_DEFINE_SPEC(FDwRunSpec)

void FDwRunSpec::Define()
{
	using namespace DwRunSpec;

	BeforeEach([this]()
	{
		C.Create();
		Run = NewRun();
		Listener = NewObject<UDwRunTestListener>();
		Listener->AddToRoot();
		C.Rooted.Add(Listener);
		Run->OnPhaseChanged.AddDynamic(Listener, &UDwRunTestListener::HandlePhaseChanged);
		Run->OnStateChanged.AddDynamic(Listener, &UDwRunTestListener::HandleStateChanged);
	});
	AfterEach([this]() { C.Destroy(); Run = nullptr; Listener = nullptr; });

	Describe("Initialize", [this]()
	{
		It("builds the roster from the captain: captain first, starting crew after, full Health, level 1", [this]()
		{
			const FDwRunState& S = Run->GetState();
			if (!TestEqual(TEXT("Captain + 2 crew"), S.Crew.Num(), 3)) { return; }
			TestTrue(TEXT("Captain first"), S.Crew[0].bIsCaptain && S.Crew[0].Unit == C.CaptainUnit);
			TestTrue(TEXT("Crew after"), !S.Crew[1].bIsCaptain && S.Crew[1].Unit == C.Deckhand && S.Crew[2].Unit == C.Deckhand);
			TestEqual(TEXT("Captain HP"), S.Crew[0].Health, 100.f);
			TestEqual(TEXT("Deckhand HP"), S.Crew[1].Health, 80.f);
			for (const FDwCrewMemberState& Member : S.Crew)
			{
				TestTrue(TEXT("Alive, level 1, no XP"), Member.bAlive && Member.Level == 1 && Member.Xp == 0);
				TestTrue(TEXT("Has an id"), Member.Id.IsValid());
			}
			TestTrue(TEXT("Unique ids"), S.Crew[0].Id != S.Crew[1].Id && S.Crew[1].Id != S.Crew[2].Id);
			TestEqual(TEXT("Captain lookup"), S.GetCaptainMember(), &S.Crew[0]);
		});

		It("starts on the map with the starting gold and a full ship", [this]()
		{
			const FDwRunState& S = Run->GetState();
			TestEqual(TEXT("Map phase"), S.Phase, EDwRunPhase::Map);
			TestEqual(TEXT("Before the first node"), S.CurrentNode, INDEX_NONE);
			TestEqual(TEXT("100 gold"), S.Gold, 100);
			TestEqual(TEXT("Ship 250/250"), S.Ship.Health, 250.f);
			TestEqual(TEXT("Max 250"), S.Ship.MaxHealth, 250.f);
			TestEqual(TEXT("Cannons 70"), S.Ship.CannonDamage, 70.f);
			TestTrue(TEXT("Empty inventory"), S.Inventory.IsEmpty());
			TestEqual(TEXT("Stats know the captain"), S.Stats.Captain.Get(), static_cast<const UDwCaptainDefinition*>(C.Captain));
			TestEqual(TEXT("Stats know the seed"), S.Stats.Seed, 7);
		});

		It("reports missing content instead of starting a broken run", [this]()
		{
			C.Captain->CaptainUnit = nullptr;
			C.Economy->EmptyTable();
			UDwRun* Broken = NewObject<UDwRun>();
			TArray<FString> Errors;
			AddExpectedError(TEXT("Run setup"), EAutomationExpectedErrorFlags::Contains, 2);
			TestFalse(TEXT("Refused"), Broken->Initialize(*C.GameData, *C.Captain, 1, C.Pool(), &Errors));
			TestEqual(TEXT("One message per problem"), Errors.Num(), 2);
		});

		It("is deterministic per seed: same roster ids and the same chart", [this]()
		{
			const UDwRun* A = NewRun(42);
			const UDwRun* B = NewRun(42);
			const UDwRun* Other = NewRun(43);
			for (int32 Index = 0; Index < A->GetState().Crew.Num(); ++Index)
			{
				TestEqual(TEXT("Same id"), A->GetState().Crew[Index].Id, B->GetState().Crew[Index].Id);
			}
			TestNotEqual(TEXT("Other seed, other ids"), A->GetState().Crew[0].Id, Other->GetState().Crew[0].Id);
			const TArray<FDwMapNode>& NodesA = A->GetState().Map.Nodes;
			const TArray<FDwMapNode>& NodesB = B->GetState().Map.Nodes;
			if (!TestEqual(TEXT("Same node count"), NodesA.Num(), NodesB.Num())) { return; }
			for (int32 Index = 0; Index < NodesA.Num(); ++Index)
			{
				TestTrue(TEXT("Same node"), NodesA[Index].Encounter == NodesB[Index].Encounter && NodesA[Index].Type == NodesB[Index].Type
					&& NodesA[Index].Next == NodesB[Index].Next);
			}
		});
	});

	Describe("Placeholder map", [this]()
	{
		It("has 8 layers of 3 lanes, all fights, then the boss", [this]()
		{
			const FDwMapGraph& Map = Run->GetState().Map;
			TestEqual(TEXT("Layers"), Map.LayerCount, 8);
			TestEqual(TEXT("24 fights + boss"), Map.Nodes.Num(), 25);
			if (!TestTrue(TEXT("Boss node"), Map.IsValidNode(Map.BossNode))) { return; }
			TestEqual(TEXT("Boss type"), Map.Nodes[Map.BossNode].Type, EDwNodeType::Boss);
			TestEqual(TEXT("Boss encounter = DA_GameData.KrakenEncounter"), Map.Nodes[Map.BossNode].Encounter.Get(),
				static_cast<const UDwEncounterDefinition*>(C.Kraken));
			for (int32 Index = 0; Index < Map.Nodes.Num(); ++Index)
			{
				const FDwMapNode& Node = Map.Nodes[Index];
				if (Index == Map.BossNode)
				{
					TestTrue(TEXT("Boss is the end"), Node.Next.IsEmpty());
					continue;
				}
				TestEqual(TEXT("Fight"), Node.Type, EDwNodeType::Fight);
				TestTrue(TEXT("Has a ship battle"), Node.Encounter && Node.Encounter->Kind == EDwEncounterKind::ShipBattle);
				if (Node.Layer < 4)
				{
					TestTrue(TEXT("Tiered: no late crew before layer 4"), Node.Encounter == C.Early);
				}
				if (Node.Layer == Map.LayerCount - 1)
				{
					TestTrue(TEXT("Last layer -> boss"), Node.Next.Num() == 1 && Node.Next[0] == Map.BossNode);
				}
				else
				{
					TestEqual(TEXT("2 choices on the edges, 3 in the middle"), Node.Next.Num(), Node.Lane == 1 ? 3 : 2);
					for (const int32 To : Node.Next)
					{
						TestEqual(TEXT("Forward one layer"), Map.Nodes[To].Layer, Node.Layer + 1);
						TestTrue(TEXT("Same or neighbouring lane"), FMath::Abs(Map.Nodes[To].Lane - Node.Lane) <= 1);
					}
				}
			}
		});

		It("offers every first-layer node from the start", [this]()
		{
			const TArray<int32> Choices = Run->GetChoices();
			TestEqual(TEXT("3 routes"), Choices.Num(), 3);
			for (const int32 Index : Choices)
			{
				TestEqual(TEXT("Layer 0"), Run->GetState().Map.Nodes[Index].Layer, 0);
			}
		});

		It("leaves a fight empty when no ship battle is allowed on its layer", [this]()
		{
			FRandomStream Random(1);
			const TArray<const UDwEncounterDefinition*> OnlyLate = { C.Late };
			TestNull(TEXT("Layer 2 has none"), FDwPlaceholderMap::PickEncounter(OnlyLate, 2, Random));
			TestEqual(TEXT("Layer 5 has it"), FDwPlaceholderMap::PickEncounter(OnlyLate, 5, Random), static_cast<const UDwEncounterDefinition*>(C.Late));
		});
	});

	Describe("ChooseNode", [this]()
	{
		It("sails to a reachable fight: visited, current, Combat", [this]()
		{
			const int32 Target = Run->GetChoices()[1];
			TestTrue(TEXT("Accepted"), Run->ChooseNode(Target));
			const FDwRunState& S = Run->GetState();
			TestEqual(TEXT("Current"), S.CurrentNode, Target);
			TestTrue(TEXT("Visited"), S.Map.Nodes[Target].bVisited);
			TestEqual(TEXT("Combat"), S.Phase, EDwRunPhase::Combat);
			TestEqual(TEXT("One phase change"), Listener->Phases.Num(), 1);
			TestTrue(TEXT("Map -> Combat"), Listener->Phases.Num() == 1 && Listener->Phases[0].Key == EDwRunPhase::Combat
				&& Listener->Phases[0].Value == EDwRunPhase::Map);
		});

		It("refuses nodes that are not ahead of the current one, and changes nothing", [this]()
		{
			ExpectRefusals(4);
			const FDwMapGraph& Map = Run->GetState().Map;
			const int32 LayerTwo = Map.Nodes.IndexOfByPredicate([](const FDwMapNode& Node) { return Node.Layer == 2; });
			TestFalse(TEXT("Skipping a layer"), Run->ChooseNode(LayerTwo));
			TestFalse(TEXT("The boss"), Run->ChooseNode(Map.BossNode));
			TestFalse(TEXT("Out of range"), Run->ChooseNode(999));
			TestFalse(TEXT("Negative"), Run->ChooseNode(-1));
			TestEqual(TEXT("Still at the start"), Run->GetState().CurrentNode, INDEX_NONE);
			TestEqual(TEXT("Still on the map"), Run->GetPhase(), EDwRunPhase::Map);
			TestEqual(TEXT("No broadcasts"), Listener->StateChanges, 0);
		});

		It("refuses going back or sideways after a fight", [this]()
		{
			ExpectRefusals(2);
			if (!SailOn()) { return; }
			const int32 First = Run->GetState().CurrentNode;
			const int32 Sibling = First + 1; // lane 1 of layer 0
			Run->ApplyCombatResult(Won(Run->GetState(), 50.f));
			TestFalse(TEXT("Same node"), Run->ChooseNode(First));
			TestFalse(TEXT("Same layer"), Run->ChooseNode(Sibling));
			TestTrue(TEXT("Next layer is fine"), Run->ChooseNode(Run->GetChoices()[0]));
		});

		It("only works on the map", [this]()
		{
			ExpectRefusals(1);
			if (!SailOn()) { return; }
			const TArray<int32> Ahead = Run->GetState().Map.GetChoices(Run->GetState().CurrentNode);
			TestTrue(TEXT("No choices offered mid-fight"), Run->GetChoices().IsEmpty());
			TestFalse(TEXT("Refused in Combat"), Run->ChooseNode(Ahead[0]));
		});
	});

	Describe("ApplyCombatResult", [this]()
	{
		It("writes Health back and returns to the map", [this]()
		{
			if (!SailOn()) { return; }
			FDwCombatResult Result = Won(Run->GetState(), 55.f);
			Result.Crew[1].Health = 500.f; // clamped to max
			Result.PlayerShipHealth = 180.f;
			TestTrue(TEXT("Applied"), Run->ApplyCombatResult(Result));
			const FDwRunState& S = Run->GetState();
			TestEqual(TEXT("Captain 55"), S.Crew[0].Health, 55.f);
			TestEqual(TEXT("Clamped to 80"), S.Crew[1].Health, 80.f);
			TestEqual(TEXT("Ship 180"), S.Ship.Health, 180.f);
			TestEqual(TEXT("Map"), S.Phase, EDwRunPhase::Map);
			TestEqual(TEXT("Cleared 1"), S.Stats.NodesCleared, 1);
			TestEqual(TEXT("Plundered 1"), S.Stats.FightsPlundered, 1);
			TestEqual(TEXT("Rounds"), S.Stats.Rounds, 4);
			TestEqual(TEXT("Next layer offered"), Run->GetChoices().Num(), S.Map.Nodes[S.CurrentNode].Next.Num());
		});

		It("applies permadeath: the dead stay dead, in the roster, and in the stats", [this]()
		{
			if (!SailOn()) { return; }
			const int32 Node = Run->GetState().CurrentNode;
			FDwCombatResult Result = Won(Run->GetState(), 40.f);
			Result.Crew[2].bAlive = false;
			Result.Crew[2].Health = 0.f;
			Run->ApplyCombatResult(Result);
			const FDwRunState& S = Run->GetState();
			TestEqual(TEXT("Still 3 in the roster"), S.Crew.Num(), 3);
			TestFalse(TEXT("Dead"), S.Crew[2].bAlive);
			TestEqual(TEXT("0 HP"), S.Crew[2].Health, 0.f);
			TestEqual(TEXT("2 living"), S.CountLivingCrew(), 2);
			if (TestEqual(TEXT("One loss"), S.Stats.CrewLost.Num(), 1))
			{
				TestEqual(TEXT("Who"), S.Stats.CrewLost[0].CrewId, S.Crew[2].Id);
				TestEqual(TEXT("Where"), S.Stats.CrewLost[0].Node, Node);
				TestEqual(TEXT("Name"), S.Stats.CrewLost[0].Name.ToString(), FString(TEXT("Deckhand")));
			}

			// A later result that claims the dead one lived (a bug elsewhere) never revives them.
			Run->ChooseNode(Run->GetChoices()[0]);
			FDwCombatResult Lie = Won(Run->GetState(), 40.f);
			FDwCrewResult& Ghost = Lie.Crew.AddDefaulted_GetRef();
			Ghost.RosterId = S.Crew[2].Id;
			Ghost.Health = 80.f;
			Run->ApplyCombatResult(Lie);
			TestFalse(TEXT("Never revived"), Run->GetState().Crew[2].bAlive);
			TestEqual(TEXT("Lost once"), Run->GetState().Stats.CrewLost.Num(), 1);
		});

		It("counts crew kills of enemies, not sinkings or friendly deaths", [this]()
		{
			if (!SailOn()) { return; }
			FDwCombatResult Result = Won(Run->GetState(), 90.f);
			FDwKillRecord Kill;
			Kill.KillerRosterId = Run->GetState().Crew[0].Id;
			Kill.VictimSide = EDwSide::Enemy;
			Result.Kills = { Kill, Kill };
			FDwKillRecord Sunk; // no killer (C11)
			Sunk.VictimSide = EDwSide::Enemy;
			Result.Kills.Add(Sunk);
			Run->ApplyCombatResult(Result);
			TestEqual(TEXT("Two kills"), Run->GetState().Stats.Kills, 2);
		});

		It("ends the run on every defeat, including captain death (C10)", [this]()
		{
			const TPair<EDwCombatOutcome, EDwRunEndReason> Cases[] = {
				{ EDwCombatOutcome::DefeatShipSunk, EDwRunEndReason::ShipSunk },
				{ EDwCombatOutcome::DefeatCrewDead, EDwRunEndReason::CrewDead },
				{ EDwCombatOutcome::DefeatCaptainDead, EDwRunEndReason::CaptainDead } };
			for (const TPair<EDwCombatOutcome, EDwRunEndReason>& Case : Cases)
			{
				Run = NewRun();
				if (!SailOn()) { return; }
				Run->ApplyCombatResult(Won(Run->GetState(), 10.f, Case.Key));
				TestEqual(TEXT("Defeat"), Run->GetPhase(), EDwRunPhase::Defeat);
				TestEqual(TEXT("Reason"), Run->GetState().Stats.EndReason, Case.Value);
				TestTrue(TEXT("Over"), Run->IsOver());
				TestTrue(TEXT("Nowhere to sail"), Run->GetChoices().IsEmpty());
				TestEqual(TEXT("A lost fight is not a cleared node"), Run->GetState().Stats.NodesCleared, 0);
			}
		});

		It("wins the run by beating the boss", [this]()
		{
			for (int32 Layer = 0; Layer < Run->GetState().Map.LayerCount; ++Layer)
			{
				if (!SailOn()) { return; }
				Run->ApplyCombatResult(Won(Run->GetState(), 70.f, Layer % 2 ? EDwCombatOutcome::VictorySunk : EDwCombatOutcome::VictoryCrewDefeated));
			}
			const TArray<int32> Last = Run->GetChoices();
			if (!TestTrue(TEXT("Only the boss is left"), Last.Num() == 1 && Last[0] == Run->GetState().Map.BossNode)) { return; }
			TestTrue(TEXT("To the boss"), Run->ChooseNode(Last[0]));
			TestEqual(TEXT("Boss fight"), Run->GetPhase(), EDwRunPhase::Combat);
			Run->ApplyCombatResult(Won(Run->GetState(), 70.f, EDwCombatOutcome::VictoryBoss));
			const FDwRunState& S = Run->GetState();
			TestEqual(TEXT("Victory"), S.Phase, EDwRunPhase::Victory);
			TestEqual(TEXT("Reason"), S.Stats.EndReason, EDwRunEndReason::Victory);
			TestEqual(TEXT("8 fights + boss cleared"), S.Stats.NodesCleared, 9);
			TestEqual(TEXT("Sunk 4"), S.Stats.FightsSunk, 4);
			TestEqual(TEXT("Plundered 4"), S.Stats.FightsPlundered, 4);
		});

		It("refuses a result without an outcome, or outside combat", [this]()
		{
			ExpectRefusals(2);
			TestFalse(TEXT("On the map"), Run->ApplyCombatResult(Won(Run->GetState(), 10.f)));
			if (!SailOn()) { return; }
			TestFalse(TEXT("No outcome"), Run->ApplyCombatResult(Won(Run->GetState(), 10.f, EDwCombatOutcome::None)));
			TestEqual(TEXT("Still fighting"), Run->GetPhase(), EDwRunPhase::Combat);
			TestEqual(TEXT("Health untouched"), Run->GetState().Crew[0].Health, 100.f);
		});
	});

	Describe("Abandon", [this]()
	{
		It("ends the run as Defeat (Abandoned) from the map or mid-fight, once", [this]()
		{
			ExpectRefusals(3);
			TestTrue(TEXT("From the map"), Run->Abandon());
			TestEqual(TEXT("Defeat"), Run->GetPhase(), EDwRunPhase::Defeat);
			TestEqual(TEXT("Abandoned"), Run->GetState().Stats.EndReason, EDwRunEndReason::Abandoned);
			TestFalse(TEXT("Only once"), Run->Abandon());
			TestFalse(TEXT("No sailing after"), Run->ChooseNode(0));

			Run = NewRun();
			if (!SailOn()) { return; }
			TestTrue(TEXT("Mid-fight"), Run->Abandon());
			TestFalse(TEXT("The fight's result no longer applies"), Run->ApplyCombatResult(Won(Run->GetState(), 10.f)));
		});

		It("keeps the summary stats", [this]()
		{
			if (!SailOn()) { return; }
			Run->ApplyCombatResult(Won(Run->GetState(), 60.f));
			Run->Abandon();
			Run->SetElapsedSeconds(123.f);
			const FDwRunStats& Stats = Run->GetState().Stats;
			TestEqual(TEXT("Cleared"), Stats.NodesCleared, 1);
			TestEqual(TEXT("Time"), Stats.ElapsedSeconds, 123.f);
			TestEqual(TEXT("Seed"), Stats.Seed, 7);
		});
	});

	Describe("Shop and event nodes", [this]()
	{
		It("can't be left from another phase", [this]()
		{
			ExpectRefusals(2);
			TestFalse(TEXT("LeaveShop on the map"), Run->LeaveShop());
			TestFalse(TEXT("LeaveEvent on the map"), Run->LeaveEvent());
		});
	});

	Describe("FDwRunRandom", [this]()
	{
		It("derives stable, independent, non-negative streams", [this]()
		{
			TestEqual(TEXT("Stable"), FDwRunRandom::ForNode(7, 3), FDwRunRandom::ForNode(7, 3));
			TestNotEqual(TEXT("Per node"), FDwRunRandom::ForNode(7, 3), FDwRunRandom::ForNode(7, 4));
			TestNotEqual(TEXT("Per seed"), FDwRunRandom::ForNode(7, 3), FDwRunRandom::ForNode(8, 3));
			TestNotEqual(TEXT("Per purpose"), FDwRunRandom::ForNode(7, 3), FDwRunRandom::ForRewards(7, 3));
			TestNotEqual(TEXT("Map attempts differ"), FDwRunRandom::ForMap(7, 0), FDwRunRandom::ForMap(7, 1));
			for (int32 Seed : { 0, 1, -1, MAX_int32, MIN_int32 })
			{
				TestTrue(TEXT("Non-negative"), FDwRunRandom::ForNode(Seed, 0) >= 0 && FDwRunRandom::ForMap(Seed, 0) >= 0 && FDwRunRandom::ForCrew(Seed) >= 0);
			}
			TestTrue(TEXT("Clock seed non-negative"), FDwRunRandom::MakeRunSeed() >= 0);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
