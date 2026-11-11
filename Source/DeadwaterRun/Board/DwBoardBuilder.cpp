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
	static FDwTile Place(const FDwTile& Source, EDwSide Owner, EDwSide SpawnSide)
	{
		FDwTile Tile = Source;
		if (Tile.Kind != EDwTileKind::None)
		{
			Tile.Owner = Owner;
		}
		if (IsSpawn(Tile))
		{
			Tile.Tags.RemoveTag(DwTags::Tile_Spawn_Player);
			Tile.Tags.RemoveTag(DwTags::Tile_Spawn_Enemy);
			Tile.Tags.AddTag(SpawnSide == EDwSide::Enemy ? DwTags::Tile_Spawn_Enemy : DwTags::Tile_Spawn_Player);
		}
		return Tile;
	}

	static FDwTile Place(const FDwTile& Source, EDwSide Side)
	{
		return Place(Source, Side, Side);
	}

	/**
	 * Player deck on the left, the other deck mirrored on the right (its rightmost column next to the player's), rows
	 * aligned by BoardingRowOffset. The other deck's tiles are owned by OtherOwner; its spawns are enemy spawns.
	 */
	static FDwGridModel Compose(const FDwDeckSource& PlayerDeck, const FDwDeckSource& OtherDeck, EDwSide OtherOwner)
	{
		const int32 MinOffset = FMath::Min(PlayerDeck.BoardingRowOffset, OtherDeck.BoardingRowOffset);
		const int32 PlayerY = PlayerDeck.BoardingRowOffset - MinOffset;
		const int32 OtherY = OtherDeck.BoardingRowOffset - MinOffset;
		const int32 OtherX = PlayerDeck.Size.X;

		FDwGridModel Grid;
		Grid.Init(FIntPoint(PlayerDeck.Size.X + OtherDeck.Size.X,
			FMath::Max(PlayerY + PlayerDeck.Size.Y, OtherY + OtherDeck.Size.Y)));

		for (int32 Y = 0; Y < PlayerDeck.Size.Y; ++Y)
		{
			for (int32 X = 0; X < PlayerDeck.Size.X; ++X)
			{
				const int32 Source = Y * PlayerDeck.Size.X + X;
				if (PlayerDeck.Tiles.IsValidIndex(Source))
				{
					*Grid.GetTileMutable(FIntPoint(X, PlayerY + Y)) = Place(PlayerDeck.Tiles[Source], EDwSide::Player);
				}
			}
		}
		for (int32 Y = 0; Y < OtherDeck.Size.Y; ++Y)
		{
			for (int32 X = 0; X < OtherDeck.Size.X; ++X)
			{
				// Mirror: the other layout's rightmost column (its boarding edge) lands next to the player's edge.
				const int32 Source = Y * OtherDeck.Size.X + (OtherDeck.Size.X - 1 - X);
				if (OtherDeck.Tiles.IsValidIndex(Source))
				{
					*Grid.GetTileMutable(FIntPoint(OtherX + X, OtherY + Y)) = Place(OtherDeck.Tiles[Source], OtherOwner, EDwSide::Enemy);
				}
			}
		}
		return Grid;
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
			case TEXT('B'):
				// The Kraken body stands in the water: walkable so a unit can occupy it, but cut off from every deck.
				Tile = DwBoard::MakeTile(EDwTileKind::Water, true);
				Tile.Tags.AddTag(DwTags::Tile_Water);
				Tile.Tags.AddTag(DwTags::Tile_Kraken_Body);
				Tile.Tags.AddTag(DwTags::Tile_Spawn_Player); // rewritten to the side by the builder
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
			case EDwTileKind::Water: Ch = T.Tags.HasTagExact(DwTags::Tile_Kraken_Body) ? TEXT('B') : TEXT('~'); break;
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
	return DwBoard::Compose(PlayerDeck, EnemyDeck, EDwSide::Enemy);
}

FDwGridModel FDwBoardBuilder::BuildShipBattle(const UDwDeckLayout& PlayerDeck, const UDwDeckLayout& EnemyDeck)
{
	return BuildShipBattle(FDwDeckSource::FromLayout(PlayerDeck), FDwDeckSource::FromLayout(EnemyDeck));
}

FDwGridModel FDwBoardBuilder::BuildKrakenArena(const FDwDeckSource& PlayerDeck, const FDwDeckSource& Arena)
{
	// Neutral arena tiles: different owner from the player deck and no boarding edges, so nobody steps across.
	return DwBoard::Compose(PlayerDeck, Arena, EDwSide::Neutral);
}

FDwGridModel FDwBoardBuilder::BuildKrakenArena(const UDwDeckLayout& PlayerDeck, const UDwDeckLayout& Arena)
{
	return BuildKrakenArena(FDwDeckSource::FromLayout(PlayerDeck), FDwDeckSource::FromLayout(Arena));
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
