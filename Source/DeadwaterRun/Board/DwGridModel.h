// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Board/DwTile.h"
#include "Core/DwTypes.h"
#include "DwGridModel.generated.h"

struct FDwCombatTuningRow;

/**
 * Grid numbers from DT_CombatTuning row "Default" (Docs/Architecture/04-Grid-Board.md).
 * The rules themselves (4-way, Manhattan, pass through allies, one move per turn, touching decks) are code.
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwGridRules
{
	GENERATED_BODY()

	/** Step cost between facing boarding-edge tiles of the two ships. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid") int32 BoardingMoveCost = 1;

	/** Step cost between a ladder and the nest (C5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid") int32 ClimbMoveCost = 2;

	/** Range bonus per level of the viewer's elevation (C5a placeholder +2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid") int32 RangeBonusPerElevation = 2;

	static FDwGridRules FromTuning(const FDwCombatTuningRow& Row);
};

/** A unit standing on a tile. The side is stored so movement rules (enemies block, allies pass) stay world-free. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwOccupant
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid") FDwUnitHandle Unit;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid") EDwSide Side = EDwSide::Neutral;

	bool IsSet() const { return Unit.IsValid(); }
};

/** One legal step from a tile, with its cost. */
struct FDwGridStep
{
	FIntPoint Tile;
	int32 Cost = 1;
};

/**
 * The board of one fight: tiles plus who stands where. Pure value type; the combat subsystem owns the live copy
 * and snapshots copy it. Row-major: Index = Y * Size.X + X.
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwGridModel
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid") FIntPoint Size = FIntPoint::ZeroValue;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid") TArray<FDwTile> Tiles;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid") TArray<FDwOccupant> Occupants;

	/** Resizes to Size with empty (Kind None) tiles and no occupants. */
	void Init(FIntPoint InSize);

	bool IsInBounds(FIntPoint Tile) const { return Tile.X >= 0 && Tile.Y >= 0 && Tile.X < Size.X && Tile.Y < Size.Y; }
	int32 ToIndex(FIntPoint Tile) const { return Tile.Y * Size.X + Tile.X; }
	FIntPoint ToTile(int32 Index) const { return FIntPoint(Index % Size.X, Index / Size.X); }

	/** nullptr when out of bounds. */
	const FDwTile* GetTile(FIntPoint Tile) const;
	FDwTile* GetTileMutable(FIntPoint Tile);

	/** True if a unit can stand here: in bounds, walkable, not water/obstacle/none. */
	bool IsStandable(FIntPoint Tile) const;

	FDwOccupant GetOccupant(FIntPoint Tile) const;
	bool IsOccupied(FIntPoint Tile) const { return GetOccupant(Tile).IsSet(); }
	void SetOccupant(FIntPoint Tile, FDwUnitHandle Unit, EDwSide Side);
	void ClearOccupant(FIntPoint Tile);

	/** The tile a unit stands on, if it is on the board. */
	TOptional<FIntPoint> FindUnit(FDwUnitHandle Unit) const;

	/**
	 * Legal single steps out of Tile (4-way), ignoring occupants. Rules:
	 * - the destination must be standable;
	 * - elevation changes only between a Ladder and an adjacent Nest, cost ClimbMoveCost;
	 * - crossing to the other ship only between two Tile.BoardingEdge tiles, cost BoardingMoveCost;
	 * - otherwise the destination's MoveCost.
	 */
	void GetNeighbors(FIntPoint Tile, const FDwGridRules& Rules, TArray<FDwGridStep>& OutSteps) const;

	/** The 4 orthogonal offsets in a fixed order (+X, -X, +Y, -Y), so every search is deterministic. */
	static const FIntPoint Directions[4];
};
