// Copyright Brandon Hillig 2026

// DW-205: events (Docs/Architecture/11-Events.md "How it's tested"). Pure: FDwEventResolver and every outcome and
// requirement over a hand-built FDwRunState; then the run flow (Event -> choose -> leave -> Rewards / LevelUp / Map)
// through UDwRun with transient content. The real event pool is checked by DeadwaterRun.Run.Content.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/DwGameData.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwEventDefinition.h"
#include "Data/DwPools.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Engine/DataTable.h"
#include "Misc/DataValidation.h"
#include "Run/DwCaptainRules.h"
#include "Run/DwRun.h"
#include "Run/Events/DwEventOutcome.h"
#include "Run/Events/DwEventResolver.h"
#include "Tests/Run/DwRunTestContent.h"

BEGIN_DEFINE_SPEC(FDwEventSpec, "DeadwaterRun.Rules.Events",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	DwRunTest::FContent C;
	FDwRunState State;
	UDwRun* Run = nullptr;

	// --- Content ------------------------------------------------------------------------------------------------------

	template <typename T>
	T* Outcome(UObject* Outer = nullptr)
	{
		return NewObject<T>(Outer ? Outer : GetTransientPackage());
	}

	/** An event with one choice per outcome list (labels "A", "B", ...), valid on every layer. */
	UDwEventDefinition* MakeEvent(const TCHAR* Name, EDwEventValence Valence, TArray<TArray<UDwEventOutcome*>> Choices = {}, float Weight = 1.f)
	{
		UDwEventDefinition* Out = C.Make<UDwEventDefinition>(Name);
		Out->DisplayName = Out->Title = FText::FromString(Name);
		Out->Body = FText::FromString(TEXT("Something on the water."));
		Out->Valence = Valence;
		Out->Weight = Weight;
		if (Choices.IsEmpty())
		{
			Choices.Add({ Outcome<UDwEventOutcome_Nothing>(Out) });
		}
		for (int32 Index = 0; Index < Choices.Num(); ++Index)
		{
			FDwEventChoice& Choice = Out->Choices.AddDefaulted_GetRef();
			Choice.Label = FText::FromString(FString::Chr(TEXT('A') + Index));
			Choice.ResultText = FText::FromString(TEXT("So it goes."));
			for (UDwEventOutcome* Each : Choices[Index])
			{
				Choice.Outcomes.Add(Each);
			}
		}
		return Out;
	}

	/** The 1 good + 1 bad placeholder events of DW-205's acceptance (the game's pool is Tuning/Events.json). */
	UDwEventDefinition* GoodPlaceholder()
	{
		UDwEventOutcome_Gold* Gold = Outcome<UDwEventOutcome_Gold>();
		Gold->Delta = 50;
		return MakeEvent(TEXT("DA_Event_TestFloatingCrate"), EDwEventValence::Good, { { Gold }, { Outcome<UDwEventOutcome_Nothing>() } });
	}

	UDwEventDefinition* BadPlaceholder()
	{
		UDwEventOutcome_ShipHealth* Hull = Outcome<UDwEventOutcome_ShipHealth>();
		Hull->Delta = -40;
		UDwEventOutcome_Gold* Toll = Outcome<UDwEventOutcome_Gold>();
		Toll->Delta = -30;
		return MakeEvent(TEXT("DA_Event_TestReef"), EDwEventValence::Bad, { { Hull }, { Toll } });
	}

	/** A run-like state: the test captain, the deckhand, the ship, 100 gold. */
	void Reset()
	{
		State = FDwRunState();
		State.Captain = C.Captain;
		State.Crew.Add(DwRunTest::Member(C.CaptainUnit, 1, /*bIsCaptain*/ true));
		State.Crew.Add(DwRunTest::Member(C.Deckhand));
		State.Ship.MaxHealth = 200.f;
		State.Ship.Health = 150.f;
		State.Gold = 100;
		State.CurrentNode = 2;
	}

	FDwEventContext Context(FRandomStream& Random)
	{
		FDwEventContext Out(State, Random);
		Out.Economy = &C.EconomyRow();
		Out.Progression = C.Progression;
		Out.MaxLevel = C.RunRow().MaxLevel;
		return Out;
	}

	/** Applies one outcome to State with stream Seed; returns the change lines. */
	TArray<FText> Apply(const UDwEventOutcome& What, int32 Seed = 1, const FGuid& Chosen = FGuid())
	{
		FRandomStream Random(Seed);
		FDwEventContext Ctx = Context(Random);
		Ctx.ChosenCrew = Chosen;
		What.Apply(Ctx);
		LastLevelUps = Ctx.LevelUps;
		LastOffers = Ctx.AbilityChoices;
		return Ctx.Changes;
	}
	TArray<FDwPendingLevelUp> LastLevelUps;
	TArray<FDwAbilityOffer> LastOffers;

	FDwCrewMemberState& Captain() { return State.Crew[0]; }
	FDwCrewMemberState& Hand() { return State.Crew[1]; }

	// --- Run flow -----------------------------------------------------------------------------------------------------

	UDwRun* NewRun(int32 Seed = 7)
	{
		UDwRun* Result = NewObject<UDwRun>();
		Result->AddToRoot();
		C.Rooted.Add(Result);
		Result->Initialize(*C.GameData, *C.Captain, Seed, C.Pool());
		return Result;
	}

	/** Sails (winning fights, leaving shops) until an event node is entered. False if the chart has none on the way. */
	bool SailToEvent()
	{
		for (int32 Step = 0; Step < 30 && !Run->IsOver(); ++Step)
		{
			switch (Run->GetPhase())
			{
			case EDwRunPhase::Map:
			{
				const TArray<int32> Choices = Run->GetChoices();
				const int32* Event = Choices.FindByPredicate([this](int32 Index) { return Run->GetState().Map.Nodes[Index].Type == EDwNodeType::Event; });
				if (Choices.IsEmpty() || (Choices.Num() == 1 && Choices[0] == Run->GetState().Map.BossNode))
				{
					return false;
				}
				Run->ChooseNode(Event ? *Event : Choices[0]);
				if (Event)
				{
					return Run->GetPhase() == EDwRunPhase::Event;
				}
				break;
			}
			case EDwRunPhase::Combat:  Run->ApplyCombatResult(DwRunTest::Won(Run->GetState(), 60.f)); break;
			case EDwRunPhase::Rewards: Run->ClaimRewards(INDEX_NONE, FGuid()); break;
			case EDwRunPhase::LevelUp: Run->ChoosePerk(Run->GetCurrentLevelUp()->PerkChoices.IsEmpty() ? INDEX_NONE : 0); break;
			case EDwRunPhase::Shop:    Run->LeaveShop(); break;
			case EDwRunPhase::Event:   return true;
			default: return false;
			}
		}
		return false;
	}

	/** A run (seed search) whose chart reaches an event node, with only Only in the pool. */
	bool RunToEvent(UDwEventDefinition* Only)
	{
		C.GameData->Events = { Only };
		for (int32 Seed = 1; Seed < 50; ++Seed)
		{
			Run = NewRun(Seed);
			if (SailToEvent())
			{
				return TestEqual(TEXT("The event"), Run->GetEvent().Event.Get(), static_cast<const UDwEventDefinition*>(Only));
			}
		}
		AddError(TEXT("No seed in 1..49 reaches an event node"));
		return false;
	}

	void ExpectRefusals(int32 Count)
	{
		AddExpectedError(TEXT("refused"), EAutomationExpectedErrorFlags::Contains, Count);
	}
END_DEFINE_SPEC(FDwEventSpec)

void FDwEventSpec::Define()
{
	BeforeEach([this]()
	{
		C.Create();
		C.SetProgression({ 100, 100, 100 });
		C.RunRow().GoodEventChance = 0.6f;
		Reset();
	});
	AfterEach([this]() { C.Destroy(); Run = nullptr; });

	Describe("Roll", [this]()
	{
		It("is Good 60% of the time over 10,000 rolls (within 1.5%)", [this]()
		{
			const TArray<const UDwEventDefinition*> Pool = { GoodPlaceholder(), BadPlaceholder() };
			FRandomStream Random(12345);
			int32 Good = 0;
			for (int32 Roll = 0; Roll < 10000; ++Roll)
			{
				const UDwEventDefinition* Picked = FDwEventResolver::Roll(State, Pool, 3, 0.6f, Random);
				Good += Picked && Picked->Valence == EDwEventValence::Good ? 1 : 0;
			}
			const float Share = Good / 10000.f;
			TestTrue(FString::Printf(TEXT("Good share %.4f in 0.585..0.615"), Share), Share >= 0.585f && Share <= 0.615f);
		});

		It("prefers events not seen this run, then allows repeats", [this]()
		{
			UDwEventDefinition* A = MakeEvent(TEXT("DA_Event_A"), EDwEventValence::Good);
			UDwEventDefinition* B = MakeEvent(TEXT("DA_Event_B"), EDwEventValence::Good);
			const TArray<const UDwEventDefinition*> Pool = { A, B };
			State.SeenEvents = { A };
			for (int32 Seed = 0; Seed < 200; ++Seed)
			{
				FRandomStream Random(Seed);
				if (!TestEqual(TEXT("Unseen B"), FDwEventResolver::Roll(State, Pool, 2, 1.f, Random), static_cast<const UDwEventDefinition*>(B)))
				{
					return;
				}
			}
			State.SeenEvents = { A, B };
			TSet<const UDwEventDefinition*> Picked;
			for (int32 Seed = 0; Seed < 200; ++Seed)
			{
				FRandomStream Random(Seed);
				Picked.Add(FDwEventResolver::Roll(State, Pool, 2, 1.f, Random));
			}
			TestEqual(TEXT("All seen: both repeat"), Picked.Num(), 2);
		});

		It("falls back to the other valence, respects layers and weights, and is stable per seed", [this]()
		{
			UDwEventDefinition* Good = GoodPlaceholder();
			const TArray<const UDwEventDefinition*> OnlyGood = { Good };
			FRandomStream BadRoll(3);
			TestEqual(TEXT("Bad rolled, no Bad event: the Good one"), FDwEventResolver::Roll(State, OnlyGood, 1, 0.f, BadRoll),
				static_cast<const UDwEventDefinition*>(Good));

			UDwEventDefinition* Late = MakeEvent(TEXT("DA_Event_Late"), EDwEventValence::Good);
			Late->MinLayer = 5;
			const TArray<const UDwEventDefinition*> OnlyLate = { Late };
			FRandomStream Early(3);
			TestNull(TEXT("Nothing fits layer 2"), FDwEventResolver::Roll(State, OnlyLate, 2, 1.f, Early));
			FRandomStream OnLayer(3);
			TestEqual(TEXT("Fits layer 5"), FDwEventResolver::Roll(State, OnlyLate, 5, 1.f, OnLayer), static_cast<const UDwEventDefinition*>(Late));

			UDwEventDefinition* Heavy = MakeEvent(TEXT("DA_Event_Heavy"), EDwEventValence::Good, {}, 3.f);
			UDwEventDefinition* Light = MakeEvent(TEXT("DA_Event_Light"), EDwEventValence::Good, {}, 1.f);
			UDwEventDefinition* Off = MakeEvent(TEXT("DA_Event_Off"), EDwEventValence::Good, {}, 0.f);
			const TArray<const UDwEventDefinition*> Weighted = { Heavy, Light, Off };
			int32 Heavies = 0;
			for (int32 Seed = 0; Seed < 4000; ++Seed)
			{
				FRandomStream Random(Seed);
				const UDwEventDefinition* Picked = FDwEventResolver::Roll(State, Weighted, 1, 1.f, Random);
				TestNotEqual(TEXT("Weight 0 never"), Picked, static_cast<const UDwEventDefinition*>(Off));
				Heavies += Picked == Heavy ? 1 : 0;
			}
			TestTrue(FString::Printf(TEXT("Weight 3:1 -> %d of 4000 heavy (~3000)"), Heavies), Heavies > 2800 && Heavies < 3200);

			FRandomStream First(99), Second(99);
			TestEqual(TEXT("Same seed, same event"), FDwEventResolver::Roll(State, Weighted, 1, 1.f, First), FDwEventResolver::Roll(State, Weighted, 1, 1.f, Second));
		});
	});

	Describe("Requirements", [this]()
	{
		It("disable a choice with the first failing requirement's text", [this]()
		{
			FDwEventChoice Choice;
			UDwEventRequirement_MinGold* Gold = Outcome<UDwEventRequirement_MinGold>();
			Gold->Amount = 150;
			UDwEventRequirement_CrewCountAtLeast* AtLeast = Outcome<UDwEventRequirement_CrewCountAtLeast>();
			AtLeast->Count = 3;
			Choice.Requirements = { Gold, AtLeast };
			TestEqual(TEXT("Gold first"), FDwEventResolver::GetRequirementBlocker(State, Choice).ToString(), FString(TEXT("Requires 150 gold")));
			State.Gold = 150;
			TestEqual(TEXT("Then crew"), FDwEventResolver::GetRequirementBlocker(State, Choice).ToString(), FString(TEXT("Requires 3 or more crew")));
			State.Crew.Add(DwRunTest::Member(C.Deckhand));
			TestTrue(TEXT("All pass"), FDwEventResolver::GetRequirementBlocker(State, Choice).IsEmpty());

			UDwEventRequirement_CrewCountAtMost* AtMost = Outcome<UDwEventRequirement_CrewCountAtMost>();
			AtMost->Count = 3;
			TestTrue(TEXT("3 aboard <= 3"), AtMost->IsMet(State));
			State.Crew.Add(DwRunTest::Member(C.Deckhand));
			TestFalse(TEXT("4 aboard > 3"), AtMost->IsMet(State));
			State.Crew.Last().bAlive = false;
			TestTrue(TEXT("The dead don't count"), AtMost->IsMet(State));

			UDwEventRequirement_HasItem* Item = Outcome<UDwEventRequirement_HasItem>();
			Item->Item = C.Grog;
			TestFalse(TEXT("No grog"), Item->IsMet(State));
			State.AddItem(C.Grog, 1, 0);
			TestTrue(TEXT("Grog aboard"), Item->IsMet(State));

			UDwEventRequirement_CaptainIs* Captain = Outcome<UDwEventRequirement_CaptainIs>();
			Captain->Captain = C.Captain;
			TestTrue(TEXT("Our captain"), Captain->IsMet(State));
			Captain->Captain = C.Make<UDwCaptainDefinition>(TEXT("DA_Captain_Other"));
			TestFalse(TEXT("Another captain"), Captain->IsMet(State));
		});
	});

	Describe("Outcomes", [this]()
	{
		It("Gold floors at 0", [this]()
		{
			UDwEventOutcome_Gold* Gold = Outcome<UDwEventOutcome_Gold>();
			Gold->Delta = 60;
			Apply(*Gold);
			TestEqual(TEXT("160"), State.Gold, 160);
			TestEqual(TEXT("Earned"), State.Stats.GoldEarned, 60);
			Gold->Delta = -200;
			const TArray<FText> Lines = Apply(*Gold);
			TestEqual(TEXT("Floor 0"), State.Gold, 0);
			TestEqual(TEXT("Says what was lost"), Lines[0].ToString(), FString(TEXT("-160 gold (all you had)")));
		});

		It("Ship and crew Health clamp to max and never below 1 (C10a)", [this]()
		{
			UDwEventOutcome_ShipHealth* Hull = Outcome<UDwEventOutcome_ShipHealth>();
			Hull->Delta = 100;
			Apply(*Hull);
			TestEqual(TEXT("Hull capped"), State.Ship.Health, 200.f);
			Hull->Delta = -500;
			Apply(*Hull);
			TestEqual(TEXT("Hull floors at 1"), State.Ship.Health, 1.f);

			UDwEventOutcome_CrewHealth* Hurt = Outcome<UDwEventOutcome_CrewHealth>();
			Hurt->Delta = -10000;
			Hurt->Selector = EDwCrewSelector::All;
			Apply(*Hurt);
			TestEqual(TEXT("Captain at 1"), Captain().Health, 1.f);
			TestEqual(TEXT("Hand at 1"), Hand().Health, 1.f);
			TestTrue(TEXT("Nobody died"), Captain().bAlive && Hand().bAlive);

			UDwEventOutcome_CrewHealth* Heal = Outcome<UDwEventOutcome_CrewHealth>();
			Heal->Delta = 10000;
			Heal->Selector = EDwCrewSelector::Captain;
			Apply(*Heal);
			TestEqual(TEXT("Captain healed to max"), Captain().Health, Captain().GetMaxHealth());
			TestEqual(TEXT("Hand untouched"), Hand().Health, 1.f);

			Heal->Selector = EDwCrewSelector::Chosen;
			TestTrue(TEXT("Needs a pick"), Heal->NeedsChosenCrew());
			Apply(*Heal, 1, Hand().Id);
			TestEqual(TEXT("Chosen hand healed"), Hand().Health, Hand().GetMaxHealth());

			Hurt->Delta = -5;
			Hurt->Selector = EDwCrewSelector::Random;
			const float Before = Captain().Health + Hand().Health;
			Apply(*Hurt, 4);
			TestEqual(TEXT("Random: exactly one member hit"), Captain().Health + Hand().Health, Before - 5.f);
			State.Crew[1].bAlive = false;
			const float CaptainBefore = Captain().Health;
			Apply(*Hurt, 5);
			TestEqual(TEXT("Random never picks the dead"), Captain().Health, CaptainBefore - 5.f);
		});

		It("GiveItem stops at MaxInventory and says what was left behind", [this]()
		{
			C.EconomyRow().MaxInventory = 3;
			UDwEventOutcome_GiveItem* Give = Outcome<UDwEventOutcome_GiveItem>();
			Give->Item = C.Grog;
			Give->Count = 2;
			Apply(*Give);
			TestEqual(TEXT("2 grog"), State.CountItem(C.Grog->GetFName()), 2);
			const TArray<FText> Lines = Apply(*Give);
			TestEqual(TEXT("Only 1 more fits"), State.CountItem(C.Grog->GetFName()), 3);
			TestEqual(TEXT("1 left behind"), State.Stats.ItemsLeftBehind, 1);
			TestEqual(TEXT("Two lines"), Lines.Num(), 2);
		});

		It("Recruit joins at the shop's recruit level and fails gracefully with a full crew", [this]()
		{
			Captain().Level = Hand().Level = 3;
			UDwEventOutcome_Recruit* Recruit = Outcome<UDwEventOutcome_Recruit>();
			Recruit->Unit = C.Deckhand;
			Apply(*Recruit);
			TestEqual(TEXT("3 crew"), State.CountLivingCrew(), 3);
			TestEqual(TEXT("Level 3 - 1 (P8)"), State.Crew.Last().Level, 2);
			TestTrue(TEXT("Seeded id"), State.Crew.Last().Id.IsValid());
			Apply(*Recruit);
			const TArray<FText> Lines = Apply(*Recruit);
			TestEqual(TEXT("MaxCrew 4"), State.CountLivingCrew(), 4);
			TestTrue(TEXT("Says there is no room"), Lines[0].ToString().Contains(TEXT("No bunk")));
		});

		It("LoseCrew never takes the captain and records the loss", [this]()
		{
			UDwEventOutcome_LoseCrew* Lose = Outcome<UDwEventOutcome_LoseCrew>();
			Lose->Selector = EDwCrewSelector::Random;
			for (int32 Seed = 0; Seed < 20; ++Seed)
			{
				Reset();
				Apply(*Lose, Seed);
				if (!TestTrue(TEXT("Captain alive"), Captain().bAlive) || !TestFalse(TEXT("Hand lost"), Hand().bAlive))
				{
					return;
				}
			}
			TestEqual(TEXT("Loss recorded"), State.Stats.CrewLost.Num(), 1);
			TestEqual(TEXT("At the node"), State.Stats.CrewLost[0].Node, 2);
			Apply(*Lose);
			TestTrue(TEXT("Only the captain left: nothing happens"), Captain().bAlive);
			Lose->Selector = EDwCrewSelector::Chosen;
			TestFalse(TEXT("Can't pick the captain"), Lose->AcceptsChosen(Captain()));
			Lose->Selector = EDwCrewSelector::Captain;
			TArray<FText> Problems;
			Lose->Validate(Problems);
			TestEqual(TEXT("Captain selector is an authoring error"), Problems.Num(), 1);
		});

		It("GrantXp levels up with the captain's XP multiplier and queues the level-ups", [this]()
		{
			UDwEventOutcome_GrantXp* Xp = Outcome<UDwEventOutcome_GrantXp>();
			Xp->Amount = 250;
			Xp->Selector = EDwCrewSelector::All;
			Apply(*Xp);
			TestEqual(TEXT("Captain 250 XP -> level 3"), Captain().Level, 3);
			TestEqual(TEXT("Hand too"), Hand().Level, 3);
			TestEqual(TEXT("Four level-ups queued"), LastLevelUps.Num(), 4);
			TestEqual(TEXT("In order"), LastLevelUps[1].NewLevel, 3);
		});

		It("AbilityChoice offers moves the crew can learn", [this]()
		{
			UDwEventOutcome_AbilityChoice* Learn = Outcome<UDwEventOutcome_AbilityChoice>();
			Learn->Count = 2;
			Apply(*Learn);
			TestEqual(TEXT("Two offers"), LastOffers.Num(), 2);
			for (const FDwAbilityOffer& Offer : LastOffers)
			{
				TestFalse(TEXT("Someone can learn it"), Offer.EligibleCrew.IsEmpty());
			}
		});

		It("RandomOf picks one weighted branch, stable per seed", [this]()
		{
			UDwEventOutcome_RandomOf* Gamble = Outcome<UDwEventOutcome_RandomOf>();
			UDwEventOutcome_Gold* Win = Outcome<UDwEventOutcome_Gold>(Gamble);
			Win->Delta = 100;
			UDwEventOutcome_ShipHealth* Lose = Outcome<UDwEventOutcome_ShipHealth>(Gamble);
			Lose->Delta = -50;
			Gamble->Options.AddDefaulted(2);
			Gamble->Options[0].Weight = 3.f;
			Gamble->Options[0].Outcomes = { Win };
			Gamble->Options[1].Weight = 1.f;
			Gamble->Options[1].Outcomes = { Lose };

			int32 Wins = 0;
			for (int32 Seed = 0; Seed < 4000; ++Seed)
			{
				FRandomStream Random(Seed);
				Wins += Gamble->PickOption(Random) == 0 ? 1 : 0;
			}
			TestTrue(FString::Printf(TEXT("3:1 -> %d of 4000 wins (~3000)"), Wins), Wins > 2800 && Wins < 3200);

			Reset();
			Apply(*Gamble, 42);
			const FDwRunState First = State;
			Reset();
			Apply(*Gamble, 42);
			TestEqual(TEXT("Same seed, same gold"), State.Gold, First.Gold);
			TestEqual(TEXT("Same seed, same hull"), State.Ship.Health, First.Ship.Health);
			TestTrue(TEXT("Exactly one branch"), (State.Gold == 200) != (State.Ship.Health == 100.f));
		});
	});

#if WITH_EDITOR
	Describe("Validation", [this]()
	{
		It("needs an outcome per choice (Nothing counts), text, and a choice with no requirement", [this]()
		{
			UDwEventDefinition* Good = GoodPlaceholder();
			FDataValidationContext Ok;
			TestEqual(TEXT("Placeholder valid"), Good->IsDataValid(Ok), EDataValidationResult::Valid);

			Good->Choices[1].Outcomes.Reset();
			FDataValidationContext NoOutcome;
			TestEqual(TEXT("A choice with no outcome is invalid"), Good->IsDataValid(NoOutcome), EDataValidationResult::Invalid);
			Good->Choices[1].Outcomes = { Outcome<UDwEventOutcome_Nothing>(Good) };

			UDwEventRequirement_MinGold* Gold = Outcome<UDwEventRequirement_MinGold>(Good);
			Gold->Amount = 10;
			for (FDwEventChoice& Choice : Good->Choices)
			{
				Choice.Requirements = { Gold };
			}
			FDataValidationContext Stuck;
			TestEqual(TEXT("Every choice gated: a broke player is stuck"), Good->IsDataValid(Stuck), EDataValidationResult::Invalid);
		});
	});
#endif

	Describe("Run flow", [this]()
	{
		It("rolls an event on arrival, refuses to leave before a choice, then resolves once and returns to the map", [this]()
		{
			UDwEventDefinition* Bad = BadPlaceholder();
			if (!RunToEvent(Bad))
			{
				return;
			}
			TestTrue(TEXT("Seen"), Run->GetState().SeenEvents.Contains(Bad));
			ExpectRefusals(3);
			TestFalse(TEXT("Leave before choosing"), Run->LeaveEvent());
			const float Hull = Run->GetState().Ship.Health;
			TestTrue(TEXT("Choice A"), Run->ChooseEventOption(0, FGuid()));
			TestEqual(TEXT("Hull -40"), Run->GetState().Ship.Health, FMath::Max(1.f, Hull - 40.f));
			TestTrue(TEXT("Resolved"), Run->GetEvent().IsResolved());
			TestEqual(TEXT("One change line"), Run->GetEvent().Changes.Num(), 1);
			TestEqual(TEXT("Still on the event screen"), Run->GetPhase(), EDwRunPhase::Event);
			TestFalse(TEXT("Only one choice per event"), Run->ChooseEventOption(1, FGuid()));
			const int32 Cleared = Run->GetState().Stats.NodesCleared;
			TestTrue(TEXT("Leave"), Run->LeaveEvent());
			TestEqual(TEXT("Map"), Run->GetPhase(), EDwRunPhase::Map);
			TestEqual(TEXT("Node cleared"), Run->GetState().Stats.NodesCleared, Cleared + 1);
			TestFalse(TEXT("Event closed"), Run->GetEvent().IsOpen());
			TestFalse(TEXT("Not on the map"), Run->ChooseEventOption(0, FGuid()));
		});

		It("disables choices whose requirements fail", [this]()
		{
			UDwEventDefinition* Event = GoodPlaceholder();
			UDwEventRequirement_MinGold* Rich = Outcome<UDwEventRequirement_MinGold>(Event);
			Rich->Amount = 100000;
			Event->Choices[0].Requirements = { Rich };
			if (!RunToEvent(Event))
			{
				return;
			}
			FText Reason;
			TestFalse(TEXT("Can't afford"), Run->CanChooseEventOption(0, FGuid(), &Reason));
			TestEqual(TEXT("Reason"), Reason.ToString(), Rich->Describe().ToString());
			TestTrue(TEXT("The other is open"), Run->CanChooseEventOption(1, FGuid()));
			ExpectRefusals(1);
			TestFalse(TEXT("Refused"), Run->ChooseEventOption(0, FGuid()));
		});

		It("lands a Chosen outcome on the picked crew member", [this]()
		{
			UDwEventOutcome_CrewHealth* Hurt = Outcome<UDwEventOutcome_CrewHealth>();
			Hurt->Delta = -15;
			Hurt->Selector = EDwCrewSelector::Chosen;
			UDwEventDefinition* Event = MakeEvent(TEXT("DA_Event_Chosen"), EDwEventValence::Good, { { Hurt } });
			if (!RunToEvent(Event))
			{
				return;
			}
			const FDwCrewMemberState& Hand = Run->GetState().Crew[1];
			const FGuid HandId = Hand.Id;
			FText Reason;
			TestFalse(TEXT("No pick"), Run->CanChooseEventOption(0, FGuid(), &Reason));
			TestEqual(TEXT("Asks for one"), Reason.ToString(), FString(TEXT("Choose a crew member.")));
			const float Before = Hand.Health;
			TestTrue(TEXT("Picked"), Run->ChooseEventOption(0, HandId));
			TestEqual(TEXT("Hand hurt"), Run->GetState().FindCrew(HandId)->Health, FMath::Max(1.f, Before - 15.f));
		});

		It("goes to Rewards for an ability offer and to LevelUp for event XP", [this]()
		{
			UDwEventOutcome_AbilityChoice* Learn = Outcome<UDwEventOutcome_AbilityChoice>();
			Learn->Count = 2;
			UDwEventOutcome_GrantXp* Xp = Outcome<UDwEventOutcome_GrantXp>();
			Xp->Amount = 1000;
			Xp->Selector = EDwCrewSelector::Captain;
			UDwEventDefinition* Event = MakeEvent(TEXT("DA_Event_Teacher"), EDwEventValence::Good, { { Learn, Xp } });
			if (!RunToEvent(Event))
			{
				return;
			}
			TestTrue(TEXT("Chosen"), Run->ChooseEventOption(0, FGuid()));
			TestTrue(TEXT("Leave"), Run->LeaveEvent());
			TestEqual(TEXT("Rewards for the offer"), Run->GetPhase(), EDwRunPhase::Rewards);
			TestEqual(TEXT("No gold from an event's offer"), Run->GetState().PendingRewards.Gold, 0);
			const FDwAbilityOffer& Offer = Run->GetState().PendingRewards.AbilityChoices[0];
			TestTrue(TEXT("Learn it"), Run->ClaimRewards(0, Offer.EligibleCrew[0]));
			TestEqual(TEXT("Then the level-ups"), Run->GetPhase(), EDwRunPhase::LevelUp);
			int32 Guard = 0;
			while (Run->GetPhase() == EDwRunPhase::LevelUp && ++Guard < 10)
			{
				Run->ChoosePerk(Run->GetCurrentLevelUp()->PerkChoices.IsEmpty() ? INDEX_NONE : 0);
			}
			TestEqual(TEXT("Back on the map"), Run->GetPhase(), EDwRunPhase::Map);
		});

		It("with no event for the layer, lets the player sail on", [this]()
		{
			UDwEventDefinition* Never = GoodPlaceholder();
			Never->MinLayer = Never->MaxLayer = 99;
			C.GameData->Events = { Never };
			// SailToEvent stops at the first event node, so exactly one run logs it once.
			AddExpectedError(TEXT("no event in DA_GameData.Events fits layer"), EAutomationExpectedErrorFlags::Contains, 1);
			for (int32 Seed = 1; Seed < 50; ++Seed)
			{
				Run = NewRun(Seed);
				if (SailToEvent())
				{
					TestNull(TEXT("Calm water"), Run->GetEvent().Event.Get());
					TestTrue(TEXT("Leave at once"), Run->LeaveEvent());
					TestEqual(TEXT("Map"), Run->GetPhase(), EDwRunPhase::Map);
					return;
				}
			}
			AddError(TEXT("No seed reaches an event node"));
		});

		It("resolves the same event the same way for the same seed", [this]()
		{
			UDwEventOutcome_RandomOf* Gamble = Outcome<UDwEventOutcome_RandomOf>();
			Gamble->Options.AddDefaulted(2);
			UDwEventOutcome_Gold* Win = Outcome<UDwEventOutcome_Gold>(Gamble);
			Win->Delta = 77;
			UDwEventOutcome_Gold* Lose = Outcome<UDwEventOutcome_Gold>(Gamble);
			Lose->Delta = -33;
			Gamble->Options[0].Outcomes = { Win };
			Gamble->Options[1].Outcomes = { Lose };
			UDwEventDefinition* Event = MakeEvent(TEXT("DA_Event_Dice"), EDwEventValence::Good, { { Gamble } });
			if (!RunToEvent(Event))
			{
				return;
			}
			const int32 Seed = Run->GetSeed();
			Run->ChooseEventOption(0, FGuid());
			const int32 Gold = Run->GetState().Gold;
			UDwRun* First = Run;
			Run = NewRun(Seed);
			SailToEvent();
			Run->ChooseEventOption(0, FGuid());
			TestEqual(TEXT("Same gold"), Run->GetState().Gold, Gold);
			TestNotEqual(TEXT("A different run object"), Run, First);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
