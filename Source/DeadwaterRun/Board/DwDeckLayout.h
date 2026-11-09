// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Core/DwDefinitionBase.h"
#include "Board/DwTile.h"
#include "DwDeckLayout.generated.h"

/**
 * One ship deck (or the Kraken arena) as a tile grid, authored facing right (boarding edge = rightmost column).
 * Docs/Architecture/04-Grid-Board.md "Layout assets". FDwBoardBuilder composes two of these per fight; FDwDeckAscii
 * converts to and from text. The details-panel painter is DW-110.
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwDeckLayout : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout")
	FIntPoint Size = FIntPoint(6, 4);

	/** Row-major, Size.X * Size.Y entries. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout")
	TArray<FDwTile> Tiles;

	/** Aligns this deck's boarding edge rows with the other ship's. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout")
	int32 BoardingRowOffset = 0;

	/** Grid origin relative to the ship mesh pivot, so tiles sit on the Synty deck. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout")
	FTransform MeshAlignment;

	/** <= 0 uses UDwGameData::TileSize. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout")
	float TileSize = 0.f;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::DeckLayout; }

	/**
	 * Layout rules, usable outside the editor (tests, tools): shape, at least one spawn, boarding edge present (ship decks)
	 * and in the rightmost column, every walkable tile connected, every nest next to a ladder, cannons reachable.
	 * Returns one message per problem; empty = valid.
	 */
	TArray<FString> Validate() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
