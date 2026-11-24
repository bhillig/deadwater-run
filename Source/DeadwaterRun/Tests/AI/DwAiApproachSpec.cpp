// Copyright Brandon Hillig 2026

// DW-401 R1: fights stalled once they ran long. Approach measured straight-line distance to attack range, so a unit
// behind a mast, under a nest or in a cul-de-sac sat in a local minimum where every step looked like a step back, and an
// attack it couldn't make from where it stood (Charge into a blocked line, an attack it can't pay for) counted as "in
// range". Approach is now the walk to a tile the unit's attacks can really hit someone from, or, when it can reach
// nobody it can hit, to one of its guns. One test per rule, then a whole fight on the layout that stalled in the simulator.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AI/DwAiScoring.h"
#include "Tests/AI/DwAiTestBench.h"

namespace DwAiApproachSpec
{
	/** Enemy deck (authored facing right, mirrored to x 6-11): a mast wall at x 9, rows 0-3, open only on row 4. */
	static TArray<FString> WalledDeck()
	{
		return { TEXT("..M..E"), TEXT("..M..E"), TEXT("..M..E"), TEXT("..M..E"), TEXT(".....E") };
	}

	/**
	 * The board of the simulator's stalled fight (Conqueror seed 784, Deckhands, node 4), 12 x 4:
	 *   y0  S..LN. | LN....   player nest (4,0), no crossing on row 0; enemy ladder (6,0), nest (7,0)
	 *   y1  S..M.E | E.MM.S   masts (3,1), (8,1), (9,1)
	 *   y2  S.M..E | E....S   mast (2,2)
	 *   y3  S....E | EM...S   mast (7,3)
	 */
	static TArray<FString> StallPlayerDeck() { return { TEXT("S..LN."), TEXT("S..M.E"), TEXT("S.M..E"), TEXT("S....E") }; }
	static TArray<FString> StallEnemyDeck() { return { TEXT("S...NL"), TEXT("S.MM.E"), TEXT("S....E"), TEXT("S...ME") }; }
}

BEGIN_DEFINE_SPEC(FDwAiApproachSpec, "DeadwaterRun.AI.Approach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

	FDwAiTestBench B;

	float ApproachAt(FDwUnitHandle Unit, FIntPoint Tile)
	{
		B.SetActing(Unit);
		const FDwAiScoringContext Context = FDwAiScoringContext::Make(B.Snapshot, Unit, false, false);
		FDwAiWeights Terms;
		FDwAiScoring::ScorePosition(Context, Tile, Terms);
		return Terms.Approach;
	}

	static FString List(const TArray<FDwCombatCommand>& Commands)
	{
		TArray<FString> Lines;
		for (const FDwCombatCommand& C : Commands) { Lines.Add(C.ToString()); }
		return FString::Join(Lines, TEXT(" | "));
	}

END_DEFINE_SPEC(FDwAiApproachSpec)

void FDwAiApproachSpec::Define()
{
	BeforeEach([this]()
	{
		B.Create();
		B.SetBoard(FDwAiTestBench::Deck(), FDwAiTestBench::Deck());
		TestTrue(TEXT("Tuning/AiTuning.json has a Default row"), B.HasDefaultTuning());
	});
	AfterEach([this]() { B.Destroy(); });

	Describe("Approach", [this]()
	{
		It("is the walk around a wall, not the distance as the crow flies", [this]()
		{
			B.SetBoard(FDwAiTestBench::Deck(), DwAiApproachSpec::WalledDeck());
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(10, 0), B.Sword).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(5, 0), B.Sword);
			// Straight line: 5 tiles, 4 short of a cutlass. Walking: down to row 4, through the gap, up to (6,0) or (5,1).
			TestEqual(TEXT("Behind the wall: 12 tiles of walking"), ApproachAt(Me, FIntPoint(10, 0)), 12.f);
			TestEqual(TEXT("At the gap: 7"), ApproachAt(Me, FIntPoint(9, 4)), 7.f);
			TestEqual(TEXT("Next to him: 0"), ApproachAt(Me, FIntPoint(6, 0)), 0.f);
		});

		It("doesn't count a Charge whose straight line is blocked", [this]()
		{
			B.SetBoard(FDwAiTestBench::Deck(), { TEXT("S....E"), TEXT("S....E"), TEXT("S..M.E"), TEXT("S....E"), TEXT("S....E") }); // mast (8,2)
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(10, 2), B.Charge).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(7, 2), B.Sword);
			// 3 tiles away, inside Charge's range 6, but the mast is in the way: walk round to (7,1) or (7,3).
			TestEqual(TEXT("In range but not in line: 4 tiles of walking"), ApproachAt(Me, FIntPoint(10, 2)), 4.f);
			TestEqual(TEXT("Clear line from (7,1): 0"), ApproachAt(Me, FIntPoint(7, 1)), 0.f);
		});

		It("doesn't count an attack the unit can't pay for", [this]()
		{
			FDwUnitSnapshot& Captain = B.AddUnit(EDwSide::Enemy, FIntPoint(10, 2), B.Sword);
			Captain.Abilities.Add(B.Charge);
			Captain.GrantedAbilityTags.AddTag(B.Charge->AbilityTag);
			const FDwUnitHandle Me = Captain.Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(7, 2), B.Sword);
			TestEqual(TEXT("Charge paid for: in reach now"), ApproachAt(Me, FIntPoint(10, 2)), 0.f);
			B.Unit(Me).Mana = 0.f;
			TestEqual(TEXT("No MP: only the cutlass counts, 2 tiles short"), ApproachAt(Me, FIntPoint(10, 2)), 2.f);
		});

		It("heads for a cannon when nobody it can hit is reachable (the last opponent in the nest)", [this]()
		{
			// Standing under a nest a cutlass can't reach stalled fights (both sides waited forever); the guns still work.
			B.SetBoard(FDwAiTestBench::Deck(), { TEXT("S..LNE"), TEXT("S....E"), TEXT("S.C..E"), TEXT("S....E"), TEXT("S....E") }); // nest (7,0), gun (9,2)
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(11, 4), B.Sword).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(7, 0), B.Rifle);
			TestEqual(TEXT("Walk to a tile next to its gun"), ApproachAt(Me, FIntPoint(11, 4)), 3.f);
			TestEqual(TEXT("Beside the gun: 0"), ApproachAt(Me, FIntPoint(9, 3)), 0.f);
			TestEqual(TEXT("Under the nest is no goal: 2 from the gun"), ApproachAt(Me, FIntPoint(7, 1)), 2.f);
		});

		It("is 0 everywhere when no opponent can be reached at all", [this]()
		{
			B.SetBoard(FDwAiTestBench::Deck(), { TEXT("S....."), TEXT("S....."), TEXT("S....."), TEXT("S....."), TEXT("S.....") }); // no boarding edge
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(6, 2), B.Sword).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(0, 2), B.Sword);
			TestEqual(TEXT("At the rail"), ApproachAt(Me, FIntPoint(6, 2)), 0.f);
			TestEqual(TEXT("At the stern"), ApproachAt(Me, FIntPoint(11, 2)), 0.f);
		});
	});

	Describe("Idle units (DW-401 R1)", [this]()
	{
		It("walk around a wall instead of waiting behind it", [this]()
		{
			B.SetBoard(FDwAiTestBench::Deck(), DwAiApproachSpec::WalledDeck());
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(10, 0), B.Sword).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(5, 0), B.Sword);
			const float Before = ApproachAt(Me, FIntPoint(10, 0));
			const FDwCombatCommand Command = B.Decide(Me);
			if (!TestEqual(*FString::Printf(TEXT("Moves: %s"), *Command.ToString()), Command.Type, EDwCommandType::Move)) { return; }
			const float After = ApproachAt(Me, Command.Target.Tile);
			TestTrue(*FString::Printf(TEXT("Closer on foot: %.0f -> %.0f"), Before, After), After < Before);
		});

		It("fire their cannon when no opponent can be reached, instead of idling", [this]()
		{
			// Deckhands weights (Approach 6): with straight-line Approach, walking 4 tiles back to the gun cost more than the shot.
			B.SetBoard(FDwAiTestBench::Deck(), { TEXT("S....."), TEXT("S....."), TEXT("C....."), TEXT("S....."), TEXT("S.....") }); // gun (11,2), no edge
			UDwCrewAiBrain* Deckhands = B.MakeBrain(TEXT("Deckhands"), /*bBoarders*/ true);
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(6, 2), B.Sword).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(0, 1), B.Sword);
			B.AddUnit(EDwSide::Player, FIntPoint(0, 3), B.Sword);
			FRandomStream Rng(1);
			FString Rejected;
			const TArray<FDwCombatCommand> Commands = B.PlayTurn(Me, Rng, &Rejected, Deckhands);
			TestTrue(*FString::Printf(TEXT("Every command legal (%s)"), *Rejected), Rejected.IsEmpty());
			const FDwCombatCommand* Shot = Commands.FindByPredicate([](const FDwCombatCommand& C) { return C.Type == EDwCommandType::Ability; });
			TestTrue(*FString::Printf(TEXT("Walks to the gun and fires at the player ship: %s"), *List(Commands)),
				Shot && Shot->AbilityTag == DwTags::Ability_Cannon && Shot->Target.bShip && Shot->Target.ShipSide == EDwSide::Player);
		});

		It("leave a captain they can't reach in his nest and work their guns", [this]()
		{
			// The simulator's other stall: the last player pirate in his own nest, boarders parked beneath it for the
			// Boarding bonus, nobody able to hit anybody. The guns win it: sink the ship and he goes down with it.
			B.SetBoard({ TEXT("S..LNE"), TEXT("S....E"), TEXT("S.C..E"), TEXT("S....E"), TEXT("S....E") }, FDwAiTestBench::Deck()); // player nest (4,0)
			UDwCrewAiBrain* Deckhands = B.MakeBrain(TEXT("Deckhands"), /*bBoarders*/ true);
			B.AddUnit(EDwSide::Player, FIntPoint(4, 0), B.Sword).Tags.AddTag(DwTags::Unit_Role_Captain);
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(4, 1), B.Sword).Handle;
			FRandomStream Rng(1);
			FString Rejected;
			const TArray<FDwCombatCommand> Commands = B.PlayTurn(Me, Rng, &Rejected, Deckhands);
			TestTrue(*FString::Printf(TEXT("Every command legal (%s)"), *Rejected), Rejected.IsEmpty());
			const FDwCombatCommand* Shot = Commands.FindByPredicate([](const FDwCombatCommand& C) { return C.Type == EDwCommandType::Ability; });
			TestTrue(*FString::Printf(TEXT("Back across to its gun, fires at the player ship: %s"), *List(Commands)),
				Shot && Shot->AbilityTag == DwTags::Ability_Cannon && Shot->Target.bShip && Shot->Target.ShipSide == EDwSide::Player);
		});

		It("engage and finish the fight on the layout that stalled in the simulator", [this]()
		{
			// Conqueror seed 784: both crews stood still for 37 rounds. The captain sat in reach of nobody, the single-choice
			// swordsman behind the mast, the Deckhands under their nest; every reachable tile looked farther away.
			B.SetBoard(DwAiApproachSpec::StallPlayerDeck(), DwAiApproachSpec::StallEnemyDeck());
			UDwCrewAiBrain* Deckhands = B.MakeBrain(TEXT("Deckhands"), /*bBoarders*/ true);
			FDwUnitSnapshot& Captain = B.AddUnit(EDwSide::Player, FIntPoint(5, 0), B.Sword, 200.f);
			Captain.MaxHealth = 200.f;
			Captain.Tags.AddTag(DwTags::Unit_Role_Captain);
			B.AddUnit(EDwSide::Player, FIntPoint(3, 0), B.Sword).Tags.AddTag(DwTags::Rule_Turn_SingleChoice);
			B.AddUnit(EDwSide::Enemy, FIntPoint(8, 0), B.Sword);
			B.AddUnit(EDwSide::Enemy, FIntPoint(9, 0), B.Sword);

			auto Alive = [this](EDwSide Side) { return B.Snapshot.Units.FilterByPredicate([Side](const FDwUnitSnapshot& U) { return U.Side == Side && U.bAlive; }).Num(); };
			FRandomStream Rng(784);
			int32 FirstBlow = INDEX_NONE;
			int32 Round = 1;
			for (; Round <= 30 && Alive(EDwSide::Player) > 0 && Alive(EDwSide::Enemy) > 0; ++Round)
			{
				for (const EDwSide Side : { EDwSide::Player, EDwSide::Enemy })
				{
					B.Snapshot.CannonsUsed.Reset();
					for (FDwUnitSnapshot& U : B.Snapshot.Units)
					{
						if (U.Side == Side) { U.ActionPoints = 1.f; U.MovePoints = U.MoveRange; }
					}
					for (int32 Index = 0; Index < B.Snapshot.Units.Num(); ++Index)
					{
						const FDwUnitSnapshot& U = B.Snapshot.Units[Index];
						if (U.Side != Side || !U.bAlive || Alive(EDwSide::Player) == 0 || Alive(EDwSide::Enemy) == 0)
						{
							continue;
						}
						FString Rejected;
						const TArray<FDwCombatCommand> Commands = B.PlayTurn(U.Handle, Rng, &Rejected, Side == EDwSide::Enemy ? Deckhands : nullptr);
						TestTrue(*FString::Printf(TEXT("Round %d: every command legal (%s)"), Round, *Rejected), Rejected.IsEmpty());
						const bool bBlow = Commands.ContainsByPredicate([](const FDwCombatCommand& C) { return C.Type == EDwCommandType::Ability && C.Target.IsUnit(); });
						FirstBlow = FirstBlow == INDEX_NONE && bBlow ? Round : FirstBlow;
					}
				}
			}
			TestTrue(*FString::Printf(TEXT("Someone lands a blow by round 6 (first: round %d)"), FirstBlow), FirstBlow != INDEX_NONE && FirstBlow <= 6);
			TestTrue(*FString::Printf(TEXT("One crew is wiped out within 30 rounds (players %d, enemies %d left after round %d)"),
				Alive(EDwSide::Player), Alive(EDwSide::Enemy), Round - 1), Alive(EDwSide::Player) == 0 || Alive(EDwSide::Enemy) == 0);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
