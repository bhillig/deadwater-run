// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Board/DwGridModel.h"

class UDwDeckLayout;

/** The tile data of one deck, independent of the asset so tests and tools can build boards from plain arrays. */
struct FDwDeckSource
{
	FIntPoint Size = FIntPoint::ZeroValue;
	TArray<FDwTile> Tiles;
	int32 BoardingRowOffset = 0;

	static DEADWATERRUN_API FDwDeckSource FromLayout(const UDwDeckLayout& Layout);
};

/**
 * ASCII deck format, one character per tile, row 0 at the top. Layouts are authored facing right: the boarding edge
 * is the rightmost column; the builder mirrors the enemy deck. Used by tests, the DW-110 import/export and placeholders.
 *
 *   .  deck              S  deck + spawn          C  deck + cannon       E  deck + boarding edge
 *   L  ladder (deck)     N  nest (elevation 1)    c  crate (low blocker) M  mast/cabin (high blocker)
 *   ~  water             -  not on the board
 */
struct DEADWATERRUN_API FDwDeckAscii
{
	/** Parses rows into a deck. Unknown characters are reported in OutErrors and become '-'. */
	static FDwDeckSource Parse(const TArray<FString>& Rows, TArray<FString>* OutErrors = nullptr);

	/** Inverse of Parse (spawn tags of either side export as 'S'). */
	static TArray<FString> Export(const FDwDeckSource& Deck);
};

/** Composes decks into the board of one fight (Docs/Architecture/04-Grid-Board.md "Layout assets and the builder"). */
struct DEADWATERRUN_API FDwBoardBuilder
{
	/**
	 * Player deck on the left, enemy deck mirrored on the right, boarding edges in adjacent columns (no gap), rows aligned
	 * by BoardingRowOffset. Every board tile of a deck is owned by that deck's side; spawn tags are rewritten to the side
	 * (Tile.Spawn.Player / Tile.Spawn.Enemy) so one layout can serve either side.
	 */
	static FDwGridModel BuildShipBattle(const FDwDeckSource& PlayerDeck, const FDwDeckSource& EnemyDeck);
	static FDwGridModel BuildShipBattle(const UDwDeckLayout& PlayerDeck, const UDwDeckLayout& EnemyDeck);

	/** One deck on its own, owned by Side. Layout validation and single-deck tests use this. */
	static FDwGridModel BuildSingleDeck(const FDwDeckSource& Deck, EDwSide Side);
};
