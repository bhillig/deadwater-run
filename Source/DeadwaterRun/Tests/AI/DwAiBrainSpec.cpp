// Copyright Brandon Hillig 2026

// DW-106: UDwCrewAiBrain decisions on hand-made snapshots with the shipping default weights (Tuning/AiTuning.json).
// Scenarios from Docs/Architecture/07-Enemy-AI.md "How it's tested", plus the rules the brain must respect.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AI/DwAiScoring.h"
#include "Tests/AI/DwAiTestBench.h"

BEGIN_DEFINE_SPEC(FDwAiBrainSpec, "DeadwaterRun.AI.Brain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

	FDwAiTestBench B;

	/** The unit's whole turn, every command validated by the same rules SubmitCommand uses. */
	TArray<FDwCombatCommand> Turn(FDwUnitHandle Unit, int32 Seed = 1)
	{
		FRandomStream Rng(Seed);
		FString Rejected;
		TArray<FDwCombatCommand> Commands = B.PlayTurn(Unit, Rng, &Rejected);
		TestTrue(*FString::Printf(TEXT("Every command legal (%s)"), *Rejected), Rejected.IsEmpty());
		TestTrue(TEXT("The turn ends with EndUnit"), !Commands.IsEmpty() && Commands.Last().Type == EDwCommandType::EndUnit);
		return Commands;
	}

	static const FDwCombatCommand* FindAbility(const TArray<FDwCombatCommand>& Commands)
	{
		return Commands.FindByPredicate([](const FDwCombatCommand& C) { return C.Type == EDwCommandType::Ability; });
	}

	static FString List(const TArray<FDwCombatCommand>& Commands)
	{
		TArray<FString> Lines;
		for (const FDwCombatCommand& C : Commands) { Lines.Add(C.ToString()); }
		return FString::Join(Lines, TEXT(" | "));
	}

END_DEFINE_SPEC(FDwAiBrainSpec)

void FDwAiBrainSpec::Define()
{
	BeforeEach([this]()
	{
		B.Create();
		B.SetBoard(FDwAiTestBench::Deck(), FDwAiTestBench::Deck());
		TestTrue(TEXT("Tuning/AiTuning.json has a Default row"), B.HasDefaultTuning());
	});
	AfterEach([this]() { B.Destroy(); });

	Describe("Doc 07 scenarios", [this]()
	{
		It("finishes a low-Health target when able", [this]()
		{
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(8, 2), B.Sword).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(5, 1), B.Sword);
			const FDwUnitHandle Wounded = B.AddUnit(EDwSide::Player, FIntPoint(5, 3), B.Sword, 20.f).Handle;
			const TArray<FDwCombatCommand> Commands = Turn(Me);
			const FDwCombatCommand* Attack = FindAbility(Commands);
			TestTrue(*FString::Printf(TEXT("Moves in and cuts down the wounded pirate: %s"), *List(Commands)),
				Commands[0].Type == EDwCommandType::Move && Attack && Attack->Target.Unit == Wounded);
			TestFalse(TEXT("It died"), B.Unit(Wounded).bAlive);
		});

		It("kills the player captain when it can (it wins the fight)", [this]()
		{
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(8, 2), B.Sword).Handle;
			FDwUnitSnapshot& Captain = B.AddUnit(EDwSide::Player, FIntPoint(5, 1), B.Sword, 25.f);
			Captain.Tags.AddTag(DwTags::Unit_Role_Captain);
			const FDwUnitHandle CaptainHandle = Captain.Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(5, 3), B.Sword, 10.f); // weaker, also killable: the lower-Health pick
			const TArray<FDwCombatCommand> Commands = Turn(Me);
			const FDwCombatCommand* Attack = FindAbility(Commands);
			TestTrue(*FString::Printf(TEXT("Goes for the captain: %s"), *List(Commands)), Attack && Attack->Target.Unit == CaptainHandle);
		});

		It("fires a cannon when the player ship is nearly sunk and the profile prefers cannons", [this]()
		{
			B.Profile->bPreferCannons = true;
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(11, 0), B.Sword).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(0, 0), B.Sword);
			B.AddUnit(EDwSide::Player, FIntPoint(0, 4), B.Sword);
			B.Ship(EDwSide::Player).Health = 90.f;
			const TArray<FDwCombatCommand> Commands = Turn(Me);
			const FDwCombatCommand* Shot = FindAbility(Commands);
			TestTrue(*FString::Printf(TEXT("Walks to the cannon and fires at the player ship: %s"), *List(Commands)),
				Shot && Shot->AbilityTag == DwTags::Ability_Cannon && Shot->Target.bShip && Shot->Target.ShipSide == EDwSide::Player);
			TestEqual(TEXT("Hull 90 - 70"), B.Ship(EDwSide::Player).Health, 20.f);
		});

		It("doesn't board a ship about to sink", [this]()
		{
			// The enemy cannon is at the back (11,0), out of this swordsman's reach this turn.
			B.SetBoard(FDwAiTestBench::Deck(), { TEXT("C....E"), TEXT("S....E"), TEXT("S....E"), TEXT("S....E"), TEXT("S....E") });
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(6, 4), B.Sword).Handle;
			const FDwUnitHandle Target = B.AddUnit(EDwSide::Player, FIntPoint(3, 2), B.Sword).Handle;
			const FDwCombatSnapshot Start = B.Snapshot;
			auto EndsOnPlayerDeck = [this](const TArray<FDwCombatCommand>& Commands)
			{
				const FDwCombatCommand* Move = Commands.FindByPredicate([](const FDwCombatCommand& C) { return C.Type == EDwCommandType::Move; });
				const FDwTile* Tile = Move ? B.Snapshot.Grid.GetTile(Move->Target.Tile) : nullptr;
				return Tile && Tile->Owner == EDwSide::Player;
			};

			const TArray<FDwCombatCommand> Healthy = Turn(Me);
			TestTrue(*FString::Printf(TEXT("Healthy hull: boards to attack (%s)"), *List(Healthy)), EndsOnPlayerDeck(Healthy) && FindAbility(Healthy) && FindAbility(Healthy)->Target.Unit == Target);

			B.Snapshot = Start;
			B.Ship(EDwSide::Player).Health = 60.f; // one more enemy cannon shot sinks it
			const TArray<FDwCombatCommand> Sinking = Turn(Me);
			TestFalse(*FString::Printf(TEXT("About to sink: stays off it (%s)"), *List(Sinking)), EndsOnPlayerDeck(Sinking));
		});

		It("doesn't move into a tile threatened by three player units when an equal option is safe", [this]()
		{
			FDwAiTuningRow Tuning = B.DefaultTuning();
			Tuning.MaxMoveCandidates = 0; // every destination, so the threatened twin is certainly considered
			B.Brain->SetTuningOverride(Tuning);
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(11, 2), B.Rifle).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(3, 1), B.Sword);
			B.AddUnit(EDwSide::Player, FIntPoint(3, 2), B.Sword);
			B.AddUnit(EDwSide::Player, FIntPoint(3, 3), B.Sword);
			// Unarmed (no threat), off row 2 so the sniper's walk to the edge (6,2) is clear. In range from (11,2) and from (6,2).
			const FDwUnitHandle Boarder = B.AddUnit(EDwSide::Player, FIntPoint(8, 0), nullptr).Handle;
			B.SetActing(Me);

			const TArray<FDwAiCandidate> Candidates = B.Brain->EvaluateCandidates(B.Snapshot, Me);
			const FDwAiCandidate* Threatened = Candidates.FindByPredicate([Boarder](const FDwAiCandidate& C)
			{
				return C.MoveTile == FIntPoint(6, 2) && C.HasAction() && C.Target.Unit == Boarder;
			});
			if (!TestNotNull(TEXT("Shooting the boarder from the edge is an option"), Threatened)) { return; }
			TestTrue(TEXT("...threatened by all three cutlasses"), Threatened->Terms.Danger >= 75.f);

			const FDwCombatCommand Command = B.Decide(Me);
			const FIntPoint Destination = Command.Type == EDwCommandType::Move ? Command.Target.Tile : B.Unit(Me).Tile;
			const FDwAiCandidate& Chosen = Candidates[0];
			TestEqual(TEXT("The best candidate is what it does"), Chosen.MoveTile, Destination);
			TestEqual(*FString::Printf(TEXT("Chosen tile is safe: %s"), *Chosen.ToString()), Chosen.Terms.Danger, 0.f);
			TestTrue(TEXT("...and still shoots the same target for the same damage"), Chosen.HasAction() && Chosen.Terms.Damage >= Threatened->Terms.Damage);
		});

		It("uses a heal on a wounded ally", [this]()
		{
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(9, 2), B.Heal).Handle;
			const FDwUnitHandle Wounded = B.AddUnit(EDwSide::Enemy, FIntPoint(10, 2), B.Sword, 40.f).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(0, 0), B.Sword);
			B.AddUnit(EDwSide::Player, FIntPoint(0, 4), B.Sword);
			const TArray<FDwCombatCommand> Commands = Turn(Me);
			const FDwCombatCommand* Heal = FindAbility(Commands);
			TestTrue(*FString::Printf(TEXT("Heals the wounded ally: %s"), *List(Commands)),
				Heal && Heal->AbilityTag == DwTags::Ability_Captain_RallyCry && Heal->Target.Unit == Wounded);
			TestEqual(TEXT("40 + 30"), B.Unit(Wounded).Health, 70.f);
		});
	});

	Describe("Snipers (rifle range 6)", [this]()
	{
		It("advance into range when nobody is in reach", [this]()
		{
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(11, 2), B.Rifle).Handle;
			const FDwUnitHandle Target = B.AddUnit(EDwSide::Player, FIntPoint(0, 2), nullptr).Handle;
			const TArray<FDwCombatCommand> Commands = Turn(Me);
			const FDwCombatCommand* Shot = FindAbility(Commands);
			TestTrue(*FString::Printf(TEXT("Moves up and shoots: %s"), *List(Commands)),
				Commands[0].Type == EDwCommandType::Move && Shot && Shot->Target.Unit == Target);
		});

		It("climb the sniper's nest for a shot the deck can't give", [this]()
		{
			// Enemy deck: ladder (8,0), nest (7,0). With 2 MP the deck tiles stay out of range of (0,0); the nest (+2) reaches it.
			B.SetBoard(FDwAiTestBench::Deck(), { TEXT("S..LNE"), TEXT("S....E"), TEXT("S....E"), TEXT("S....E"), TEXT("S....E") });
			FDwUnitSnapshot& Sniper = B.AddUnit(EDwSide::Enemy, FIntPoint(8, 0), B.Rifle);
			Sniper.MovePoints = Sniper.MoveRange = 2.f;
			const FDwUnitHandle Me = Sniper.Handle;
			const FDwUnitHandle Target = B.AddUnit(EDwSide::Player, FIntPoint(0, 0), nullptr).Handle;
			const TArray<FDwCombatCommand> Commands = Turn(Me);
			const FDwCombatCommand* Shot = FindAbility(Commands);
			TestTrue(*FString::Printf(TEXT("Climbs and shoots: %s"), *List(Commands)),
				Commands[0].Type == EDwCommandType::Move && Commands[0].Target.Tile == FIntPoint(7, 0) && Shot && Shot->Target.Unit == Target);
		});
	});

	Describe("Rules", [this]()
	{
		It("moves before acting, acts once, and ends the unit (one move per turn, C3)", [this]()
		{
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(8, 2), B.Sword).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(5, 2), B.Sword);
			const TArray<FDwCombatCommand> Commands = Turn(Me);
			TestEqual(*FString::Printf(TEXT("Move, Ability, EndUnit: %s"), *List(Commands)), Commands.Num(), 3);
			if (Commands.Num() == 3)
			{
				TestEqual(TEXT("Move"), Commands[0].Type, EDwCommandType::Move);
				TestEqual(TEXT("Ability"), Commands[1].Type, EDwCommandType::Ability);
			}
		});

		It("repositions out of danger after acting without moving", [this]()
		{
			FDwUnitSnapshot& Sniper = B.AddUnit(EDwSide::Enemy, FIntPoint(7, 2), B.Rifle);
			Sniper.ActionPoints = 0.f; // already shot this turn
			const FDwUnitHandle Me = Sniper.Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(3, 1), B.Sword);
			B.AddUnit(EDwSide::Player, FIntPoint(3, 2), B.Sword);
			B.AddUnit(EDwSide::Player, FIntPoint(3, 3), B.Sword);
			const float Before = FDwAiScoring::DangerAt(FDwAiScoringContext::Make(B.Snapshot, Me, false, false), FIntPoint(7, 2));
			const FDwCombatCommand Command = B.Decide(Me);
			if (!TestEqual(TEXT("Moves"), Command.Type, EDwCommandType::Move)) { return; }
			const float After = FDwAiScoring::DangerAt(FDwAiScoringContext::Make(B.Snapshot, Me, false, false), Command.Target.Tile);
			TestTrue(*FString::Printf(TEXT("Less exposed: %.0f -> %.0f"), Before, After), After < Before);
		});

		It("never uses the player's items on the enemy side", [this]()
		{
			UDwItemDefinition* Potion = B.MakePotion();
			B.Snapshot.Items.Add(Potion->GetFName(), 3);
			B.Snapshot.ItemDefinitions.Add(Potion->GetFName(), Potion);
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(9, 2), nullptr).Handle;
			B.AddUnit(EDwSide::Enemy, FIntPoint(10, 2), B.Sword, 20.f);
			B.AddUnit(EDwSide::Player, FIntPoint(0, 0), B.Sword);
			B.SetActing(Me);
			const TArray<FDwAiCandidate> Candidates = B.Brain->EvaluateCandidates(B.Snapshot, Me);
			TestFalse(TEXT("No item candidate"), Candidates.ContainsByPredicate([](const FDwAiCandidate& C) { return C.AbilityTag == DwTags::Ability_Item; }));
		});

		It("uses items on the player side (simulator policy)", [this]()
		{
			UDwItemDefinition* Potion = B.MakePotion();
			B.Snapshot.Items.Add(Potion->GetFName(), 1);
			B.Snapshot.ItemDefinitions.Add(Potion->GetFName(), Potion);
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Player, FIntPoint(1, 2), nullptr).Handle;
			const FDwUnitHandle Wounded = B.AddUnit(EDwSide::Player, FIntPoint(0, 2), B.Sword, 30.f).Handle;
			B.AddUnit(EDwSide::Enemy, FIntPoint(11, 0), B.Sword);
			const FDwCombatCommand Command = B.Decide(Me);
			TestTrue(*FString::Printf(TEXT("Potion on the wounded ally: %s"), *Command.ToString()),
				Command.Type == EDwCommandType::Ability && Command.AbilityTag == DwTags::Ability_Item && Command.ItemId == Potion->GetFName() && Command.Target.Unit == Wounded);
		});

		It("doesn't try to move while rooted", [this]()
		{
			FDwUnitSnapshot& Sniper = B.AddUnit(EDwSide::Enemy, FIntPoint(11, 2), B.Rifle);
			Sniper.Tags.AddTag(DwTags::Status_Rooted);
			const FDwUnitHandle Me = Sniper.Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(0, 2), nullptr); // out of range
			const TArray<FDwCombatCommand> Commands = Turn(Me);
			TestEqual(*FString::Printf(TEXT("Nothing to do: %s"), *List(Commands)), Commands.Num(), 1);
		});

		It("moves or attacks, never both, under Rule.Turn.SingleChoice", [this]()
		{
			FDwUnitSnapshot& Raider = B.AddUnit(EDwSide::Enemy, FIntPoint(8, 2), B.Sword);
			Raider.Tags.AddTag(DwTags::Rule_Turn_SingleChoice);
			const FDwUnitHandle Me = Raider.Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(5, 2), B.Sword);
			const TArray<FDwCombatCommand> Commands = Turn(Me);
			const int32 Moves = Commands.FilterByPredicate([](const FDwCombatCommand& C) { return C.Type == EDwCommandType::Move; }).Num();
			const int32 Actions = Commands.FilterByPredicate([](const FDwCombatCommand& C) { return C.Type == EDwCommandType::Ability; }).Num();
			TestTrue(*FString::Printf(TEXT("One of the two: %s"), *List(Commands)), Moves + Actions == 1);
		});

		It("ends at once when it can neither move nor act", [this]()
		{
			FDwUnitSnapshot& Spent = B.AddUnit(EDwSide::Enemy, FIntPoint(8, 2), B.Sword);
			Spent.ActionPoints = 0.f;
			Spent.MovePoints = 0.f;
			const FDwUnitHandle Me = Spent.Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(7, 2), B.Sword);
			TestEqual(TEXT("EndUnit"), B.Decide(Me).Type, EDwCommandType::EndUnit);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
