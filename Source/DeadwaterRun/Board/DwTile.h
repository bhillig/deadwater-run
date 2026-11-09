// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/DwTypes.h"
#include "DwTile.generated.h"

/** Docs/Architecture/04-Grid-Board.md "FDwTile". */
UENUM(BlueprintType)
enum class EDwTileKind : uint8
{
	None,     // Not on the board
	Deck,
	Nest,
	Ladder,
	Water,
	Obstacle
};

/** One grid cell. Pure data: layouts store these, FDwGridModel (DW-102) composes them per fight. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwTile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile")
	EDwTileKind Kind = EDwTileKind::None;

	/** Which ship the tile belongs to; sinking kills units standing on that side's tiles. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile")
	EDwSide Owner = EDwSide::Neutral;

	/** 0 deck, 1 nest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile")
	uint8 Elevation = 0;

	/** 0 none, 1 low (crates, rails), 2 high (mast, cabin). LOS blocks if BlockerHeight > viewer Elevation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile")
	uint8 BlockerHeight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile")
	bool bWalkable = false;

	/** Boarding-edge crossing and ladder climb costs come from FDwGridRules, not from here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile")
	int32 MoveCost = 1;

	/** Tile.Cannon, Tile.BoardingEdge, Tile.Spawn.Player, ... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile", meta = (Categories = "Tile"))
	FGameplayTagContainer Tags;
};
