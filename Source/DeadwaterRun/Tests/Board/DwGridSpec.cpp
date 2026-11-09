// Copyright Brandon Hillig 2026

// DW-102: pure grid rules (Docs/Architecture/04-Grid-Board.md "How it's tested").

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Board/DwBoardBuilder.h"
#include "Board/DwDeckLayout.h"
#include "Board/DwGridQueries.h"
#include "Core/DwGameplayTags.h"

namespace DwGridSpec
{
	static FDwGridModel Deck(const TArray<FString>& Rows, EDwSide Side = EDwSide::Player)
	{
		return FDwBoardBuilder::BuildSingleDeck(FDwDeckAscii::Parse(Rows), Side);
	}

	static FDwGridModel OpenDeck(int32 Size)
	{
		TArray<FString> Rows;
		Rows.Init(FString::ChrN(Size, TEXT('.')), Size);
		return Deck(Rows);
	}
}

BEGIN_DEFINE_SPEC(FDwGridSpec, "DeadwaterRun.Rules.Grid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FDwGridRules Rules;
END_DEFINE_SPEC(FDwGridSpec)

void FDwGridSpec::Define()
{
	BeforeEach([this]() { Rules = FDwGridRules(); });

	Describe("Movement", [this]()
	{
		It("reaches the 4-way diamond with budget 5 on an open deck", [this]()
		{
			const FDwGridModel Grid = DwGridSpec::OpenDeck(11);
			const TMap<FIntPoint, int32> Reach = FDwPathfinder::ComputeReachable(Grid, Rules, FIntPoint(5, 5), 5, EDwSide::Player);
			TestEqual(TEXT("Diamond of radius 5 has 2*5*6 tiles"), Reach.Num(), 60);
			TestEqual(TEXT("Diagonal neighbour costs 2 (no diagonal steps)"), Reach.FindRef(FIntPoint(6, 6)), 2);
			TestTrue(TEXT("5 straight steps reachable"), Reach.Contains(FIntPoint(10, 5)));
			TestFalse(TEXT("(3,3) away diagonally = 6 steps, out of budget"), Reach.Contains(FIntPoint(8, 8)));
			TestFalse(TEXT("Start is not a destination"), Reach.Contains(FIntPoint(5, 5)));
		});

		It("is blocked by enemies, passes through allies but never ends on them", [this]()
		{
			FDwGridModel Grid = DwGridSpec::Deck({ TEXT(".....") });
			Grid.SetOccupant(FIntPoint(1, 0), FDwUnitHandle(1), EDwSide::Player);
			Grid.SetOccupant(FIntPoint(3, 0), FDwUnitHandle(2), EDwSide::Enemy);

			const TMap<FIntPoint, int32> Reach = FDwPathfinder::ComputeReachable(Grid, Rules, FIntPoint(0, 0), 5, EDwSide::Player);
			TestFalse(TEXT("Ally tile is not a destination"), Reach.Contains(FIntPoint(1, 0)));
			TestEqual(TEXT("Walks through the ally"), Reach.FindRef(FIntPoint(2, 0)), 2);
			TestFalse(TEXT("Enemy tile is not a destination"), Reach.Contains(FIntPoint(3, 0)));
			TestFalse(TEXT("Can't pass the enemy"), Reach.Contains(FIntPoint(4, 0)));
			TestTrue(TEXT("No path onto an occupied tile"), FDwPathfinder::FindPath(Grid, Rules, FIntPoint(0, 0), FIntPoint(1, 0), EDwSide::Player).IsEmpty());
		});

		It("finds the cheapest path around obstacles", [this]()
		{
			const FDwGridModel Grid = DwGridSpec::Deck({
				TEXT("....."),
				TEXT(".MMM."),
				TEXT("....."),
			});
			const TArray<FIntPoint> Path = FDwPathfinder::FindPath(Grid, Rules, FIntPoint(0, 1), FIntPoint(4, 1), EDwSide::Player);
			TestEqual(TEXT("Path length around the mast"), Path.Num(), 6);
			TestEqual(TEXT("Ends on the goal"), Path.Num() > 0 ? Path.Last() : FIntPoint::NoneValue, FIntPoint(4, 1));
			TestEqual(TEXT("Path cost"), FDwPathfinder::GetPathCost(Grid, Rules, FIntPoint(0, 1), Path), 6);
			TestFalse(TEXT("Never steps on an obstacle"), Path.ContainsByPredicate([&Grid](const FIntPoint& P) { return !Grid.IsStandable(P); }));
		});
	});

	Describe("Nest", [this]()
	{
		It("is entered only from the ladder, at climb cost 2", [this]()
		{
			const FDwGridModel Grid = DwGridSpec::Deck({
				TEXT("....."),
				TEXT("..LN."),
				TEXT("....."),
			});
			const FIntPoint Nest(3, 1);
			TestFalse(TEXT("Budget 3 can't reach the nest (2 to ladder + 2 climb)"),
				FDwPathfinder::ComputeReachable(Grid, Rules, FIntPoint(0, 1), 3, EDwSide::Player).Contains(Nest));
			TestEqual(TEXT("Budget 4 reaches it at cost 4"),
				FDwPathfinder::ComputeReachable(Grid, Rules, FIntPoint(0, 1), 4, EDwSide::Player).FindRef(Nest), 4);

			// From the deck tile right next to the nest you still have to go round to the ladder.
			const TArray<FIntPoint> Path = FDwPathfinder::FindPath(Grid, Rules, FIntPoint(4, 1), Nest, EDwSide::Player);
			TestEqual(TEXT("Adjacent deck tile goes via the ladder (4 + climb 2)"), FDwPathfinder::GetPathCost(Grid, Rules, FIntPoint(4, 1), Path), 6);
			TestEqual(TEXT("Last step comes from the ladder"), Path.Num() >= 2 ? Path[Path.Num() - 2] : FIntPoint::NoneValue, FIntPoint(2, 1));

			Rules.ClimbMoveCost = 5;
			TestEqual(TEXT("Climb cost comes from the rules"),
				FDwPathfinder::ComputeReachable(Grid, Rules, FIntPoint(2, 0), 10, EDwSide::Player).FindRef(Nest), 6);
		});

		It("holds one unit", [this]()
		{
			FDwGridModel Grid = DwGridSpec::Deck({ TEXT("LN") });
			Grid.SetOccupant(FIntPoint(1, 0), FDwUnitHandle(1), EDwSide::Player);
			TestFalse(TEXT("Second unit can't enter an occupied nest"),
				FDwPathfinder::ComputeReachable(Grid, Rules, FIntPoint(0, 0), 5, EDwSide::Player).Contains(FIntPoint(1, 0)));
		});
	});

	Describe("LineOfSight", [this]()
	{
		It("low blockers stop deck units but not the nest", [this]()
		{
			const FDwGridModel DeckGrid = DwGridSpec::Deck({ TEXT("..c..") });
			TestFalse(TEXT("Deck to deck over a crate"), FDwLineOfSight::HasLineOfSight(DeckGrid, FIntPoint(0, 0), FIntPoint(4, 0)));

			const FDwGridModel NestGrid = DwGridSpec::Deck({ TEXT("N.c..") });
			TestTrue(TEXT("Nest sees over the crate"), FDwLineOfSight::HasLineOfSight(NestGrid, FIntPoint(0, 0), FIntPoint(4, 0)));
		});

		It("high blockers stop everyone", [this]()
		{
			const FDwGridModel Grid = DwGridSpec::Deck({ TEXT("N.M..") });
			TestFalse(TEXT("Nest can't see through a mast"), FDwLineOfSight::HasLineOfSight(Grid, FIntPoint(0, 0), FIntPoint(4, 0)));
			TestFalse(TEXT("Deck can't either"), FDwLineOfSight::HasLineOfSight(Grid, FIntPoint(1, 0), FIntPoint(4, 0)));
		});

		It("is never blocked by units, and diagonal lines check both corner tiles", [this]()
		{
			FDwGridModel Grid = DwGridSpec::Deck({ TEXT(".....") });
			Grid.SetOccupant(FIntPoint(2, 0), FDwUnitHandle(1), EDwSide::Enemy);
			TestTrue(TEXT("Units don't block (C6a placeholder)"), FDwLineOfSight::HasLineOfSight(Grid, FIntPoint(0, 0), FIntPoint(4, 0)));

			TArray<FIntPoint> Line;
			FDwLineOfSight::TraceSupercover(FIntPoint(0, 0), FIntPoint(2, 2), Line);
			TestTrue(TEXT("Supercover includes both corner tiles"), Line.Contains(FIntPoint(1, 0)) && Line.Contains(FIntPoint(0, 1)));
			const FDwGridModel Diagonal = DwGridSpec::Deck({
				TEXT("..."),
				TEXT("c.."),
				TEXT("..."),
			});
			TestFalse(TEXT("Crate touching the diagonal blocks a deck viewer"),
				FDwLineOfSight::HasLineOfSight(Diagonal, FIntPoint(0, 0), FIntPoint(2, 2)));
		});
	});

	Describe("Range", [this]()
	{
		It("adds the elevation bonus once, from the nest only", [this]()
		{
			const FDwGridModel Grid = DwGridSpec::Deck({ TEXT("N........") });
			TestTrue(TEXT("Nest: distance 7 within range 5 + 2"), FDwTargetingQuery::IsInRange(Grid, Rules, FIntPoint(0, 0), FIntPoint(7, 0), 5));
			TestFalse(TEXT("Nest: distance 8 is out"), FDwTargetingQuery::IsInRange(Grid, Rules, FIntPoint(0, 0), FIntPoint(8, 0), 5));
			TestFalse(TEXT("Deck: no bonus"), FDwTargetingQuery::IsInRange(Grid, Rules, FIntPoint(1, 0), FIntPoint(7, 0), 5));
			TestEqual(TEXT("Manhattan distance"), FDwLineOfSight::GetDistance(Rules, FIntPoint(0, 0), FIntPoint(3, 4)), 7);
		});
	});

	Describe("LineAndLanding", [this]()
	{
		It("charges to the first unit and stops before it", [this]()
		{
			FDwGridModel Grid = DwGridSpec::Deck({ TEXT("........") });
			Grid.SetOccupant(FIntPoint(5, 0), FDwUnitHandle(9), EDwSide::Enemy);
			const FDwLineHit Hit = FDwTargetingQuery::FirstUnitAlongLine(Grid, Rules, FIntPoint(0, 0), FIntPoint(1, 0), 6);
			TestEqual(TEXT("Hits the enemy"), Hit.Unit.Unit, FDwUnitHandle(9));
			TestEqual(TEXT("Stops on the tile before it"), Hit.StopTile, FIntPoint(4, 0));

			const FDwLineHit Short = FDwTargetingQuery::FirstUnitAlongLine(Grid, Rules, FIntPoint(0, 0), FIntPoint(1, 0), 3);
			TestFalse(TEXT("Out of reach: no unit"), Short.Unit.IsSet());
			TestEqual(TEXT("Moves the full distance"), Short.StopTile, FIntPoint(3, 0));

			const FDwGridModel Blocked = DwGridSpec::Deck({ TEXT("..M...") });
			TestEqual(TEXT("Stops at obstacles"), FDwTargetingQuery::FirstUnitAlongLine(Blocked, Rules, FIntPoint(0, 0), FIntPoint(1, 0), 6).StopTile, FIntPoint(1, 0));
		});

		It("lands a grapple on the free neighbour closest to the puller", [this]()
		{
			FDwGridModel Grid = DwGridSpec::OpenDeck(5);
			Grid.SetOccupant(FIntPoint(1, 2), FDwUnitHandle(1), EDwSide::Player);
			const TOptional<FIntPoint> Landing = FDwTargetingQuery::FreeTileAdjacentTo(Grid, FIntPoint(2, 2), FIntPoint(0, 2));
			TestTrue(TEXT("Found a landing tile"), Landing.IsSet());
			TestNotEqual(TEXT("Never the occupied tile"), Landing.Get(FIntPoint::NoneValue), FIntPoint(1, 2));
			TestEqual(TEXT("Closest free tile toward the puller"),
				FMath::Abs(Landing.Get(FIntPoint::NoneValue).X - 0) + FMath::Abs(Landing.Get(FIntPoint::NoneValue).Y - 2), 3);
		});
	});

	Describe("BoardBuilder", [this]()
	{
		It("joins facing boarding edges, mirrors the enemy and aligns rows by offset", [this]()
		{
			FDwDeckSource Player = FDwDeckAscii::Parse({ TEXT("S..E"), TEXT("S...") });
			FDwDeckSource Enemy = FDwDeckAscii::Parse({ TEXT("S..."), TEXT("SC.E") });
			Player.BoardingRowOffset = 1; // player row 0 lines up with enemy row 1
			const FDwGridModel Grid = FDwBoardBuilder::BuildShipBattle(Player, Enemy);

			TestEqual(TEXT("Width = both decks, no gap"), Grid.Size.X, 8);
			TestEqual(TEXT("Height covers the offset"), Grid.Size.Y, 3);
			TestEqual(TEXT("Row above the player deck is off-board"), Grid.GetTile(FIntPoint(0, 0))->Kind, EDwTileKind::None);
			TestTrue(TEXT("Player spawn tagged for the player"), Grid.GetTile(FIntPoint(0, 1))->Tags.HasTagExact(DwTags::Tile_Spawn_Player));
			TestTrue(TEXT("Enemy mirrored: its spawn column is on the far right"), Grid.GetTile(FIntPoint(7, 1))->Tags.HasTagExact(DwTags::Tile_Spawn_Enemy));
			TestTrue(TEXT("Enemy mirrored: cannon next to its spawn"), Grid.GetTile(FIntPoint(6, 1))->Tags.HasTagExact(DwTags::Tile_Cannon));

			for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
			{
				const FIntPoint P = Grid.ToTile(Index);
				if (Grid.IsStandable(P))
				{
					TestEqual(FString::Printf(TEXT("(%d,%d) owner"), P.X, P.Y), Grid.Tiles[Index].Owner, P.X < 4 ? EDwSide::Player : EDwSide::Enemy);
				}
			}

			Rules.BoardingMoveCost = 3;
			TArray<FDwGridStep> Steps;
			Grid.GetNeighbors(FIntPoint(3, 1), Rules, Steps);
			const FDwGridStep* Cross = Steps.FindByPredicate([](const FDwGridStep& S) { return S.Tile == FIntPoint(4, 1); });
			TestNotNull(TEXT("Facing boarding edges are neighbours"), Cross);
			TestEqual(TEXT("Crossing uses BoardingMoveCost"), Cross ? Cross->Cost : -1, 3);

			Grid.GetNeighbors(FIntPoint(3, 2), Rules, Steps);
			TestFalse(TEXT("Rows without an edge on both sides are not crossable"),
				Steps.ContainsByPredicate([](const FDwGridStep& S) { return S.Tile == FIntPoint(4, 2); }));
		});
	});

	Describe("Layouts", [this]()
	{
		It("round-trips ASCII", [this]()
		{
			const TArray<FString> Rows = { TEXT("S.cE"), TEXT("LN~-"), TEXT("C.M.") };
			TestEqual(TEXT("Export(Parse(x)) == x"), FDwDeckAscii::Export(FDwDeckAscii::Parse(Rows)), Rows);
			TArray<FString> Errors;
			FDwDeckAscii::Parse({ TEXT("..?") }, &Errors);
			TestEqual(TEXT("Unknown characters reported"), Errors.Num(), 1);
		});

		It("validates spawns, edges, connectivity and nests", [this]()
		{
			auto MakeLayout = [](const TArray<FString>& Rows)
			{
				UDwDeckLayout* Layout = NewObject<UDwDeckLayout>(GetTransientPackage());
				const FDwDeckSource Deck = FDwDeckAscii::Parse(Rows);
				Layout->Size = Deck.Size;
				Layout->Tiles = Deck.Tiles;
				return Layout;
			};
			auto HasError = [](const TArray<FString>& Errors, const TCHAR* Text)
			{
				return Errors.ContainsByPredicate([Text](const FString& E) { return E.Contains(Text); });
			};

			TestEqual(TEXT("Valid deck has no errors"), MakeLayout({ TEXT("S.LN"), TEXT("S.CE") })->Validate().Num(), 0);
			TestTrue(TEXT("No spawns"), HasError(MakeLayout({ TEXT("...E") })->Validate(), TEXT("No spawn")));
			TestTrue(TEXT("No boarding edge"), HasError(MakeLayout({ TEXT("S...") })->Validate(), TEXT("boarding edge")));
			TestTrue(TEXT("Edge not rightmost"), HasError(MakeLayout({ TEXT("SE..") })->Validate(), TEXT("rightmost")));
			TestTrue(TEXT("Disconnected tile"), HasError(MakeLayout({ TEXT("S.M.E") })->Validate(), TEXT("can't be reached")));
			TestTrue(TEXT("Nest without ladder"), HasError(MakeLayout({ TEXT("S.N.E") })->Validate(), TEXT("no adjacent ladder")));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
