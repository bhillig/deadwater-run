// Copyright Brandon Hillig 2026

#include "Board/DwGridModel.h"

#include "Core/DwGameplayTags.h"
#include "Data/Tuning/DwTuningRows.h"

const FIntPoint FDwGridModel::Directions[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

FDwGridRules FDwGridRules::FromTuning(const FDwCombatTuningRow& Row)
{
	FDwGridRules Rules;
	Rules.BoardingMoveCost = Row.BoardingMoveCost;
	Rules.ClimbMoveCost = Row.ClimbMoveCost;
	Rules.RangeBonusPerElevation = Row.RangeBonusPerElevation;
	return Rules;
}

void FDwGridModel::Init(FIntPoint InSize)
{
	Size = FIntPoint(FMath::Max(InSize.X, 0), FMath::Max(InSize.Y, 0));
	Tiles.Reset();
	Tiles.SetNum(Size.X * Size.Y);
	Occupants.Reset();
	Occupants.SetNum(Size.X * Size.Y);
}

const FDwTile* FDwGridModel::GetTile(FIntPoint Tile) const
{
	return IsInBounds(Tile) && Tiles.IsValidIndex(ToIndex(Tile)) ? &Tiles[ToIndex(Tile)] : nullptr;
}

FDwTile* FDwGridModel::GetTileMutable(FIntPoint Tile)
{
	return IsInBounds(Tile) && Tiles.IsValidIndex(ToIndex(Tile)) ? &Tiles[ToIndex(Tile)] : nullptr;
}

bool FDwGridModel::IsStandable(FIntPoint Tile) const
{
	const FDwTile* T = GetTile(Tile);
	// Water is only standable where a layout marks it walkable: the Kraken's anchor in its arena.
	return T && T->bWalkable && (T->Kind == EDwTileKind::Deck || T->Kind == EDwTileKind::Nest || T->Kind == EDwTileKind::Ladder
		|| T->Kind == EDwTileKind::Water);
}

FDwOccupant FDwGridModel::GetOccupant(FIntPoint Tile) const
{
	return IsInBounds(Tile) && Occupants.IsValidIndex(ToIndex(Tile)) ? Occupants[ToIndex(Tile)] : FDwOccupant();
}

void FDwGridModel::SetOccupant(FIntPoint Tile, FDwUnitHandle Unit, EDwSide Side)
{
	if (IsInBounds(Tile) && Occupants.IsValidIndex(ToIndex(Tile)))
	{
		Occupants[ToIndex(Tile)].Unit = Unit;
		Occupants[ToIndex(Tile)].Side = Side;
	}
}

void FDwGridModel::ClearOccupant(FIntPoint Tile)
{
	SetOccupant(Tile, FDwUnitHandle(), EDwSide::Neutral);
}

TOptional<FIntPoint> FDwGridModel::FindUnit(FDwUnitHandle Unit) const
{
	if (Unit.IsValid())
	{
		for (int32 Index = 0; Index < Occupants.Num(); ++Index)
		{
			if (Occupants[Index].Unit == Unit)
			{
				return ToTile(Index);
			}
		}
	}
	return {};
}

void FDwGridModel::GetNeighbors(FIntPoint Tile, const FDwGridRules& Rules, TArray<FDwGridStep>& OutSteps) const
{
	OutSteps.Reset();
	const FDwTile* From = GetTile(Tile);
	if (!From)
	{
		return;
	}

	for (const FIntPoint& Dir : Directions)
	{
		const FIntPoint ToPoint = Tile + Dir;
		if (!IsStandable(ToPoint))
		{
			continue;
		}
		const FDwTile& To = *GetTile(ToPoint);

		int32 Cost = FMath::Max(To.MoveCost, 1);

		if (To.Elevation != From->Elevation)
		{
			const bool bLadderToNest = From->Kind == EDwTileKind::Ladder && To.Kind == EDwTileKind::Nest;
			const bool bNestToLadder = From->Kind == EDwTileKind::Nest && To.Kind == EDwTileKind::Ladder;
			if (!bLadderToNest && !bNestToLadder)
			{
				continue;
			}
			Cost = Rules.ClimbMoveCost;
		}

		if (To.Owner != From->Owner)
		{
			const bool bBothEdges = From->Tags.HasTagExact(DwTags::Tile_BoardingEdge) && To.Tags.HasTagExact(DwTags::Tile_BoardingEdge);
			if (!bBothEdges)
			{
				continue;
			}
			Cost = Rules.BoardingMoveCost;
		}

		OutSteps.Add({ ToPoint, Cost });
	}
}
