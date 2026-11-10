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
#include "Tests/Run/DwRunTestContent.h"
#include "Tests/Run/DwRunTestListener.h"

BEGIN_DEFINE_SPEC(FDwRunSpec, "DeadwaterRun.Rules.Run",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	DwRunTest::FContent C;
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

	/** After a won fight: claim the rewards (no new move) and take the first perk of every level-up, back to the map. */
	bool FinishNode()
	{
		if (!TestEqual(TEXT("Rewards after a won fight"), Run->GetPhase(), EDwRunPhase::Rewards) || !Run->ClaimRewards(INDEX_NONE, FGuid()))
		{
			return false;
		}
		while (const FDwPendingLevelUp* LevelUp = Run->GetPhase() == EDwRunPhase::LevelUp ? Run->GetCurrentLevelUp() : nullptr)
		{
			if (!TestTrue(TEXT("Perk taken"), Run->ChoosePerk(LevelUp->PerkChoices.IsEmpty() ? INDEX_NONE : 0)))
			{
				return false;
			}
		}
		return TestEqual(TEXT("Back on the map"), Run->GetPhase(), EDwRunPhase::Map);
	}

	FDwCrewMemberState& CrewAt(int32 Index) { return const_cast<FDwCrewMemberState&>(Run->GetState().Crew[Index]); }
END_DEFINE_SPEC(FDwRunSpec)

void FDwRunSpec::Define()
{
	using namespace DwRunTest;

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
			if (!FinishNode()) { return; }
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
			TestEqual(TEXT("Rewards first"), S.Phase, EDwRunPhase::Rewards);
			TestEqual(TEXT("Not cleared before the rewards are taken"), S.Stats.NodesCleared, 0);
			if (!FinishNode()) { return; }
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

			if (!FinishNode()) { return; }
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
				if (!FinishNode()) { return; }
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
			if (!FinishNode()) { return; }
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

	Describe("Rewards (DW-203)", [this]()
	{
		It("follows a won fight with Rewards: plunder pays 1.5x gold, one item and three new moves; claiming banks them", [this]()
		{
			if (!SailOn()) { return; }
			const int32 Node = Run->GetState().CurrentNode;
			Run->ApplyCombatResult(Won(Run->GetState(), 70.f));
			const FDwRunState& S = Run->GetState();
			TestEqual(TEXT("Rewards"), S.Phase, EDwRunPhase::Rewards);
			TestEqual(TEXT("Combat -> Rewards"), Listener->Phases.Last().Key, EDwRunPhase::Rewards);
			TestEqual(TEXT("225 gold pending"), S.PendingRewards.Gold, 225);
			TestEqual(TEXT("One plunder item"), S.PendingRewards.Items.Num(), 1);
			TestEqual(TEXT("Three moves"), S.PendingRewards.AbilityChoices.Num(), 3);
			TestEqual(TEXT("Gold not banked yet"), S.Gold, 100);
			TestEqual(TEXT("Still at the node"), S.CurrentNode, Node);
			TestTrue(TEXT("No routes before claiming"), Run->GetChoices().IsEmpty());

			TestTrue(TEXT("Claimed"), Run->ClaimRewards(INDEX_NONE, FGuid()));
			TestEqual(TEXT("325 gold"), S.Gold, 325);
			TestEqual(TEXT("Earned stat"), S.Stats.GoldEarned, 225);
			TestEqual(TEXT("Item in the hold"), S.Inventory.Num(), 1);
			TestEqual(TEXT("No move learned when skipped"), S.Crew[0].Abilities.Num() + S.Crew[1].Abilities.Num() + S.Crew[2].Abilities.Num(), 0);
			TestEqual(TEXT("Map"), S.Phase, EDwRunPhase::Map);
			TestEqual(TEXT("Cleared"), S.Stats.NodesCleared, 1);
			TestEqual(TEXT("Bundle cleared"), S.PendingRewards.Gold, 0);
		});

		It("pays a sink 1x with no item, and stacks plunder of the same item", [this]()
		{
			if (!SailOn()) { return; }
			Run->ApplyCombatResult(Won(Run->GetState(), 70.f, EDwCombatOutcome::VictorySunk));
			TestEqual(TEXT("150"), Run->GetState().PendingRewards.Gold, 150);
			TestTrue(TEXT("No item"), Run->GetState().PendingRewards.Items.IsEmpty());
			if (!FinishNode()) { return; }

			C.GameData->DefaultShop->Items = { C.Grog };
			for (int32 Fight = 0; Fight < 2; ++Fight)
			{
				if (!SailOn()) { return; }
				Run->ApplyCombatResult(Won(Run->GetState(), 70.f, EDwCombatOutcome::VictorySurrender));
				if (!FinishNode()) { return; }
			}
			TestEqual(TEXT("One stack"), Run->GetState().Inventory.Num(), 1);
			TestEqual(TEXT("Two grog"), Run->GetState().CountItem(C.Grog->GetFName()), 2);
			TestEqual(TEXT("150 + 225 + 225 earned"), Run->GetState().Stats.GoldEarned, 600);
		});

		It("teaches the chosen move to someone who can learn it, and refuses anyone else", [this]()
		{
			ExpectRefusals(3);
			if (!SailOn()) { return; }
			FDwCombatResult Result = Won(Run->GetState(), 70.f);
			Result.Crew[2].bAlive = false;
			Run->ApplyCombatResult(Result);
			const FDwRunState& S = Run->GetState();
			const TArray<FDwAbilityOffer>& Offers = S.PendingRewards.AbilityChoices;
			// Rigging is the deckhands' only, Lantern the captain's only: three of four always include one of them.
			int32 Choice = INDEX_NONE;
			FGuid Outsider;
			for (int32 Index = 0; Index < Offers.Num() && Choice == INDEX_NONE; ++Index)
			{
				for (const FDwCrewMemberState& Member : S.Crew)
				{
					if (Member.bAlive && !Offers[Index].EligibleCrew.Contains(Member.Id))
					{
						Choice = Index;
						Outsider = Member.Id;
						break;
					}
				}
			}
			if (!TestTrue(TEXT("An offer someone can't take"), Choice != INDEX_NONE)) { return; }
			for (const FDwAbilityOffer& Offer : Offers)
			{
				TestFalse(TEXT("The dead are never eligible"), Offer.EligibleCrew.Contains(S.Crew[2].Id));
			}
			TestFalse(TEXT("Not eligible"), Run->ClaimRewards(Choice, Outsider));
			TestFalse(TEXT("No such choice"), Run->ClaimRewards(7, S.Crew[0].Id));
			TestFalse(TEXT("The dead can't learn"), Run->ClaimRewards(Choice, S.Crew[2].Id));
			TestEqual(TEXT("Nothing banked by a refusal"), S.Gold, 100);

			const FGuid Learner = Offers[Choice].EligibleCrew[0];
			const UDwAbilityDefinition* Ability = Offers[Choice].Ability;
			TestTrue(TEXT("Learned"), Run->ClaimRewards(Choice, Learner));
			const FDwCrewMemberState* Member = S.FindCrew(Learner);
			TestTrue(TEXT("Knows it"), Member && Member->Abilities.Num() == 1 && Member->Abilities[0] == Ability);
		});

		It("with every ability slot full, a new move needs a slot to replace (P4)", [this]()
		{
			ExpectRefusals(2);
			UDwAbilityDefinition* Old1 = C.Ability(TEXT("Old1"));
			UDwAbilityDefinition* Old2 = C.Ability(TEXT("Old2"));
			CrewAt(1).Abilities = { Old1, Old2 }; // MaxAbilitySlots 2
			if (!SailOn()) { return; }
			Run->ApplyCombatResult(Won(Run->GetState(), 70.f));
			const FGuid Hand = Run->GetState().Crew[1].Id;
			const int32 Choice = Run->GetState().PendingRewards.AbilityChoices.IndexOfByPredicate(
				[&Hand](const FDwAbilityOffer& Offer) { return Offer.EligibleCrew.Contains(Hand); });
			if (!TestTrue(TEXT("Something for the deckhand"), Choice != INDEX_NONE)) { return; }
			const UDwAbilityDefinition* New = Run->GetState().PendingRewards.AbilityChoices[Choice].Ability;
			TestFalse(TEXT("Full without a slot"), Run->ClaimRewards(Choice, Hand));
			TestFalse(TEXT("No slot 5"), Run->ClaimRewards(Choice, Hand, 5));
			TestTrue(TEXT("Replace slot 1"), Run->ClaimRewards(Choice, Hand, 0));
			const TArray<TObjectPtr<const UDwAbilityDefinition>>& Abilities = Run->GetState().Crew[1].Abilities;
			TestTrue(TEXT("Old1 replaced, Old2 kept"), Abilities.Num() == 2 && Abilities[0] == New && Abilities[1] == Old2);
		});

		It("refuses rewards and perks outside their phases, and can be abandoned mid-rewards", [this]()
		{
			ExpectRefusals(4);
			TestFalse(TEXT("Claim on the map"), Run->ClaimRewards(INDEX_NONE, FGuid()));
			TestFalse(TEXT("Perk on the map"), Run->ChoosePerk(0));
			if (!SailOn()) { return; }
			Run->ApplyCombatResult(Won(Run->GetState(), 70.f));
			TestFalse(TEXT("No sailing from the rewards"), Run->ChooseNode(Run->GetState().Map.Nodes[Run->GetState().CurrentNode].Next[0]));
			TestFalse(TEXT("No perk before the claim"), Run->ChoosePerk(0));
			TestTrue(TEXT("Abandon"), Run->Abandon());
			TestEqual(TEXT("Defeat"), Run->GetPhase(), EDwRunPhase::Defeat);
		});

		It("gives the same rewards for the same seed and node", [this]()
		{
			UDwRun* A = NewRun(42);
			UDwRun* B = NewRun(42);
			for (UDwRun* Each : { A, B })
			{
				Each->ChooseNode(Each->GetChoices()[1]);
				Each->ApplyCombatResult(Won(Each->GetState(), 70.f));
			}
			const FDwRewardBundle& RA = A->GetState().PendingRewards;
			const FDwRewardBundle& RB = B->GetState().PendingRewards;
			TestTrue(TEXT("Same item"), RA.Items.Num() == 1 && RB.Items.Num() == 1 && RA.Items[0].Item == RB.Items[0].Item);
			TestEqual(TEXT("Same count"), RA.AbilityChoices.Num(), RB.AbilityChoices.Num());
			for (int32 Index = 0; Index < RA.AbilityChoices.Num(); ++Index)
			{
				TestTrue(TEXT("Same moves"), RA.AbilityChoices[Index].Ability == RB.AbilityChoices[Index].Ability);
			}
		});
	});

	Describe("XP and level-ups (DW-203)", [this]()
	{
		It("gives kill XP to the killer and clear XP to every living crew member; sinking and friendly kills give none", [this]()
		{
			if (!SailOn()) { return; }
			const FDwRunState& S = Run->GetState();
			const FGuid Captain = S.Crew[0].Id, Hand = S.Crew[1].Id, Fallen = S.Crew[2].Id;
			FDwCombatResult Result = Won(S, 40.f);
			Result.Crew[2].bAlive = false;
			Result.Kills = { KillBy(Hand, 25), KillBy(Hand, 25), KillBy(FGuid(), 25), KillBy(Fallen, 25), KillBy(Hand, 25, EDwSide::Player) };
			Run->ApplyCombatResult(Result);

			TestEqual(TEXT("Captain: clear 50"), S.FindCrew(Captain)->Xp, 50);
			TestEqual(TEXT("Deckhand: 50 + 2 x 25 = level 2"), S.FindCrew(Hand)->Level, 2);
			TestEqual(TEXT("Deckhand: 0 XP into level 2"), S.FindCrew(Hand)->Xp, 0);
			TestTrue(TEXT("The fallen gain nothing"), S.FindCrew(Fallen)->Level == 1 && S.FindCrew(Fallen)->Xp == 0);
			TestEqual(TEXT("XP gains listed for the living"), S.PendingRewards.Xp.Num(), 2);
			const FDwXpGain* Gain = S.PendingRewards.Xp.FindByPredicate([&Hand](const FDwXpGain& G) { return G.CrewId == Hand; });
			TestTrue(TEXT("Deckhand gain"), Gain && Gain->Xp == 100 && Gain->LevelsGained == 1);

			if (!TestEqual(TEXT("One level-up"), S.PendingLevelUps.Num(), 1)) { return; }
			const FDwPendingLevelUp& LevelUp = S.PendingLevelUps[0];
			TestTrue(TEXT("Deckhand to level 2"), LevelUp.CrewId == Hand && LevelUp.NewLevel == 2);
			TestTrue(TEXT("+10 HP, +2 MP"), LevelUp.StatGain.MaxHealth == 10 && LevelUp.StatGain.MaxMana == 2);
			TestEqual(TEXT("Health grows with MaxHealth"), S.FindCrew(Hand)->Health, 50.f);
			TestTrue(TEXT("Perks rolled for the rewards screen"), LevelUp.bPerksRolled && LevelUp.PerkChoices.Num() == 3);
		});

		It("multiplies XP for a Veteran Crew captain (K7: +50% XP)", [this]()
		{
			C.Captain->RuleTags.AddTag(DwTags::Rule_Captain_VeteranCrew);
			if (!SailOn()) { return; }
			Run->ApplyCombatResult(Won(Run->GetState(), 70.f));
			TestEqual(TEXT("50 x 1.5"), Run->GetState().Crew[1].Xp, 75);
			TestEqual(TEXT("Shown as 75"), Run->GetState().PendingRewards.Xp[1].Xp, 75);
		});

		It("queues a level-up per level gained, in roster order, then back to the map", [this]()
		{
			ExpectRefusals(1);
			C.EncounterRow().ClearXp = 250; // 100 + 100 + 50: two levels each
			if (!SailOn()) { return; }
			Run->ApplyCombatResult(Won(Run->GetState(), 70.f));
			const FDwRunState& S = Run->GetState();
			if (!TestEqual(TEXT("Six level-ups"), S.PendingLevelUps.Num(), 6)) { return; }
			TestTrue(TEXT("Captain first, level 2 then 3"), S.PendingLevelUps[0].CrewId == S.Crew[0].Id && S.PendingLevelUps[0].NewLevel == 2
				&& S.PendingLevelUps[1].CrewId == S.Crew[0].Id && S.PendingLevelUps[1].NewLevel == 3);
			TestTrue(TEXT("Only the first is rolled yet"), S.PendingLevelUps[0].bPerksRolled && !S.PendingLevelUps[1].bPerksRolled);

			Run->ClaimRewards(INDEX_NONE, FGuid());
			TestEqual(TEXT("LevelUp"), S.Phase, EDwRunPhase::LevelUp);
			const FDwPendingLevelUp* First = Run->GetCurrentLevelUp();
			if (!TestTrue(TEXT("Three perks: Tough, Swift, Lamplighter (no Marksman)"), First && First->PerkChoices.Num() == 3
				&& !First->PerkChoices.Contains(C.Marksman))) { return; }
			TestFalse(TEXT("Out of range"), Run->ChoosePerk(3));
			const int32 Swift = First->PerkChoices.IndexOfByKey(C.Swift);
			TestTrue(TEXT("Swift"), Run->ChoosePerk(Swift));
			TestTrue(TEXT("Captain has Swift"), S.Crew[0].Perks.Num() == 1 && S.Crew[0].Perks[0] == C.Swift);

			const FDwPendingLevelUp* Second = Run->GetCurrentLevelUp();
			if (!TestTrue(TEXT("Second level-up rolled now, without Swift (1 stack)"), Second && Second->bPerksRolled && Second->NewLevel == 3
				&& Second->PerkChoices.Num() == 2 && !Second->PerkChoices.Contains(C.Swift))) { return; }
			TestTrue(TEXT("Lamplighter"), Run->ChoosePerk(Second->PerkChoices.IndexOfByKey(C.Lamplighter)));
			TestTrue(TEXT("Lantern learned through the perk"), S.Crew[0].Abilities.Contains(C.Lantern));
			for (int32 Left = 4; Left > 0; --Left)
			{
				TestEqual(TEXT("Still levelling"), S.Phase, EDwRunPhase::LevelUp);
				TestTrue(TEXT("Perk"), Run->ChoosePerk(0));
			}
			TestEqual(TEXT("Map"), S.Phase, EDwRunPhase::Map);
			TestEqual(TEXT("Cleared once"), S.Stats.NodesCleared, 1);
			TestTrue(TEXT("Queue empty"), S.PendingLevelUps.IsEmpty());
			TestEqual(TEXT("Everyone level 3"), S.Crew[0].Level + S.Crew[1].Level + S.Crew[2].Level, 9);
		});

		It("confirms a level-up with nothing left to offer with INDEX_NONE", [this]()
		{
			ExpectRefusals(1);
			C.Perks->Perks.Reset();
			C.EncounterRow().ClearXp = 100;
			if (!SailOn()) { return; }
			Run->ApplyCombatResult(Won(Run->GetState(), 70.f));
			Run->ClaimRewards(INDEX_NONE, FGuid());
			TestTrue(TEXT("No choices"), Run->GetCurrentLevelUp() && Run->GetCurrentLevelUp()->PerkChoices.IsEmpty());
			TestFalse(TEXT("Index 0 refused"), Run->ChoosePerk(0));
			for (int32 Each = 0; Each < 3; ++Each)
			{
				TestTrue(TEXT("Confirmed"), Run->ChoosePerk(INDEX_NONE));
			}
			TestEqual(TEXT("Map"), Run->GetPhase(), EDwRunPhase::Map);
			TestEqual(TEXT("Level 2 anyway"), Run->GetState().Crew[0].Level, 2);
		});

		It("stops at MaxLevel", [this]()
		{
			C.RunRow().MaxLevel = 2;
			C.EncounterRow().ClearXp = 1000;
			if (!SailOn()) { return; }
			Run->ApplyCombatResult(Won(Run->GetState(), 70.f));
			TestEqual(TEXT("One level-up each"), Run->GetState().PendingLevelUps.Num(), 3);
			TestTrue(TEXT("Level 2, no XP"), Run->GetState().Crew[0].Level == 2 && Run->GetState().Crew[0].Xp == 0);
		});

		It("rolls the same perk choices for the same seed", [this]()
		{
			C.EncounterRow().ClearXp = 250;
			UDwRun* A = NewRun(42);
			UDwRun* B = NewRun(42);
			for (UDwRun* Each : { A, B })
			{
				Each->ChooseNode(Each->GetChoices()[0]);
				Each->ApplyCombatResult(Won(Each->GetState(), 70.f));
				Each->ClaimRewards(INDEX_NONE, FGuid());
			}
			for (int32 Step = 0; Step < 6; ++Step)
			{
				const FDwPendingLevelUp* LA = A->GetCurrentLevelUp();
				const FDwPendingLevelUp* LB = B->GetCurrentLevelUp();
				if (!TestTrue(TEXT("Both levelling"), LA && LB)) { return; }
				TestTrue(TEXT("Same choices"), LA->PerkChoices == LB->PerkChoices);
				A->ChoosePerk(0);
				B->ChoosePerk(0);
			}
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
