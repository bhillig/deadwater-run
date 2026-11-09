// Copyright Brandon Hillig 2026

#include "Board/DwBoardBuilder.h"

#include "Board/DwDeckLayout.h"
#include "Core/DwGameplayTags.h"

namespace DwBoard
{
	static FDwTile MakeTile(EDwTileKind Kind, bool bWalkable, uint8 Elevation = 0, uint8 Blocker = 0)
	{
		FDwTile Tile;
		Tile.Kind = Kind;
		Tile.bWalkable = bWalkable;
		Tile.Elevation = Elevation;
		Tile.BlockerHeight = Blocker;
		return Tile;
	}

	static FDwTile DeckTile(const FGameplayTag& Extra = FGameplayTag())
	{
		FDwTile Tile = MakeTile(EDwTileKind::Deck, true);
		Tile.Tags.AddTag(DwTags::Tile_Deck);
		if (Extra.IsValid())
		{
			Tile.Tags.AddTag(Extra);
		}
		return Tile;
	}

	static bool IsSpawn(const FDwTile& Tile)
	{
		return Tile.Tags.HasTagExact(DwTags::Tile_Spawn_Player) || Tile.Tags.HasTagExact(DwTags::Tile_Spawn_Enemy);
	}

	/** Copies a deck tile onto the board, assigning ownership and side-specific spawn tags. */
	static FDwTile Place(const FDwTile& Source, EDwSide Side)
	{
		FDwTile Tile = Source;
		if (Tile.Kind != EDwTileKind::None)
		{
			Tile.Owner = Side;
		}
		if (IsSpawn(Tile))
		{
			Tile.Tags.RemoveTag(DwTags::Tile_Spawn_Player);
			Tile.Tags.RemoveTag(DwTags::Tile_Spawn_Enemy);
			Tile.Tags.AddTag(Side == EDwSide::Enemy ? DwTags::Tile_Spawn_Enemy : DwTags::Tile_Spawn_Player);
		}
		return Tile;
	}
}

FDwDeckSource FDwDeckSource::FromLayout(const UDwDeckLayout& Layout)
{
	FDwDeckSource Source;
	Source.Size = Layout.Size;
	Source.Tiles = Layout.Tiles;
	Source.BoardingRowOffset = Layout.BoardingRowOffset;
	return Source;
}

FDwDeckSource FDwDeckAscii::Parse(const TArray<FString>& Rows, TArray<FString>* OutErrors)
{
	FDwDeckSource Deck;
	int32 Width = 0;
	for (const FString& Row : Rows)
	{
		Width = FMath::Max(Width, Row.Len());
	}
	Deck.Size = FIntPoint(Width, Rows.Num());
	Deck.Tiles.Reserve(Width * Rows.Num());

	for (int32 Y = 0; Y < Rows.Num(); ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const TCHAR Ch = X < Rows[Y].Len() ? Rows[Y][X] : TEXT('-');
			FDwTile Tile;
			switch (Ch)
			{
			case TEXT('.'): Tile = DwBoard::DeckTile(); break;
			case TEXT('S'): Tile = DwBoard::DeckTile(DwTags::Tile_Spawn_Player); break;
			case TEXT('C'): Tile = DwBoard::DeckTile(DwTags::Tile_Cannon); break;
			case TEXT('E'): Tile = DwBoard::DeckTile(DwTags::Tile_BoardingEdge); break;
			case TEXT('L'):
				Tile = DwBoard::MakeTile(EDwTileKind::Ladder, true);
				Tile.Tags.AddTag(DwTags::Tile_Ladder);
				break;
			case TEXT('N'):
				Tile = DwBoard::MakeTile(EDwTileKind::Nest, true, /*Elevation*/ 1);
				Tile.Tags.AddTag(DwTags::Tile_Nest);
				break;
			case TEXT('c'): Tile = DwBoard::MakeTile(EDwTileKind::Obstacle, false, 0, /*Blocker*/ 1); break;
			case TEXT('M'): Tile = DwBoard::MakeTile(EDwTileKind::Obstacle, false, 0, /*Blocker*/ 2); break;
			case TEXT('~'):
				Tile = DwBoard::MakeTile(EDwTileKind::Water, false);
				Tile.Tags.AddTag(DwTags::Tile_Water);
				break;
			case TEXT('-'): Tile = DwBoard::MakeTile(EDwTileKind::None, false); break;
			default:
				if (OutErrors)
				{
					OutErrors->Add(FString::Printf(TEXT("Unknown tile character '%c' at (%d, %d)."), Ch, X, Y));
				}
				Tile = DwBoard::MakeTile(EDwTileKind::None, false);
				break;
			}
			Deck.Tiles.Add(Tile);
		}
	}
	return Deck;
}

TArray<FString> FDwDeckAscii::Export(const FDwDeckSource& Deck)
{
	TArray<FString> Rows;
	for (int32 Y = 0; Y < Deck.Size.Y; ++Y)
	{
		FString Row;
		for (int32 X = 0; X < Deck.Size.X; ++X)
		{
			const int32 Index = Y * Deck.Size.X + X;
			if (!Deck.Tiles.IsValidIndex(Index))
			{
				Row.AppendChar(TEXT('-'));
				continue;
			}
			const FDwTile& T = Deck.Tiles[Index];
			TCHAR Ch = TEXT('-');
			switch (T.Kind)
			{
			case EDwTileKind::Deck:
				Ch = DwBoard::IsSpawn(T) ? TEXT('S')
					: T.Tags.HasTagExact(DwTags::Tile_Cannon) ? TEXT('C')
					: T.Tags.HasTagExact(DwTags::Tile_BoardingEdge) ? TEXT('E')
					: TEXT('.');
				break;
			case EDwTileKind::Ladder: Ch = TEXT('L'); break;
			case EDwTileKind::Nest: Ch = TEXT('N'); break;
			case EDwTileKind::Obstacle: Ch = T.BlockerHeight >= 2 ? TEXT('M') : TEXT('c'); break;
			case EDwTileKind::Water: Ch = TEXT('~'); break;
			default: break;
			}
			Row.AppendChar(Ch);
		}
		Rows.Add(Row);
	}
	return Rows;
}

FDwGridModel FDwBoardBuilder::BuildShipBattle(const FDwDeckSource& PlayerDeck, const FDwDeckSource& EnemyDeck)
{
	const int32 MinOffset = FMath::Min(PlayerDeck.BoardingRowOffset, EnemyDeck.BoardingRowOffset);
	const int32 PlayerY = PlayerDeck.BoardingRowOffset - MinOffset;
	const int32 EnemyY = EnemyDeck.BoardingRowOffset - MinOffset;
	const int32 EnemyX = PlayerDeck.Size.X;

	FDwGridModel Grid;
	Grid.Init(FIntPoint(PlayerDeck.Size.X + EnemyDeck.Size.X,
		FMath::Max(PlayerY + PlayerDeck.Size.Y, EnemyY + EnemyDeck.Size.Y)));

	for (int32 Y = 0; Y < PlayerDeck.Size.Y; ++Y)
	{
		for (int32 X = 0; X < PlayerDeck.Size.X; ++X)
		{
			const int32 Source = Y * PlayerDeck.Size.X + X;
			if (PlayerDeck.Tiles.IsValidIndex(Source))
			{
				*Grid.GetTileMutable(FIntPoint(X, PlayerY + Y)) = DwBoard::Place(PlayerDeck.Tiles[Source], EDwSide::Player);
			}
		}
	}
	for (int32 Y = 0; Y < EnemyDeck.Size.Y; ++Y)
	{
		for (int32 X = 0; X < EnemyDeck.Size.X; ++X)
		{
			// Mirror: the enemy layout's rightmost column (its boarding edge) lands next to the player's edge.
			const int32 Source = Y * EnemyDeck.Size.X + (EnemyDeck.Size.X - 1 - X);
			if (EnemyDeck.Tiles.IsValidIndex(Source))
			{
				*Grid.GetTileMutable(FIntPoint(EnemyX + X, EnemyY + Y)) = DwBoard::Place(EnemyDeck.Tiles[Source], EDwSide::Enemy);
			}
		}
	}
	return Grid;
}

FDwGridModel FDwBoardBuilder::BuildShipBattle(const UDwDeckLayout& PlayerDeck, const UDwDeckLayout& EnemyDeck)
{
	return BuildShipBattle(FDwDeckSource::FromLayout(PlayerDeck), FDwDeckSource::FromLayout(EnemyDeck));
}

FDwGridModel FDwBoardBuilder::BuildSingleDeck(const FDwDeckSource& Deck, EDwSide Side)
{
	FDwGridModel Grid;
	Grid.Init(Deck.Size);
	for (int32 Index = 0; Index < Grid.Tiles.Num() && Index < Deck.Tiles.Num(); ++Index)
	{
		Grid.Tiles[Index] = DwBoard::Place(Deck.Tiles[Index], Side);
	}
	return Grid;
}
