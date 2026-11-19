// Copyright Brandon Hillig 2026

// DW-211 + shop screen: run screen view models without widgets (Docs/Architecture/14-UI.md "How it's tested"): the map exposes only
// legal routes, every enabled button is one UDwRun accepts and every disabled one says why. An unexpected UDwRun
// "refused" warning fails these tests, so a view model can never offer something the run would refuse.
// Also: DA_UIScreenSet maps every run phase to its built screen (needs Tools/CreateUIContent.ps1).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/DwAssetManager.h"
#include "Core/DwGameplayTags.h"
#include "Run/DwRun.h"
#include "Run/Economy/DwShopRules.h"
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

	/** Claims the rewards and takes the first perk of every level-up: back on the map. */
	bool FinishFight()
	{
		if (!TestTrue(TEXT("Claimed"), Run->ClaimRewards(INDEX_NONE, FGuid())))
		{
			return false;
		}
		while (const FDwPendingLevelUp* LevelUp = Run->GetPhase() == EDwRunPhase::LevelUp ? Run->GetCurrentLevelUp() : nullptr)
		{
			Run->ChoosePerk(LevelUp->PerkChoices.IsEmpty() ? INDEX_NONE : 0);
		}
		return TestEqual(TEXT("Back on the map"), Run->GetPhase(), EDwRunPhase::Map);
	}

	/** Wins fights (hull back at ShipHealth) and passes events until a shop is offered, then enters it. */
	bool SailToShop(float ShipHealth = 120.f)
	{
		for (int32 Step = 0; Step < 30 && !Run->IsOver(); ++Step)
		{
			const TArray<int32> Choices = Run->GetChoices();
			const FDwRunState& S = Run->GetState();
			const int32* Shop = Choices.FindByPredicate([&S](int32 Index) { return S.Map.Nodes[Index].Type == EDwNodeType::Shop; });
			if (Shop)
			{
				return TestTrue(TEXT("Entered the shop"), Run->ChooseNode(*Shop)) && TestEqual(TEXT("Shop phase"), Run->GetPhase(), EDwRunPhase::Shop);
			}
			const int32* Fight = Choices.FindByPredicate([&S](int32 Index) { return S.Map.Nodes[Index].Type == EDwNodeType::Fight; });
			if (!TestTrue(TEXT("A route"), !Choices.IsEmpty()) || !Run->ChooseNode(Fight ? *Fight : Choices[0]))
			{
				return false;
			}
			if (Run->GetPhase() == EDwRunPhase::Event)
			{
				Run->LeaveEvent();
				continue;
			}
			FDwCombatResult Result = DwRunTest::Won(Run->GetState(), 70.f);
			Result.PlayerShipHealth = ShipHealth;
			if (!Run->ApplyCombatResult(Result) || !FinishFight())
			{
				return false;
			}
		}
		return TestTrue(TEXT("Found a shop"), false);
	}

	/** A run with plenty of gold, in its first shop, with a shop view model bound to it. */
	UDwShopViewModel* OpenShop(int32 StartingGold = 1000)
	{
		C.EconomyRow().StartingGold = StartingGold;
		Run = NewRun();
		if (!SailToShop())
		{
			return nullptr;
		}
		UDwShopViewModel* Shop = Make<UDwShopViewModel>();
		Shop->Bind(Run);
		return Shop;
	}

	const FDwShopOffer* Offer(int32 OfferId) const
	{
		return Run->GetShop().Offers.FindByPredicate([OfferId](const FDwShopOffer& O) { return O.OfferId == OfferId; });
	}

	/** Offer ids of Type in shop order (the order the view model makes its cards in). */
	TArray<int32> OffersOf(EDwShopOfferType Type, bool bSkipSoldOut = false) const
	{
		TArray<int32> Ids;
		for (const FDwShopOffer& O : Run->GetShop().Offers)
		{
			if (O.Type == Type && !(bSkipSoldOut && O.bSoldOut))
			{
				Ids.Add(O.OfferId);
			}
		}
		return Ids;
	}

	/** Every card in Cards matches UDwRun::CanPurchase for the offer in Ids at the same index; disabled ones say why. */
	void ExpectCardsMatchRun(const TCHAR* What, const TArray<TObjectPtr<UDwChoiceViewModel>>& Cards, const TArray<int32>& Ids)
	{
		if (!TestEqual(*FString::Printf(TEXT("%s: one card per offer"), What), Cards.Num(), Ids.Num()))
		{
			return;
		}
		for (int32 Index = 0; Index < Cards.Num(); ++Index)
		{
			const FDwChoiceView& View = Cards[Index]->GetView();
			const bool bSells = Run->CanPurchase(Ids[Index], FGuid()) == EDwPurchaseResult::Ok;
			TestEqual(*FString::Printf(TEXT("%s %d (%s): enabled = the run sells it"), What, Index, *View.Title.ToString()), View.bEnabled, bSells);
			TestEqual(*FString::Printf(TEXT("%s %d: a reason exactly when disabled"), What, Index), View.DisabledReason.IsEmpty(), View.bEnabled);
		}
	}

	int32 CrewIndex(const FGuid& Id) const
	{
		return Run->GetState().Crew.IndexOfByPredicate([&Id](const FDwCrewMemberState& M) { return M.Id == Id; });
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

	Describe("Shop", [this]()
	{
		It("shows gold, hull, hold and crew, and one card per offer that is enabled exactly when the run sells it", [this]()
		{
			UDwShopViewModel* Shop = OpenShop();
			if (!Shop) { return; }
			const_cast<FDwRunState&>(Run->GetState()).Gold = 130; // enough for some offers, not all
			Shop->Bind(Run);
			const FDwRunState& S = Run->GetState();
			TestEqual(TEXT("Gold"), Shop->GetGold(), S.Gold);
			TestEqual(TEXT("Hull"), Shop->GetShipText().ToString(), FString::Printf(TEXT("Hull %d / %d"), FMath::RoundToInt(S.Ship.Health), FMath::RoundToInt(S.Ship.MaxHealth)));
			TestEqual(TEXT("Hold"), Shop->GetHoldText().ToString(), FString::Printf(TEXT("Hold %d / 6"), S.CountItems()));
			TestEqual(TEXT("Crew lines"), Shop->GetCrew().Num(), S.Crew.Num());

			TArray<int32> Supplies = OffersOf(EDwShopOfferType::Repair);
			Supplies.Append(OffersOf(EDwShopOfferType::Item));
			ExpectCardsMatchRun(TEXT("Supply"), Shop->GetSupplies(), Supplies);
			ExpectCardsMatchRun(TEXT("Recruit"), Shop->GetRecruits(), OffersOf(EDwShopOfferType::Recruit));
			ExpectCardsMatchRun(TEXT("Upgrade"), Shop->GetUpgrades(), OffersOf(EDwShopOfferType::ShipUpgrade));
			ExpectCardsMatchRun(TEXT("Disband"), Shop->GetDisbands(), OffersOf(EDwShopOfferType::Disband, /*bSkipSoldOut*/ true));
			TestEqual(TEXT("A card per move"), Shop->GetMoves().Num(), OffersOf(EDwShopOfferType::Ability).Num());
			TestTrue(TEXT("Some cards are out of reach at this gold"), Shop->GetUpgrades().ContainsByPredicate([](const TObjectPtr<UDwChoiceViewModel>& Card)
			{
				return !Card->GetView().bEnabled && Card->GetView().DisabledReason.ToString().StartsWith(TEXT("Needs 150 gold"));
			}));
			TestNull(TEXT("No Buy until a move is picked"), Shop->GetBuyMove());
			TestTrue(TEXT("Leave enabled"), Shop->GetLeave() && Shop->GetLeave()->GetView().bEnabled);
		});

		It("buys repair, an item, a recruit and an upgrade on click; gold and the cards follow", [this]()
		{
			UDwShopViewModel* Shop = OpenShop();
			if (!Shop) { return; }
			const FDwRunState& S = Run->GetState();
			const int32 Gold = S.Gold;
			const int32 Crew = S.Crew.Num();
			const int32 Items = S.CountItems();

			TestTrue(TEXT("Repair"), Shop->GetSupplies()[0]->Select());
			TestEqual(TEXT("Hull 120 + 100"), S.Ship.Health, 220.f);
			TestEqual(TEXT("A purchase counted"), Shop->GetPurchaseCount(), 1);
			TestEqual(TEXT("Ok"), Shop->GetLastResult(), EDwPurchaseResult::Ok);
			TestTrue(TEXT("Item"), Shop->GetSupplies()[1]->Select());
			TestEqual(TEXT("In the hold"), S.CountItems(), Items + 1);
			TestTrue(TEXT("Recruit"), Shop->GetRecruits()[0]->Select());
			TestEqual(TEXT("A new crew member"), S.Crew.Num(), Crew + 1);
			TestFalse(TEXT("The recruit is sold"), Shop->GetRecruits()[0]->GetView().bEnabled);
			TestEqual(TEXT("Sold out"), Shop->GetRecruits()[0]->GetView().DisabledReason.ToString(), FString(TEXT("Sold out.")));
			TestTrue(TEXT("Upgrade"), Shop->GetUpgrades()[0]->Select());
			TestEqual(TEXT("Four purchases"), Shop->GetPurchaseCount(), 4);
			TestEqual(TEXT("Gold paid"), S.Gold, Gold - 100 - Offer(OffersOf(EDwShopOfferType::Item)[0])->Price - 80 - 150);
			TestEqual(TEXT("Header follows"), Shop->GetGold(), S.Gold);
			TestTrue(TEXT("Purchases sound like purchases"), Shop->GetSupplies()[0]->GetView().SelectSound == FGameplayTag(DwTags::UI_Sound_Purchase));
			ExpectCardsMatchRun(TEXT("Upgrade after"), Shop->GetUpgrades(), OffersOf(EDwShopOfferType::ShipUpgrade));
		});

		It("refuses with the reason and without asking the run when it can't sell", [this]()
		{
			UDwShopViewModel* Shop = OpenShop();
			if (!Shop) { return; }
			const_cast<FDwRunState&>(Run->GetState()).Gold = 10;
			Shop->Bind(Run);
			const FDwChoiceView& Repair = Shop->GetSupplies()[0]->GetView();
			TestFalse(TEXT("Repair disabled"), Repair.bEnabled);
			TestEqual(TEXT("Why"), Repair.DisabledReason.ToString(), FString(TEXT("Needs 100 gold - you have 10.")));
			TestFalse(TEXT("A disabled card does nothing"), Shop->GetSupplies()[0]->Select());
			TestFalse(TEXT("The intent refuses too"), Shop->Buy(OffersOf(EDwShopOfferType::Repair)[0])); // no 'refused' warning: never reached TryPurchase
			TestEqual(TEXT("Counted for OnPurchase"), Shop->GetPurchaseCount(), 1);
			TestEqual(TEXT("Not enough gold"), Shop->GetLastResult(), EDwPurchaseResult::NotEnoughGold);
			TestEqual(TEXT("Nothing paid"), Run->GetState().Gold, 10);
			TestFalse(TEXT("Moves don't buy through Buy"), Shop->Buy(OffersOf(EDwShopOfferType::Ability)[0]));
		});

		It("teaches a move: pick it, who learns it and what a full learner forgets; Buy waits for each step", [this]()
		{
			UDwShopViewModel* Shop = OpenShop();
			if (!Shop) { return; }
			// A move more than one crew member can learn (only the captain's Lantern has a single learner).
			const TArray<int32> Moves = OffersOf(EDwShopOfferType::Ability);
			const int32 MoveIndex = Moves.IndexOfByPredicate([this](int32 Id) { return Offer(Id)->EligibleCrew.Num() > 1; });
			if (!TestTrue(TEXT("A move with two learners"), MoveIndex != INDEX_NONE)) { return; }
			const int32 MoveId = Moves[MoveIndex];
			const UDwAbilityDefinition* Move = Offer(MoveId)->Ability;
			const FGuid LearnerId = Offer(MoveId)->EligibleCrew[1];
			// The learner is full (MaxAbilitySlots 2) with moves that don't touch this offer.
			FDwCrewMemberState& Learner = CrewAt(CrewIndex(LearnerId));
			Learner.Abilities = { C.Lantern, C.Cutlass };
			Shop->Bind(Run);

			TestTrue(TEXT("Picked the move"), Shop->GetMoves()[MoveIndex]->Select());
			TestTrue(TEXT("Shown as picked"), Shop->GetMoves()[MoveIndex]->GetView().bSelected);
			TestEqual(TEXT("A learner card each"), Shop->GetLearners().Num(), Offer(MoveId)->EligibleCrew.Num());
			if (!TestNotNull(TEXT("Buy shown"), Shop->GetBuyMove())) { return; }
			TestFalse(TEXT("Buy waits for a learner"), Shop->GetBuyMove()->GetView().bEnabled);
			TestTrue(TEXT("Says so"), Shop->GetBuyMove()->GetView().DisabledReason.ToString().StartsWith(TEXT("Pick who learns")));

			TestTrue(TEXT("Picked the learner"), Shop->GetLearners()[1]->Select());
			TestEqual(TEXT("Two moves to forget"), Shop->GetForget().Num(), 2);
			TestFalse(TEXT("Buy waits for a move to forget"), Shop->GetBuyMove()->GetView().bEnabled);
			TestTrue(TEXT("Says so"), Shop->GetBuyMove()->GetView().DisabledReason.ToString().Contains(TEXT("pick one to forget")));

			TestTrue(TEXT("Forget Lantern"), Shop->GetForget()[0]->Select());
			TestTrue(TEXT("Buy ready"), Shop->GetBuyMove()->GetView().bEnabled);
			const int32 Gold = Run->GetState().Gold;
			TestTrue(TEXT("Bought"), Shop->GetBuyMove()->Select());
			const FDwCrewMemberState& After = Run->GetState().Crew[CrewIndex(LearnerId)];
			TestTrue(TEXT("Learned it in Lantern's slot"), After.Abilities.Num() == 2 && After.Abilities[0] == Move && After.Abilities[1] == C.Cutlass);
			TestEqual(TEXT("Paid"), Run->GetState().Gold, Gold - Offer(MoveId)->Price);
			TestNull(TEXT("The pick is done"), Shop->GetBuyMove());
			TestEqual(TEXT("No learners left on screen"), Shop->GetLearners().Num(), 0);
			TestFalse(TEXT("The move is sold"), Shop->GetMoves()[MoveIndex]->GetView().bEnabled);
		});

		It("a second click on the picked move drops it; a lone learner is picked for you", [this]()
		{
			UDwShopViewModel* Shop = OpenShop();
			if (!Shop) { return; }
			const TArray<int32> Moves = OffersOf(EDwShopOfferType::Ability);
			TestTrue(TEXT("Picked"), Shop->GetMoves()[0]->Select());
			TestTrue(TEXT("Dropped"), Shop->GetMoves()[0]->Select());
			TestNull(TEXT("No Buy"), Shop->GetBuyMove());
			TestFalse(TEXT("Not picked"), Shop->GetMoves()[0]->GetView().bSelected);

			// Leave one learner for the first move: the others know it already.
			const FDwShopOffer& First = *Offer(Moves[0]);
			for (int32 Index = 1; Index < First.EligibleCrew.Num(); ++Index)
			{
				CrewAt(CrewIndex(First.EligibleCrew[Index])).Abilities = { First.Ability };
			}
			CrewAt(CrewIndex(First.EligibleCrew[0])).Abilities.Reset(); // room to learn: no move to forget
			FDwShopRules::Refresh(const_cast<FDwRunState&>(Run->GetState()), FDwShopContext{ &C.EconomyRow(), C.RunRow().MaxAbilitySlots });
			Shop->Bind(Run);
			TestTrue(TEXT("Picked again"), Shop->GetMoves()[0]->Select());
			if (!TestNotNull(TEXT("Buy"), Shop->GetBuyMove())) { return; }
			TestTrue(TEXT("Ready at once: the only learner is picked"), Shop->GetBuyMove()->GetView().bEnabled);
		});

		It("disband asks first (the card carries the question) and keeps the last crew member", [this]()
		{
			UDwShopViewModel* Shop = OpenShop();
			if (!Shop) { return; }
			const int32 Crew = Run->GetState().Crew.Num();
			if (!TestEqual(TEXT("Two hands to disband"), Shop->GetDisbands().Num(), 2)) { return; }
			const FDwChoiceView& First = Shop->GetDisbands()[0]->GetView();
			TestTrue(TEXT("Asks first"), First.ConfirmTitle.ToString().StartsWith(TEXT("Disband")) && !First.ConfirmYes.IsEmpty());
			TestTrue(TEXT("Disbanded (the button confirmed)"), Shop->GetDisbands()[0]->Select());
			TestEqual(TEXT("One fewer"), Run->GetState().Crew.Num(), Crew - 1);
			if (!TestEqual(TEXT("Their card is gone"), Shop->GetDisbands().Num(), 1)) { return; }
			const FDwChoiceView& Last = Shop->GetDisbands()[0]->GetView();
			TestFalse(TEXT("The last hand stays"), Last.bEnabled);
			TestEqual(TEXT("Why"), Last.DisabledReason.ToString(), FString(TEXT("The captain keeps at least one crew member.")));
		});

		It("Leave goes back to the map; off the shop every list is empty and Leave is disabled", [this]()
		{
			UDwShopViewModel* Shop = OpenShop();
			if (!Shop) { return; }
			TestTrue(TEXT("Picked a move"), Shop->GetMoves()[0]->Select());
			TestTrue(TEXT("Left"), Shop->GetLeave()->Select());
			TestEqual(TEXT("Map"), Run->GetPhase(), EDwRunPhase::Map);
			TestEqual(TEXT("No supplies"), Shop->GetSupplies().Num(), 0);
			TestEqual(TEXT("No moves"), Shop->GetMoves().Num(), 0);
			TestEqual(TEXT("No disbands"), Shop->GetDisbands().Num(), 0);
			TestNull(TEXT("The pick is gone"), Shop->GetBuyMove());
			TestFalse(TEXT("Leave disabled"), Shop->GetLeave()->GetView().bEnabled);
			TestFalse(TEXT("And refuses"), Shop->GetLeave()->Select());
		});

		It("has a reason for every refusal", [this]()
		{
			const UEnum* Results = StaticEnum<EDwPurchaseResult>();
			for (int32 Index = 0; Index < Results->NumEnums() - 1; ++Index) // last entry is _MAX
			{
				const EDwPurchaseResult Result = static_cast<EDwPurchaseResult>(Results->GetValueByIndex(Index));
				const FText Reason = UDwShopViewModel::DescribeResult(Result, *Run, nullptr, FGuid());
				TestEqual(*Results->GetNameStringByIndex(Index), Reason.IsEmpty(), Result == EDwPurchaseResult::Ok);
			}
		});
	});

	Describe("Event shell", [this]()
	{
		It("shows calm water when no event fits and Continue sails on", [this]()
		{
			UDwNodeShellViewModel* Shell = Make<UDwNodeShellViewModel>();
			Shell->Bind(Run);
			TestEqual(TEXT("No buttons on the map"), Shell->GetActions().Num(), 0);
			bool bEvent = false;
			for (int32 Seed = 0; Seed < 20 && !bEvent; ++Seed)
			{
				Run = NewRun(Seed);
				Shell->Bind(Run);
				while (!Run->IsOver() && !bEvent)
				{
					const TArray<int32> Choices = Run->GetChoices();
					if (Choices.Num() == 1 && Choices[0] == Run->GetState().Map.BossNode) { break; }
					Run->ChooseNode(Choices[0]);
					switch (Run->GetPhase())
					{
					case EDwRunPhase::Combat:
						Run->ApplyCombatResult(Won(Run->GetState(), 70.f));
						if (!FinishFight()) { return; }
						break;
					case EDwRunPhase::Shop:
						TestEqual(TEXT("The shell stays empty in a shop"), Shell->GetActions().Num(), 0);
						Run->LeaveShop();
						break;
					case EDwRunPhase::Event:
						// The test content has no events: an event node is calm water with one way on.
						TestEqual(TEXT("Headline"), Shell->GetHeadline().ToString(), FString(TEXT("Calm water")));
						if (!TestEqual(TEXT("One button"), Shell->GetActions().Num(), 1)) { return; }
						TestTrue(TEXT("Continue enabled"), Shell->GetActions()[0]->GetView().bEnabled);
						TestTrue(TEXT("Left"), Shell->GetActions()[0]->Select());
						TestEqual(TEXT("Map"), Run->GetPhase(), EDwRunPhase::Map);
						bEvent = true;
						break;
					default: AddError(TEXT("Unexpected phase")); return;
					}
				}
			}
			TestTrue(TEXT("Saw an event"), bEvent);
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
				{ EDwRunPhase::LevelUp, UDwLevelUpScreen::StaticClass() }, { EDwRunPhase::Shop, UDwShopScreen::StaticClass() },
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
