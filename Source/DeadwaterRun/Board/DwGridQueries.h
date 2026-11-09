// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Board/DwGridModel.h"

/**
 * Pure grid queries (Docs/Architecture/04-Grid-Board.md "Pure queries"). No world, no UObjects; used by abilities,
 * the UI, the AI and the simulator alike. All results are deterministic for the same grid.
 */

/** Movement: Dijkstra over FDwGridModel::GetNeighbors with the occupancy rules (enemies block, allies pass, never end on a unit). */
struct DEADWATERRUN_API FDwPathfinder
{
	/** Cost per tile reachable within Budget, excluding Start and occupied tiles. */
	static TMap<FIntPoint, int32> ComputeReachable(const FDwGridModel& Grid, const FDwGridRules& Rules, FIntPoint Start, int32 Budget, EDwSide MoverSide);

	/** Cheapest path Start -> Goal (Start excluded, Goal included), or empty if Goal is unreachable or occupied. */
	static TArray<FIntPoint> FindPath(const FDwGridModel& Grid, const FDwGridRules& Rules, FIntPoint Start, FIntPoint Goal, EDwSide MoverSide);

	/** Total step cost of a path returned by FindPath (0 for an empty path). */
	static int32 GetPathCost(const FDwGridModel& Grid, const FDwGridRules& Rules, FIntPoint Start, const TArray<FIntPoint>& Path);
};

/** Line of sight: supercover grid line, blocked by tiles whose BlockerHeight is above the viewer's elevation. Units never block (C6a). */
struct DEADWATERRUN_API FDwLineOfSight
{
	static bool HasLineOfSight(const FDwGridModel& Grid, FIntPoint From, FIntPoint To);

	/** Manhattan distance (C1). Rules are taken for symmetry with range checks; elevation bonus lives in IsInRange. */
	static int32 GetDistance(const FDwGridRules& Rules, FIntPoint From, FIntPoint To);

	/** Every tile the straight line From -> To passes through, endpoints included, both tiles at exact corners. */
	static void TraceSupercover(FIntPoint From, FIntPoint To, TArray<FIntPoint>& OutTiles);
};

/** Result of walking a straight line until something is in the way (Charge). */
struct FDwLineHit
{
	/** First unit met, if any. */
	FDwOccupant Unit;
	/** Last free tile before the unit or obstacle (Start if the first step is blocked). */
	FIntPoint StopTile = FIntPoint::ZeroValue;
	/** Tile of the unit that was hit. */
	FIntPoint UnitTile = FIntPoint::ZeroValue;
};

struct DEADWATERRUN_API FDwTargetingQuery
{
	/** Distance <= Range + viewer elevation * RangeBonusPerElevation (bonus applied once). */
	static bool IsInRange(const FDwGridModel& Grid, const FDwGridRules& Rules, FIntPoint From, FIntPoint To, int32 Range);

	/**
	 * Steps from Start along a 4-way Direction for at most MaxSteps. Stops at the first occupied tile (Unit set),
	 * or before a tile it can't step onto (obstacle, water, elevation change, a non-boarding ship crossing).
	 */
	static FDwLineHit FirstUnitAlongLine(const FDwGridModel& Grid, const FDwGridRules& Rules, FIntPoint Start, FIntPoint Direction, int32 MaxSteps);

	/** Free standable 4-way neighbour of Tile closest (Manhattan) to TowardFrom; ties broken by direction order. Grapple landing. */
	static TOptional<FIntPoint> FreeTileAdjacentTo(const FDwGridModel& Grid, FIntPoint Tile, FIntPoint TowardFrom);
};
