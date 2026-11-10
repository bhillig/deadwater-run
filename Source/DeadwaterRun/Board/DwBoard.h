// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Board/DwGridModel.h"
#include "DwBoard.generated.h"

class UDwDeckLayout;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/** World-space highlight layers (Docs/Architecture/04-Grid-Board.md "Presentation"). */
UENUM(BlueprintType)
enum class EDwHighlightLayer : uint8
{
	Move,
	Path,
	Target,
	AreaOfEffect,
	Danger,
	Hover,
	Preview UMETA(Hidden), // editor layout preview only
	Count UMETA(Hidden)
};

/**
 * Presents an FDwGridModel in the world: maps tiles to world positions and draws highlight layers with one
 * instanced mesh per layer. Holds no rules; the combat subsystem gives it the grid (SetGrid).
 * Board space: tile (X, Y) centre = actor transform * ((X + 0.5) * TileSize, (Y + 0.5) * TileSize, Elevation * ElevationHeight).
 * Colours: per-layer materials here until UDwHighlightPalette arrives (DW-216, settings U3).
 */
UCLASS(Blueprintable)
class DEADWATERRUN_API ADwBoard : public AActor
{
	GENERATED_BODY()

public:
	ADwBoard();

	/**
	 * SeamColumn / SeamGap: columns from SeamColumn on are drawn SeamGap world units further along X. A ship battle puts
	 * the seam between the two decks, so each grid sits inside its own hull instead of the hulls overlapping. Presentation
	 * only: the grid and every rule still see adjacent columns.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Board")
	void SetGrid(const FDwGridModel& InGrid, float InTileSize, int32 InSeamColumn = -1, float InSeamGap = 0.f);

	UFUNCTION(BlueprintPure, Category = "Deadwater|Board")
	float GetTileSize() const { return TileSize; }

	/** Board-space rectangle on the deck plane covering every tile, seam included. */
	FBox2D GetLocalBounds() const;

	const FDwGridModel& GetGrid() const { return Grid; }

	UFUNCTION(BlueprintPure, Category = "Deadwater|Board")
	FVector TileToWorld(FIntPoint Tile) const;

	/** Tile under a world point on the deck plane (elevation 0). */
	UFUNCTION(BlueprintPure, Category = "Deadwater|Board")
	bool WorldToTile(const FVector& WorldPoint, FIntPoint& OutTile) const;

	/** Tile under a cursor ray, elevation-aware: an elevated tile (the nest) is hit before the deck below it. */
	UFUNCTION(BlueprintPure, Category = "Deadwater|Board")
	bool RayToTile(const FVector& RayOrigin, const FVector& RayDirection, FIntPoint& OutTile) const;

	UFUNCTION(BlueprintCallable, Category = "Deadwater|Board")
	void ShowHighlights(EDwHighlightLayer Layer, const TArray<FIntPoint>& Tiles);

	UFUNCTION(BlueprintCallable, Category = "Deadwater|Board")
	void ClearHighlights(EDwHighlightLayer Layer);

	UFUNCTION(BlueprintCallable, Category = "Deadwater|Board")
	void ClearAllHighlights();

	/** Number of highlighted tiles on a layer (tests, UI). */
	UFUNCTION(BlueprintPure, Category = "Deadwater|Board")
	int32 GetHighlightCount(EDwHighlightLayer Layer) const;

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Height of one elevation level in world units (the nest). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board")
	float ElevationHeight = 200.f;

	/** Highlight tiles are drawn slightly above the deck to avoid z-fighting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board")
	float HighlightLift = 2.f;

	/** Fraction of a tile a highlight covers, leaving a visible gap between tiles. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board", meta = (ClampMin = "0.1", ClampMax = "1"))
	float HighlightFill = 0.92f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board")
	TObjectPtr<UStaticMesh> TileMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board")
	TMap<EDwHighlightLayer, TObjectPtr<UMaterialInterface>> LayerMaterials;

	/**
	 * Colour of a layer that has no material in LayerMaterials: HighlightBaseMaterial tinted through its "Color" vector
	 * parameter. With M_TileOverlay (BP_Board) alpha is the fill opacity and the tile gets a solid outline.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board")
	TMap<EDwHighlightLayer, FLinearColor> LayerColors;

	/** Parent of every tinted tile material. Needs a "Color" vector parameter; "EdgeOpacity" is set when it exists. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board")
	TObjectPtr<UMaterialInterface> HighlightBaseMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board", meta = (ClampMin = "0", ClampMax = "1"))
	float HighlightEdgeOpacity = 0.95f;

	/** A faint outline on every standable tile, so the grid reads on real ship decks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Grid")
	bool bDrawGrid = false;

	/** Alpha = fill opacity of a grid tile (usually 0). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Grid")
	FLinearColor GridColor = FLinearColor(1.f, 0.95f, 0.8f, 0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Grid", meta = (ClampMin = "0", ClampMax = "1"))
	float GridEdgeOpacity = 0.35f;

	/** Grey-box decks for fights without ship art (tests, new layouts): every board tile drawn, coloured by owner. BP_Board turns it off. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Placeholder")
	bool bDrawPlaceholderDecks = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Placeholder")
	FLinearColor PlayerDeckColor = FLinearColor(0.11f, 0.06f, 0.025f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Placeholder")
	FLinearColor EnemyDeckColor = FLinearColor(0.09f, 0.02f, 0.02f);

	/** Cannons, ladders, crates and masts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Placeholder")
	FLinearColor FeatureColor = FLinearColor(0.01f, 0.01f, 0.01f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Placeholder")
	FLinearColor WaterColor = FLinearColor(0.004f, 0.03f, 0.07f);

	/** Editor-only alignment aid in dev maps: draws this layout's walkable tiles. Never set in shipped levels. */
	UPROPERTY(EditInstanceOnly, Category = "Board|Preview")
	TObjectPtr<UDwDeckLayout> PreviewLayout;

protected:
	virtual void BeginPlay() override;

private:
	UInstancedStaticMeshComponent* GetLayer(EDwHighlightLayer Layer) const;
	void DrawPlaceholderDecks();
	void DrawGrid();
	UMaterialInterface* MakeTint(const FLinearColor& Color, float EdgeOpacity = -1.f);
	/** Board-space X of a column's low edge, and the column under a board-space X (false in the seam). */
	float ColumnToLocalX(int32 Column) const;
	bool LocalXToColumn(float LocalX, int32& OutColumn) const;
	FVector TileCentreLocal(FIntPoint Tile) const;
	FTransform MakeInstanceTransform(FIntPoint Tile, EDwHighlightLayer Layer) const;
	void ApplyLayerMaterials();

	UPROPERTY(VisibleAnywhere, Category = "Board")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Board")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> HighlightLayers;

	UPROPERTY(VisibleAnywhere, Category = "Board|Placeholder") TObjectPtr<UInstancedStaticMeshComponent> PlayerDeck;
	UPROPERTY(VisibleAnywhere, Category = "Board|Placeholder") TObjectPtr<UInstancedStaticMeshComponent> EnemyDeck;
	UPROPERTY(VisibleAnywhere, Category = "Board|Placeholder") TObjectPtr<UInstancedStaticMeshComponent> Features;
	UPROPERTY(VisibleAnywhere, Category = "Board|Placeholder") TObjectPtr<UStaticMeshComponent> Water;
	UPROPERTY(VisibleAnywhere, Category = "Board|Grid") TObjectPtr<UInstancedStaticMeshComponent> GridOverlay;

	UPROPERTY(Transient)
	FDwGridModel Grid;

	UPROPERTY(Transient)
	float TileSize = 100.f;

	UPROPERTY(Transient)
	int32 SeamColumn = INDEX_NONE;

	UPROPERTY(Transient)
	float SeamGap = 0.f;
};
