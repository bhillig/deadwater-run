// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Core/DwDefinitionBase.h"
#include "Board/DwTile.h"
#include "DwDeckLayout.generated.h"

/**
 * One ship deck (or the Kraken arena) as a tile grid. Docs/Architecture/04-Grid-Board.md "Layout assets".
 * DW-004 declares the data and validates its shape; DW-102 adds connectivity/nest/cannon/boarding-edge validation
 * (needs the pathfinder) and FDwBoardBuilder. The painter is DW-110.
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

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
