// Copyright Brandon Hillig 2026

#include "Board/DwDeckLayout.h"

#include "Algo/AnyOf.h"
#include "Board/DwBoardBuilder.h"
#include "Board/DwGridQueries.h"
#include "Core/DwGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "DwDeckLayout"

TArray<FString> UDwDeckLayout::Validate() const
{
	TArray<FString> Errors;

	if (Size.X <= 0 || Size.Y <= 0)
	{
		Errors.Add(FString::Printf(TEXT("Size must be positive, got %dx%d."), Size.X, Size.Y));
		return Errors;
	}
	if (Tiles.Num() != Size.X * Size.Y)
	{
		Errors.Add(FString::Printf(TEXT("Tiles has %d entries, Size %dx%d needs %d."), Tiles.Num(), Size.X, Size.Y, Size.X * Size.Y));
		return Errors;
	}

	const FDwGridModel Grid = FDwBoardBuilder::BuildSingleDeck(FDwDeckSource::FromLayout(*this), EDwSide::Player);
	const FDwGridRules Rules; // connectivity does not depend on the numbers
	const bool bArena = Tiles.ContainsByPredicate([](const FDwTile& T)
		{ return T.Tags.HasTag(DwTags::Tile_Kraken_Body) || T.Tags.HasTag(DwTags::Tile_Kraken_Tentacle); });

	int32 Spawns = 0;
	int32 Edges = 0;
	TArray<FIntPoint> Standable;
	for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
	{
		const FIntPoint P = Grid.ToTile(Index);
		const FDwTile& T = Grid.Tiles[Index];
		if (Grid.IsStandable(P))
		{
			Standable.Add(P);
		}
		if (T.Tags.HasTagExact(DwTags::Tile_Spawn_Player) || T.Tags.HasTagExact(DwTags::Tile_Spawn_Enemy))
		{
			++Spawns;
			if (!Grid.IsStandable(P))
			{
				Errors.Add(FString::Printf(TEXT("Spawn tile (%d, %d) is not walkable."), P.X, P.Y));
			}
		}
		if (T.Tags.HasTagExact(DwTags::Tile_BoardingEdge))
		{
			++Edges;
			if (P.X != Size.X - 1)
			{
				Errors.Add(FString::Printf(TEXT("Boarding edge (%d, %d) must be in the rightmost column (layouts face right)."), P.X, P.Y));
			}
		}
		if (T.Kind == EDwTileKind::Nest)
		{
			const bool bHasLadder = Algo::AnyOf(FDwGridModel::Directions, [&](const FIntPoint& Dir)
				{ const FDwTile* N = Grid.GetTile(P + Dir); return N && N->Kind == EDwTileKind::Ladder; });
			if (!bHasLadder)
			{
				Errors.Add(FString::Printf(TEXT("Nest (%d, %d) has no adjacent ladder."), P.X, P.Y));
			}
		}
		if (T.Tags.HasTagExact(DwTags::Tile_Cannon))
		{
			const bool bReachable = Grid.IsStandable(P) || Algo::AnyOf(FDwGridModel::Directions, [&](const FIntPoint& Dir)
				{ return Grid.IsStandable(P + Dir); });
			if (!bReachable)
			{
				Errors.Add(FString::Printf(TEXT("Cannon (%d, %d) is neither walkable nor next to a walkable tile."), P.X, P.Y));
			}
		}
	}

	if (Spawns == 0)
	{
		Errors.Add(TEXT("No spawn tiles (S)."));
	}
	if (!bArena && Edges == 0)
	{
		Errors.Add(TEXT("Ship decks need at least one boarding edge tile (E)."));
	}

	// Every walkable tile reachable from the first one (the nest only through its ladder).
	if (Standable.Num() > 0)
	{
		const TMap<FIntPoint, int32> Reached = FDwPathfinder::ComputeReachable(Grid, Rules, Standable[0], MAX_int32 - 1, EDwSide::Player);
		for (int32 Index = 1; Index < Standable.Num(); ++Index)
		{
			if (!Reached.Contains(Standable[Index]))
			{
				Errors.Add(FString::Printf(TEXT("Walkable tile (%d, %d) can't be reached from (%d, %d)."),
					Standable[Index].X, Standable[Index].Y, Standable[0].X, Standable[0].Y));
			}
		}
	}

	return Errors;
}

#if WITH_EDITOR
EDataValidationResult UDwDeckLayout::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	for (const FString& Error : Validate())
	{
		Context.AddError(FText::FromString(Error));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
