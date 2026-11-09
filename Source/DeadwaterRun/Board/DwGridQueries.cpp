// Copyright Brandon Hillig 2026

#include "Board/DwGridQueries.h"

#include "Algo/Reverse.h"

namespace DwGrid
{
	struct FOpenEntry
	{
		int32 Cost;
		int32 Index;
	};

	// Min-heap on (cost, index): equal costs resolve by board index, so results never depend on hash order.
	struct FOpenLess
	{
		bool operator()(const FOpenEntry& A, const FOpenEntry& B) const
		{
			return A.Cost != B.Cost ? A.Cost < B.Cost : A.Index < B.Index;
		}
	};

	/**
	 * Shared Dijkstra. Expands through allies, never through enemies. Fills Cost (INT32_MAX = unreached) and Parent.
	 * Stops expanding beyond Budget (pass INT32_MAX for unbounded) and early-outs when Goal is settled.
	 */
	static void Search(const FDwGridModel& Grid, const FDwGridRules& Rules, FIntPoint Start, int32 Budget, EDwSide MoverSide,
		TOptional<FIntPoint> Goal, TArray<int32>& OutCost, TArray<int32>& OutParent)
	{
		const int32 Num = Grid.Size.X * Grid.Size.Y;
		OutCost.Init(MAX_int32, Num);
		OutParent.Init(INDEX_NONE, Num);
		if (!Grid.IsInBounds(Start))
		{
			return;
		}

		TArray<FOpenEntry> Open;
		const int32 StartIndex = Grid.ToIndex(Start);
		OutCost[StartIndex] = 0;
		Open.HeapPush({ 0, StartIndex }, FOpenLess());

		TArray<FDwGridStep> Steps;
		while (Open.Num() > 0)
		{
			FOpenEntry Current;
			Open.HeapPop(Current, FOpenLess());
			if (Current.Cost > OutCost[Current.Index])
			{
				continue; // stale entry
			}
			const FIntPoint Tile = Grid.ToTile(Current.Index);
			if (Goal.IsSet() && Tile == Goal.GetValue())
			{
				return;
			}

			Grid.GetNeighbors(Tile, Rules, Steps);
			for (const FDwGridStep& Step : Steps)
			{
				const FDwOccupant Occupant = Grid.GetOccupant(Step.Tile);
				if (Occupant.IsSet() && Occupant.Side != MoverSide)
				{
					continue; // enemies block (C2)
				}
				const int32 NewCost = Current.Cost + Step.Cost;
				const int32 NextIndex = Grid.ToIndex(Step.Tile);
				if (NewCost <= Budget && NewCost < OutCost[NextIndex])
				{
					OutCost[NextIndex] = NewCost;
					OutParent[NextIndex] = Current.Index;
					Open.HeapPush({ NewCost, NextIndex }, FOpenLess());
				}
			}
		}
	}
}

TMap<FIntPoint, int32> FDwPathfinder::ComputeReachable(const FDwGridModel& Grid, const FDwGridRules& Rules, FIntPoint Start, int32 Budget, EDwSide MoverSide)
{
	TMap<FIntPoint, int32> Result;
	if (Budget <= 0)
	{
		return Result;
	}

	TArray<int32> Cost, Parent;
	DwGrid::Search(Grid, Rules, Start, Budget, MoverSide, {}, Cost, Parent);

	for (int32 Index = 0; Index < Cost.Num(); ++Index)
	{
		const FIntPoint Tile = Grid.ToTile(Index);
		// Allies can be walked through but never ended on; Start is not a destination.
		if (Cost[Index] != MAX_int32 && Tile != Start && !Grid.IsOccupied(Tile))
		{
			Result.Add(Tile, Cost[Index]);
		}
	}
	return Result;
}

TArray<FIntPoint> FDwPathfinder::FindPath(const FDwGridModel& Grid, const FDwGridRules& Rules, FIntPoint Start, FIntPoint Goal, EDwSide MoverSide)
{
	TArray<FIntPoint> Path;
	if (Start == Goal || !Grid.IsStandable(Goal) || Grid.IsOccupied(Goal))
	{
		return Path;
	}

	TArray<int32> Cost, Parent;
	DwGrid::Search(Grid, Rules, Start, MAX_int32, MoverSide, Goal, Cost, Parent);

	int32 Index = Grid.ToIndex(Goal);
	if (Cost[Index] == MAX_int32)
	{
		return Path;
	}
	const int32 StartIndex = Grid.ToIndex(Start);
	while (Index != StartIndex && Index != INDEX_NONE)
	{
		Path.Add(Grid.ToTile(Index));
		Index = Parent[Index];
	}
	Algo::Reverse(Path);
	return Path;
}

int32 FDwPathfinder::GetPathCost(const FDwGridModel& Grid, const FDwGridRules& Rules, FIntPoint Start, const TArray<FIntPoint>& Path)
{
	int32 Total = 0;
	FIntPoint Previous = Start;
	TArray<FDwGridStep> Steps;
	for (const FIntPoint& Tile : Path)
	{
		Grid.GetNeighbors(Previous, Rules, Steps);
		const FDwGridStep* Step = Steps.FindByPredicate([&Tile](const FDwGridStep& S) { return S.Tile == Tile; });
		if (!Step)
		{
			return MAX_int32; // not a legal path
		}
		Total += Step->Cost;
		Previous = Tile;
	}
	return Total;
}

void FDwLineOfSight::TraceSupercover(FIntPoint From, FIntPoint To, TArray<FIntPoint>& OutTiles)
{
	// Walks tile centres to tile centres. When the line passes exactly through a corner, both side tiles are
	// included, so a blocker touching the corner blocks (conservative, symmetric for equal elevations).
	OutTiles.Reset();
	const int32 Dx = FMath::Abs(To.X - From.X);
	const int32 Dy = FMath::Abs(To.Y - From.Y);
	const int32 Sx = To.X > From.X ? 1 : -1;
	const int32 Sy = To.Y > From.Y ? 1 : -1;

	FIntPoint P = From;
	OutTiles.Add(P);
	int32 Ix = 0;
	int32 Iy = 0;
	while (Ix < Dx || Iy < Dy)
	{
		// Compare (0.5 + Ix) / Dx with (0.5 + Iy) / Dy without division.
		const int64 Lhs = (1 + 2 * int64(Ix)) * Dy;
		const int64 Rhs = (1 + 2 * int64(Iy)) * Dx;
		if (Lhs == Rhs)
		{
			OutTiles.Add(FIntPoint(P.X + Sx, P.Y));
			OutTiles.Add(FIntPoint(P.X, P.Y + Sy));
			P.X += Sx;
			P.Y += Sy;
			++Ix;
			++Iy;
		}
		else if (Lhs < Rhs)
		{
			P.X += Sx;
			++Ix;
		}
		else
		{
			P.Y += Sy;
			++Iy;
		}
		OutTiles.Add(P);
	}
}

bool FDwLineOfSight::HasLineOfSight(const FDwGridModel& Grid, FIntPoint From, FIntPoint To)
{
	const FDwTile* Viewer = Grid.GetTile(From);
	if (!Viewer || !Grid.IsInBounds(To))
	{
		return false;
	}

	TArray<FIntPoint> Line;
	TraceSupercover(From, To, Line);
	for (const FIntPoint& Tile : Line)
	{
		if (Tile == From || Tile == To)
		{
			continue; // endpoints never block their own line
		}
		const FDwTile* T = Grid.GetTile(Tile);
		if (T && T->BlockerHeight > Viewer->Elevation)
		{
			return false;
		}
	}
	return true;
}

int32 FDwLineOfSight::GetDistance(const FDwGridRules& /*Rules*/, FIntPoint From, FIntPoint To)
{
	return FMath::Abs(To.X - From.X) + FMath::Abs(To.Y - From.Y);
}

bool FDwTargetingQuery::IsInRange(const FDwGridModel& Grid, const FDwGridRules& Rules, FIntPoint From, FIntPoint To, int32 Range)
{
	const FDwTile* Viewer = Grid.GetTile(From);
	if (!Viewer || !Grid.IsInBounds(To))
	{
		return false;
	}
	const int32 Bonus = Viewer->Elevation * Rules.RangeBonusPerElevation;
	return FDwLineOfSight::GetDistance(Rules, From, To) <= Range + Bonus;
}

FDwLineHit FDwTargetingQuery::FirstUnitAlongLine(const FDwGridModel& Grid, const FDwGridRules& Rules, FIntPoint Start, FIntPoint Direction, int32 MaxSteps)
{
	FDwLineHit Hit;
	Hit.StopTile = Start;

	TArray<FDwGridStep> Steps;
	FIntPoint Current = Start;
	for (int32 Step = 0; Step < MaxSteps; ++Step)
	{
		const FIntPoint Next = Current + Direction;
		Grid.GetNeighbors(Current, Rules, Steps);
		if (!Steps.ContainsByPredicate([&Next](const FDwGridStep& S) { return S.Tile == Next; }))
		{
			break; // obstacle, water, edge of the board or an illegal crossing
		}
		const FDwOccupant Occupant = Grid.GetOccupant(Next);
		if (Occupant.IsSet())
		{
			Hit.Unit = Occupant;
			Hit.UnitTile = Next;
			break;
		}
		Current = Next;
		Hit.StopTile = Current;
	}
	return Hit;
}

TOptional<FIntPoint> FDwTargetingQuery::FreeTileAdjacentTo(const FDwGridModel& Grid, FIntPoint Tile, FIntPoint TowardFrom)
{
	TOptional<FIntPoint> Best;
	int32 BestDistance = MAX_int32;
	for (const FIntPoint& Dir : FDwGridModel::Directions)
	{
		const FIntPoint Candidate = Tile + Dir;
		if (!Grid.IsStandable(Candidate) || Grid.IsOccupied(Candidate))
		{
			continue;
		}
		const int32 Distance = FMath::Abs(Candidate.X - TowardFrom.X) + FMath::Abs(Candidate.Y - TowardFrom.Y);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Candidate;
		}
	}
	return Best;
}
