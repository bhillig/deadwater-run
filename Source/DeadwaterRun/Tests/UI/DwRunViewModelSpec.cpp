// Copyright Brandon Hillig 2026

// DW-211: run screen view models without widgets (Docs/Architecture/14-UI.md "How it's tested"): the map exposes only
// legal routes, every enabled button is one UDwRun accepts and every disabled one says why. An unexpected UDwRun
// "refused" warning fails these tests, so a view model can never offer something the run would refuse.
// Also: DA_UIScreenSet maps every run phase to its built screen (needs Tools/CreateUIContent.ps1).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/DwAssetManager.h"
#include "Run/DwRun.h"
#include "Tests/Run/DwRunTestContent.h"
#include "UI/DwRunScreens.h"
#include "UI/DwRunViewModels.h"
#include "UI/DwUIScreenSet.h"

BEGIN_DEFINE_SPEC(FDwRunViewModelSpec, "DeadwaterRun.UI.Run",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	DwRunTest::FContent C;
	UDwRun* Run = nullptr;

	template <typename T>
	T* Make()
	{
		T* Object = NewObject<T>();
		Object->AddToRoot();
		C.Rooted.Add(Object);
		return Object;
	}

	UDwRun* NewRun(int32 Seed = 7)
	{
		UDwRun* Result = Make<UDwRun>();
		Result->Initialize(*C.GameData, *C.Captain, Seed, C.Pool());
		return Result;
	}

	FDwCrewMemberState& CrewAt(int32 Index) { return const_cast<FDwRunState&>(Run->GetState()).Crew[Index]; }

	/** Sails to the first fight route and wins it as plunder: phase Rewards. */
	bool WinFirstFight()
	{
		const TArray<int32> Choices = Run->GetChoices();
		return TestTrue(TEXT("Sailed"), !Choices.IsEmpty() && Run->ChooseNode(Choices[0]))
			&& TestTrue(TEXT("Won"), Run->ApplyCombatResult(DwRunTest::Won(Run->GetState(), 70.f)))
			&& TestEqual(TEXT("Rewards"), Run->GetPhase(), EDwRunPhase::Rewards);
	}

	bool HasLine(const TArray<FText>& Lines, const TCHAR* Fragment)
	{
		return Lines.ContainsByPredicate([Fragment](const FText& Line) { return Line.ToString().Contains(Fragment); });
	}
END_DEFINE_SPEC(FDwRunViewModelSpec)

void FDwRunViewModelSpec::Define()
{
	using namespace DwRunTest;

	BeforeEach([this]()
	{
		C.Create();
		Run = NewRun();
	});
	AfterEach([this]() { C.Destroy(); Run = nullptr; });

	Describe("Map", [this]()
	{
		It("lists every node with its label, and only the routes UDwRun offers are choosable", [this]()
		{
			UDwMapViewModel* Map = Make<UDwMapViewModel>();
			Map->Bind(Run);
			const FDwRunState& S = Run->GetState();
			TestEqual(TEXT("Every node"), Map->GetNodes().Num(), S.Map.Nodes.Num());
			TestEqual(TEXT("Layers"), Map->GetLayerCount(), S.Map.LayerCount);
			TestEqual(TEXT("Lanes"), Map->GetLaneCount(), 3);
			const TArray<int32> Choices = Run->GetChoices();
			for (const FDwMapNodeView& Node : Map->GetNodes())
			{
				TestEqual(TEXT("Choosable = a UDwRun choice"), Node.State == EDwMapNodeState::Choosable, Choices.Contains(Node.Index));
				TestEqual(TEXT("CanChoose agrees"), Map->CanChoose(Node.Index), Choices.Contains(Node.Index));
				const FString Label = Node.Label.ToString();
				switch (Node.Type)
				{
				case EDwNodeType::Fight: TestEqual(TEXT("Fights say Fight, no crew (M6)"), Label, FString(TEXT("Fight"))); break;
				case EDwNodeType::Shop:  TestEqual(TEXT("Shop"), Label, FString(TEXT("Shop"))); break;
				case EDwNodeType::Event: TestEqual(TEXT("Events are ? (M4)"), Label, FString(TEXT("?"))); break;
				case EDwNodeType::Boss:  TestEqual(TEXT("Boss"), Label, FString(TEXT("Kraken"))); break;
				}
			}
			TestEqual(TEXT("Gold"), Map->GetGold(), 100);
			TestEqual(TEXT("Hull"), Map->GetShipText().ToString(), FString(TEXT("Hull 250 / 250")));
			TestEqual(TEXT("Crew lines"), Map->GetCrew().Num(), 3);
		});

		It("refuses other nodes with a reason, without ever calling the run", [this]()
		{
			UDwMapViewModel* Map = Make<UDwMapViewModel>();
			Map->Bind(Run);
			const FDwMapGraph& Graph = Run->GetState().Map;
			FText Reason;
			TestFalse(TEXT("The boss"), Map->CanChoose(Graph.BossNode, &Reason));
			TestTrue(TEXT("Says why"), Reason.ToString().Contains(TEXT("Too far")));
			TestFalse(TEXT("Not a node"), Map->ChooseNode(999));
			TestFalse(TEXT("Boss intent refused"), Map->ChooseNode(Graph.BossNode));
			TestEqual(TEXT("Still on the map"), Run->GetPhase(), EDwRunPhase::Map);

			const int32 Pick = Run->GetChoices()[1];
			TestTrue(TEXT("A route"), Map->ChooseNode(Pick));
			TestEqual(TEXT("Sailed"), Run->GetState().CurrentNode, Pick);
			TestEqual(TEXT("Refreshed: here"), Map->FindNode(Pick)->State, EDwMapNodeState::Current);
			TestFalse(TEXT("Nothing choosable mid-fight"), Map->CanChoose(Graph.Nodes[Pick].Next[0], &Reason));
			TestTrue(TEXT("Finish first"), Reason.ToString().Contains(TEXT("Finish this stop")));
		});

		It("marks the sailed node, the routes left behind and the next routes", [this]()
		{
			UDwMapViewModel* Map = Make<UDwMapViewModel>();
			Map->Bind(Run);
			if (!WinFirstFight() || !Run->ClaimRewards(INDEX_NONE, FGuid())) { return; }
			const int32 Here = Run->GetState().CurrentNode;
			TestEqual(TEXT("Map"), Run->GetPhase(), EDwRunPhase::Map);
			for (const FDwMapNodeView& Node : Map->GetNodes())
			{
				if (Node.Index == Here)
				{
					TestEqual(TEXT("Current"), Node.State, EDwMapNodeState::Current);
				}
				else if (Node.Layer == 0)
				{
					TestEqual(TEXT("The other first-layer routes are behind"), Node.State, EDwMapNodeState::Missed);
				}
				else if (Run->GetState().Map.Nodes[Here].Next.Contains(Node.Index))
				{
					TestEqual(TEXT("Next routes"), Node.State, EDwMapNodeState::Choosable);
				}
				else
				{
					TestEqual(TEXT("The rest is ahead"), Node.State, EDwMapNodeState::Ahead);
				}
			}
		});
	});

	Describe("Rewards", [this]()
	{
		It("shows the gold, the plunder and the XP; Take with no new move claims them", [this]()
		{
			UDwRewardsViewModel* Rewards = Make<UDwRewardsViewModel>();
			Rewards->Bind(Run);
			TestTrue(TEXT("Nothing to take on the map"), !Rewards->GetClaim()->GetView().bEnabled);
			if (!WinFirstFight()) { return; }
			TestTrue(TEXT("Plunder headline"), Rewards->GetHeadline().ToString().Contains(TEXT("plunder")));
			TestTrue(TEXT("Gold line"), HasLine(Rewards->GetLines(), TEXT("+225 gold")));
			TestTrue(TEXT("Item line"), HasLine(Rewards->GetLines(), TEXT("+ ")));
			TestTrue(TEXT("XP line"), HasLine(Rewards->GetLines(), TEXT("+50 XP")));
			TestEqual(TEXT("Three offers"), Rewards->GetOffers().Num(), 3);
			TestTrue(TEXT("No learner list before a pick"), Rewards->GetLearners().IsEmpty());
			const FDwChoiceView& Claim = Rewards->GetClaim()->GetView();
			TestTrue(TEXT("Take is enabled"), Claim.bEnabled && Claim.DisabledReason.IsEmpty());
			TestTrue(TEXT("Taken"), Rewards->GetClaim()->Select());
			TestEqual(TEXT("325 gold"), Run->GetState().Gold, 325);
			TestEqual(TEXT("Map"), Run->GetPhase(), EDwRunPhase::Map);
		});

		It("needs a learner for a picked move, and a move to forget when the learner is full; then teaches it", [this]()
		{
			UDwAbilityDefinition* Old1 = C.Ability(TEXT("Old1"));
			UDwAbilityDefinition* Old2 = C.Ability(TEXT("Old2"));
			CrewAt(1).Abilities = { Old1, Old2 }; // MaxAbilitySlots 2: full
			UDwRewardsViewModel* Rewards = Make<UDwRewardsViewModel>();
			Rewards->Bind(Run);
			if (!WinFirstFight()) { return; }
			const FGuid Hand = Run->GetState().Crew[1].Id;
			const TArray<FDwAbilityOffer>& Offers = Run->GetState().PendingRewards.AbilityChoices;
			const int32 Offer = Offers.IndexOfByPredicate([&Hand](const FDwAbilityOffer& O) { return O.EligibleCrew.Contains(Hand) && O.EligibleCrew.Num() > 1; });
			if (!TestTrue(TEXT("An offer the full deckhand and someone else can learn"), Offer != INDEX_NONE)) { return; }
			const UDwAbilityDefinition* Ability = Offers[Offer].Ability;

			TestTrue(TEXT("Pick it"), Rewards->GetOffers()[Offer]->Select());
			TestTrue(TEXT("Selected"), Rewards->GetOffers()[Offer]->GetView().bSelected);
			TestFalse(TEXT("Take disabled"), Rewards->GetClaim()->GetView().bEnabled);
			TestTrue(TEXT("Says pick who"), Rewards->GetClaim()->GetView().DisabledReason.ToString().Contains(TEXT("Pick who learns")));
			TestFalse(TEXT("A disabled Take does nothing"), Rewards->GetClaim()->Select());
			TestEqual(TEXT("One learner card per eligible crew member"), Rewards->GetLearners().Num(), Offers[Offer].EligibleCrew.Num());

			const int32 HandCard = Offers[Offer].EligibleCrew.IndexOfByKey(Hand);
			TestTrue(TEXT("The deckhand learns"), Rewards->GetLearners()[HandCard]->Select());
			TestTrue(TEXT("Says forget"), Rewards->GetClaim()->GetView().DisabledReason.ToString().Contains(TEXT("pick one to forget")));
			TestEqual(TEXT("Two moves to forget"), Rewards->GetForget().Num(), 2);
			TestTrue(TEXT("Forget Old1"), Rewards->GetForget()[0]->Select());
			TestTrue(TEXT("Take enabled"), Rewards->GetClaim()->GetView().bEnabled);
			TestTrue(TEXT("Names the move"), Rewards->GetClaim()->GetView().Title.ToString().Contains(TEXT("learn")));

			TestTrue(TEXT("Taken"), Rewards->GetClaim()->Select());
			const TArray<TObjectPtr<const UDwAbilityDefinition>>& Known = Run->GetState().Crew[1].Abilities;
			TestTrue(TEXT("Old1 replaced by the new move"), Known.Num() == 2 && Known[0] == Ability && Known[1] == Old2);
		});

		It("a second click on the picked move skips it again; a lone learner is picked for you", [this]()
		{
			UDwRewardsViewModel* Rewards = Make<UDwRewardsViewModel>();
			Rewards->Bind(Run);
			if (!WinFirstFight()) { return; }
			const TArray<FDwAbilityOffer>& Offers = Run->GetState().PendingRewards.AbilityChoices;
			Rewards->GetOffers()[0]->Select();
			Rewards->GetOffers()[0]->Select();
			TestFalse(TEXT("Deselected"), Rewards->GetOffers()[0]->GetView().bSelected);
			TestTrue(TEXT("Take enabled again"), Rewards->GetClaim()->GetView().bEnabled);
			const int32 Lone = Offers.IndexOfByPredicate([](const FDwAbilityOffer& O) { return O.EligibleCrew.Num() == 1; });
			if (Lone == INDEX_NONE) { return; } // every offer had several learners this seed
			Rewards->GetOffers()[Lone]->Select();
			TestTrue(TEXT("Learner preselected"), Rewards->GetLearners().Num() == 1 && Rewards->GetLearners()[0]->GetView().bSelected);
			TestTrue(TEXT("Take enabled"), Rewards->GetClaim()->GetView().bEnabled);
		});
	});

	Describe("Level-up", [this]()
	{
		It("shows the front level-up, its stat gains and one card per perk; a card takes the perk", [this]()
		{
			C.EncounterRow().ClearXp = 100; // everyone to level 2
			UDwLevelUpViewModel* LevelUp = Make<UDwLevelUpViewModel>();
			LevelUp->Bind(Run);
			if (!WinFirstFight() || !Run->ClaimRewards(INDEX_NONE, FGuid())) { return; }
			TestTrue(TEXT("Headline"), LevelUp->GetHeadline().ToString().Contains(TEXT("Test Captain reaches level 2")));
			TestEqual(TEXT("Stat gains"), LevelUp->GetStatText().ToString(), FString(TEXT("+10 max HP   +2 max MP")));
			TestTrue(TEXT("Queue"), LevelUp->GetQueueText().ToString().Contains(TEXT("2 more")));
			if (!TestEqual(TEXT("Three perk cards"), LevelUp->GetCards().Num(), 3)) { return; }
			const UDwPerkDefinition* First = Run->GetCurrentLevelUp()->PerkChoices[0];
			TestEqual(TEXT("Card title"), LevelUp->GetCards()[0]->GetView().Title.ToString(), First->DisplayName.ToString());
			TestTrue(TEXT("Picked"), LevelUp->GetCards()[0]->Select());
			TestTrue(TEXT("Captain has it"), Run->GetState().Crew[0].Perks.Contains(First));
			TestTrue(TEXT("Next level-up shown"), LevelUp->GetHeadline().ToString().Contains(TEXT("Deckhand")));
		});

		It("offers Continue when the pool has nothing left", [this]()
		{
			C.Perks->Perks.Reset();
			C.EncounterRow().ClearXp = 100;
			UDwLevelUpViewModel* LevelUp = Make<UDwLevelUpViewModel>();
			LevelUp->Bind(Run);
			if (!WinFirstFight() || !Run->ClaimRewards(INDEX_NONE, FGuid())) { return; }
			if (!TestEqual(TEXT("One card"), LevelUp->GetCards().Num(), 1)) { return; }
			TestEqual(TEXT("Continue"), LevelUp->GetCards()[0]->GetView().Title.ToString(), FString(TEXT("Continue")));
			for (int32 Each = 0; Each < 3; ++Each)
			{
				TestTrue(TEXT("Continued"), LevelUp->GetCards()[0]->Select());
			}
			TestEqual(TEXT("Map"), Run->GetPhase(), EDwRunPhase::Map);
		});
	});

	Describe("Shop and event shells", [this]()
	{
		It("show where you are and leave back to the map", [this]()
		{
			UDwNodeShellViewModel* Shell = Make<UDwNodeShellViewModel>();
			Shell->Bind(Run);
			TestFalse(TEXT("Nothing to leave on the map"), Shell->GetLeave()->GetView().bEnabled);
			bool bShop = false, bEvent = false;
			for (int32 Seed = 0; Seed < 20 && !(bShop && bEvent); ++Seed)
			{
				Run = NewRun(Seed);
				Shell->Bind(Run);
				while (!Run->IsOver() && !(bShop && bEvent))
				{
					const TArray<int32> Choices = Run->GetChoices();
					if (Choices.Num() == 1 && Choices[0] == Run->GetState().Map.BossNode) { break; }
					const int32 Next = Choices[0];
					Run->ChooseNode(Next);
					switch (Run->GetPhase())
					{
					case EDwRunPhase::Combat:
						Run->ApplyCombatResult(Won(Run->GetState(), 70.f));
						Run->ClaimRewards(INDEX_NONE, FGuid());
						while (const FDwPendingLevelUp* L = Run->GetPhase() == EDwRunPhase::LevelUp ? Run->GetCurrentLevelUp() : nullptr)
						{
							Run->ChoosePerk(L->PerkChoices.IsEmpty() ? INDEX_NONE : 0);
						}
						break;
					case EDwRunPhase::Shop:
					case EDwRunPhase::Event:
					{
						const bool bIsShop = Run->GetPhase() == EDwRunPhase::Shop;
						TestEqual(TEXT("Headline"), Shell->GetHeadline().ToString(), FString(bIsShop ? TEXT("Shop") : TEXT("Something on the horizon")));
						TestTrue(TEXT("Leave enabled"), Shell->GetLeave()->GetView().bEnabled);
						TestTrue(TEXT("Left"), Shell->GetLeave()->Select());
						TestEqual(TEXT("Map"), Run->GetPhase(), EDwRunPhase::Map);
						(bIsShop ? bShop : bEvent) = true;
						break;
					}
					default: AddError(TEXT("Unexpected phase")); return;
					}
				}
			}
			TestTrue(TEXT("Saw a shop and an event"), bShop && bEvent);
		});
	});

#if WITH_EDITOR
	Describe("Content", [this]()
	{
		It("maps every run phase to its built screen in DA_UIScreenSet", [this]()
		{
			const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
			const UDwUIScreenSet* Screens = GameData ? GameData->UIScreenSet.Get() : nullptr;
			if (!TestNotNull(TEXT("DA_UIScreenSet"), Screens)) { return; }
			const TPair<EDwRunPhase, UClass*> Expected[] = {
				{ EDwRunPhase::Map, UDwMapScreen::StaticClass() }, { EDwRunPhase::Rewards, UDwRewardsScreen::StaticClass() },
				{ EDwRunPhase::LevelUp, UDwLevelUpScreen::StaticClass() }, { EDwRunPhase::Shop, UDwNodeShellScreen::StaticClass() },
				{ EDwRunPhase::Event, UDwNodeShellScreen::StaticClass() } };
			for (const TPair<EDwRunPhase, UClass*>& Entry : Expected)
			{
				const UClass* Class = Screens->GetPhaseScreen(Entry.Key).LoadSynchronous();
				TestTrue(*FString::Printf(TEXT("%s -> %s"), *DwRun::PhaseName(Entry.Key), *GetNameSafe(Class)), Class && Class->IsChildOf(Entry.Value));
				const UDwRunScreen* Defaults = Class ? Class->GetDefaultObject<UDwRunScreen>() : nullptr;
				TestTrue(TEXT("Has its choice button class"), Defaults && Defaults->ChoiceButtonClass);
			}
			const UDwMapScreen* Map = Cast<UDwMapScreen>(Screens->Map.LoadSynchronous() ? Screens->Map.LoadSynchronous()->GetDefaultObject() : nullptr);
			TestTrue(TEXT("Map has its node class"), Map && Map->NodeWidgetClass);
			TestTrue(TEXT("No screen for Victory (summary: DW-210)"), Screens->GetPhaseScreen(EDwRunPhase::Victory).IsNull());
		});
	});
#endif
}

#endif // WITH_DEV_AUTOMATION_TESTS
