// Copyright Brandon Hillig 2026

// DW-106: every FDwAiScoring term on hand-made snapshots (no world). Raw term values, before weights.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AI/DwAiScoring.h"
#include "Tests/AI/DwAiTestBench.h"

BEGIN_DEFINE_SPEC(FDwAiScoringSpec, "DeadwaterRun.AI.Scoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

	FDwAiTestBench B;

	FDwAiWeights Action(FDwUnitHandle Source, const UDwAbilityDefinition* Ability, const FDwAbilityTarget& Target, TOptional<FIntPoint> Tile = {})
	{
		const FDwAiScoringContext Context = FDwAiScoringContext::Make(B.Snapshot, Source, false, false);
		const FIntPoint From = Tile.Get(B.Unit(Source).Tile);
		const FDwAbilityPreview Preview = B.Preview(Source, Ability, Target, From);
		TestTrue(TEXT("Preview valid"), Preview.bValid);
		FDwAiWeights Terms;
		FDwAiScoring::ScoreAction(Context, From, *Ability, Preview, Terms);
		return Terms;
	}

	FDwAiWeights Position(FDwUnitHandle Unit, FIntPoint Tile, bool bPreferCannons = false, bool bBoarders = false)
	{
		const FDwAiScoringContext Context = FDwAiScoringContext::Make(B.Snapshot, Unit, bPreferCannons, bBoarders);
		FDwAiWeights Terms;
		FDwAiScoring::ScorePosition(Context, Tile, Terms);
		return Terms;
	}

	float Danger(FDwUnitHandle Unit, FIntPoint Tile)
	{
		return FDwAiScoring::DangerAt(FDwAiScoringContext::Make(B.Snapshot, Unit, false, false), Tile);
	}

END_DEFINE_SPEC(FDwAiScoringSpec)

void FDwAiScoringSpec::Define()
{
	BeforeEach([this]()
	{
		B.Create();
		B.SetBoard(FDwAiTestBench::Deck(), FDwAiTestBench::Deck());
	});
	AfterEach([this]() { B.Destroy(); });

	Describe("Action terms", [this]()
	{
		It("counts damage up to the target's Health, a kill, and how wounded the target already was", [this]()
		{
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(6, 2), B.Sword).Handle;
			const FDwUnitHandle Hurt = B.AddUnit(EDwSide::Player, FIntPoint(5, 2), B.Sword, 10.f).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(0, 0), B.Sword); // so it isn't the last of the crew
			const FDwAiWeights Terms = Action(Me, B.Sword, B.At(Hurt));
			TestEqual(TEXT("Damage capped at 10 HP left"), Terms.Damage, 10.f);
			TestEqual(TEXT("One kill"), Terms.Kill, 1.f);
			TestEqual(TEXT("Missing 90% of its Health"), Terms.LowHealthFocus, 0.9f, 1e-4f);
			TestEqual(TEXT("Not the last enemy or the captain: no win"), Terms.Win, 0.f);
		});

		It("values a kill that ends the fight (the player captain, C10), and only that", [this]()
		{
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(6, 2), B.Sword).Handle;
			FDwUnitSnapshot& Captain = B.AddUnit(EDwSide::Player, FIntPoint(5, 2), B.Sword, 20.f);
			Captain.Tags.AddTag(DwTags::Unit_Role_Captain);
			const FDwUnitHandle CaptainHandle = Captain.Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(0, 0), B.Sword);
			const FDwAiWeights Terms = Action(Me, B.Sword, B.At(CaptainHandle));
			TestEqual(TEXT("Killing the captain wins"), Terms.Win, 1.f);
			TestEqual(TEXT("Captain pressure counts the damage"), Terms.CaptainPressure, 20.f);
			TestEqual(TEXT("The snapshot is untouched"), B.Unit(CaptainHandle).Health, 20.f);
			TestTrue(TEXT("Still alive in the snapshot"), B.Unit(CaptainHandle).bAlive);
		});

		It("counts hull damage, more of it as the ship gets low, and a sinking shot as a win", [this]()
		{
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(9, 2), nullptr).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(0, 0), B.Sword);
			B.Ship(EDwSide::Player).Health = 125.f;
			FDwAiWeights Terms = Action(Me, B.Cannon, FDwAbilityTarget::MakeShip(EDwSide::Player));
			TestEqual(TEXT("70 to the hull"), Terms.ShipDamage, 70.f);
			TestEqual(TEXT("Half the hull gone: half again"), Terms.ShipLowHull, 35.f, 1e-3f);
			TestEqual(TEXT("Not sunk yet"), Terms.Win, 0.f);

			B.Ship(EDwSide::Player).Health = 50.f;
			Terms = Action(Me, B.Cannon, FDwAbilityTarget::MakeShip(EDwSide::Player));
			TestEqual(TEXT("Capped at the hull left"), Terms.ShipDamage, 50.f);
			TestEqual(TEXT("Sinking the player ship wins"), Terms.Win, 1.f);
		});

		It("sees that sinking the enemy ship with your last pirates aboard loses the fight (C12)", [this]()
		{
			// The player-side policy (simulator): the only crew member is boarding the enemy ship.
			const FDwUnitHandle Boarder = B.AddUnit(EDwSide::Player, FIntPoint(8, 0), nullptr).Handle;
			B.AddUnit(EDwSide::Enemy, FIntPoint(11, 4), B.Sword);
			B.Ship(EDwSide::Enemy).Health = 50.f;
			const FDwAiWeights Terms = Action(Boarder, B.Cannon, FDwAbilityTarget::MakeShip(EDwSide::Enemy), FIntPoint(2, 2));
			TestEqual(TEXT("Fired from its own cannon tile it would be a win..."), Terms.Win, 1.f);

			const FDwAiScoringContext Context = FDwAiScoringContext::Make(B.Snapshot, Boarder, false, false);
			FDwAiWeights Aboard;
			// Same shot, but the shooter is imagined standing on the enemy deck: it goes down with the ship.
			FDwAiScoring::ScoreAction(Context, FIntPoint(8, 0), *B.Cannon, B.Preview(Boarder, B.Cannon, FDwAbilityTarget::MakeShip(EDwSide::Enemy), FIntPoint(2, 2)), Aboard);
			TestEqual(TEXT("...but aboard the sinking ship it is a defeat"), Aboard.Win, -1.f);
		});

		It("values a heal by what the ally is missing, never more", [this]()
		{
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(9, 2), B.Heal).Handle;
			const FDwUnitHandle Hurt = B.AddUnit(EDwSide::Enemy, FIntPoint(10, 2), B.Sword, 80.f).Handle;
			const FDwUnitHandle Fine = B.AddUnit(EDwSide::Enemy, FIntPoint(9, 3), B.Sword).Handle;
			TestEqual(TEXT("Missing 20: worth 20 of the 30"), Action(Me, B.Heal, B.At(Hurt)).Heal, 20.f);
			TestEqual(TEXT("Full Health: worth nothing"), Action(Me, B.Heal, B.At(Fine)).Heal, 0.f);
		});

		It("counts MP spent as a cost", [this]()
		{
			FDwAbilityTuningRow* Row = B.AbilityTable->FindRow<FDwAbilityTuningRow>(TEXT("Sword"), TEXT("test"));
			Row->ManaCost = 5;
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(6, 2), B.Sword).Handle;
			const FDwUnitHandle Target = B.AddUnit(EDwSide::Player, FIntPoint(5, 2), B.Sword).Handle;
			TestEqual(TEXT("5 MP"), Action(Me, B.Sword, B.At(Target)).ManaCost, 5.f);
		});
	});

	Describe("Danger", [this]()
	{
		It("adds up the best hit of every opponent that can reach the tile next turn", [this]()
		{
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(11, 2), B.Rifle).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(3, 1), B.Sword);
			B.AddUnit(EDwSide::Player, FIntPoint(3, 2), B.Sword);
			B.AddUnit(EDwSide::Player, FIntPoint(3, 3), B.Sword);
			TestEqual(TEXT("Three cutlasses can reach the edge"), Danger(Me, FIntPoint(6, 2)), 75.f);
			TestEqual(TEXT("Nobody reaches the back of the deck"), Danger(Me, FIntPoint(11, 2)), 0.f);
		});

		It("knows melee can't reach the sniper's nest and walls block rifles", [this]()
		{
			// Enemy deck with a ladder and nest at the front: L at x 8, N at x 7 (row 0); a mast at x 9, row 3.
			B.SetBoard(FDwAiTestBench::Deck(), { TEXT("S..LNE"), TEXT("S....E"), TEXT("S....E"), TEXT("S.M..E"), TEXT("S....E") });
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(11, 4), B.Rifle).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(4, 0), B.Sword);
			TestEqual(TEXT("A cutlass two tiles from the nest can't touch it"), Danger(Me, FIntPoint(7, 0)), 0.f);
			TestEqual(TEXT("...but can hit the ladder"), Danger(Me, FIntPoint(8, 0)), 25.f);

			B.Snapshot.Units.Reset();
			B.Snapshot.Grid.Occupants.Init(FDwOccupant(), B.Snapshot.Grid.Tiles.Num());
			const FDwUnitHandle Me2 = B.AddUnit(EDwSide::Enemy, FIntPoint(11, 4), B.Rifle).Handle;
			FDwUnitSnapshot& Shooter = B.AddUnit(EDwSide::Player, FIntPoint(5, 3), B.Rifle);
			Shooter.Tags.AddTag(DwTags::Status_Rooted); // can't move: only its own tile counts
			TestEqual(TEXT("The mast at (9,3) hides (10,3)"), Danger(Me2, FIntPoint(10, 3)), 0.f);
			TestEqual(TEXT("(10,2) is in the open"), Danger(Me2, FIntPoint(10, 2)), 20.f);
		});

		It("ignores opponents that will skip their turn (frozen, or at MaxFear)", [this]()
		{
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(6, 2), B.Sword).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(5, 2), B.Sword).Tags.AddTag(DwTags::Status_Incapacitated_Frozen);
			B.AddUnit(EDwSide::Player, FIntPoint(5, 1), B.Sword).Fear = 3.f;
			TestEqual(TEXT("No threat"), Danger(Me, FIntPoint(6, 2)), 0.f);
		});
	});

	Describe("Position terms", [this]()
	{
		It("flags the opponent's deck as a sinking risk only when this side's cannons can sink it", [this]()
		{
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(6, 2), B.Sword).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(0, 0), B.Sword);
			TestEqual(TEXT("Healthy ship: no risk"), Position(Me, FIntPoint(5, 2)).SinkingRisk, 0.f);
			B.Ship(EDwSide::Player).Health = 70.f; // one enemy cannon deals 70
			TestEqual(TEXT("About to sink: risky to board"), Position(Me, FIntPoint(5, 2)).SinkingRisk, 1.f);
			TestEqual(TEXT("Own deck is not a boarding risk"), Position(Me, FIntPoint(7, 2)).SinkingRisk, 0.f);
		});

		It("measures how far short of attack range a tile is, ignoring targets the attack can't reach", [this]()
		{
			B.SetBoard({ TEXT("S..LNE"), TEXT("S....E"), TEXT("S....E"), TEXT("S....E"), TEXT("S....E") }, FDwAiTestBench::Deck());
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(11, 2), B.Sword).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(4, 0), B.Sword); // in the player's nest: out of a cutlass's reach
			B.AddUnit(EDwSide::Player, FIntPoint(0, 2), B.Sword);
			TestEqual(TEXT("11 tiles from the reachable one, range 1"), Position(Me, FIntPoint(11, 2)).Approach, 10.f);
			TestEqual(TEXT("Adjacent: in range"), Position(Me, FIntPoint(1, 2)).Approach, 0.f);
		});

		It("gives ranged units credit for height, boarders for the opponent deck, gunners for cannons", [this]()
		{
			B.SetBoard(FDwAiTestBench::Deck(), { TEXT("S..LNE"), TEXT("S....E"), TEXT("S.C..E"), TEXT("S....E"), TEXT("S....E") });
			const FDwUnitHandle Sniper = B.AddUnit(EDwSide::Enemy, FIntPoint(8, 0), B.Rifle).Handle;
			const FDwUnitHandle Swordsman = B.AddUnit(EDwSide::Enemy, FIntPoint(11, 4), B.Sword).Handle;
			B.AddUnit(EDwSide::Player, FIntPoint(0, 0), B.Sword);
			TestEqual(TEXT("Nest: elevation 1"), Position(Sniper, FIntPoint(7, 0)).HighGround, 1.f);
			TestEqual(TEXT("Melee gets nothing from height"), Position(Swordsman, FIntPoint(7, 0)).HighGround, 0.f);
			TestEqual(TEXT("Boarders like the player deck"), Position(Swordsman, FIntPoint(4, 4), false, true).Boarding, 1.f);
			TestEqual(TEXT("Others don't"), Position(Swordsman, FIntPoint(4, 4)).Boarding, 0.f);
			TestEqual(TEXT("Gunners like a loaded cannon next to them"), Position(Swordsman, FIntPoint(9, 3), true).CannonAffinity, 1.f);
			B.Snapshot.CannonsUsed.Add(FIntPoint(9, 2));
			TestEqual(TEXT("...not a fired one"), Position(Swordsman, FIntPoint(9, 3), true).CannonAffinity, 0.f);
		});
	});

	Describe("Candidate", [this]()
	{
		It("drops the danger of an opponent the action kills", [this]()
		{
			const FDwUnitHandle Me = B.AddUnit(EDwSide::Enemy, FIntPoint(6, 2), B.Sword).Handle;
			const FDwUnitHandle Weak = B.AddUnit(EDwSide::Player, FIntPoint(5, 2), B.Sword, 10.f).Handle;
			const FDwAiScoringContext Context = FDwAiScoringContext::Make(B.Snapshot, Me, false, false);
			FDwAiCandidate Kill;
			Kill.MoveTile = FIntPoint(6, 2);
			Kill.AbilityTag = DwTags::Ability_Attack_Melee;
			Kill.Target = B.At(Weak);
			Kill.Preview = B.Preview(Me, B.Sword, Kill.Target);
			FDwAiScoring::ScoreCandidate(Context, Kill, B.Sword, B.DefaultTuning().Weights);
			TestEqual(TEXT("Its only threat is dead"), Kill.Terms.Danger, 0.f);
			TestEqual(TEXT("Standing still it is threatened"), FDwAiScoring::DangerAt(Context, FIntPoint(6, 2)), 25.f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
