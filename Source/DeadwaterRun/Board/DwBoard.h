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

	UFUNCTION(BlueprintCallable, Category = "Deadwater|Board")
	void SetGrid(const FDwGridModel& InGrid, float InTileSize);

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

	/** Editor-only alignment aid in dev maps: draws this layout's walkable tiles. Never set in shipped levels. */
	UPROPERTY(EditInstanceOnly, Category = "Board|Preview")
	TObjectPtr<UDwDeckLayout> PreviewLayout;

protected:
	virtual void BeginPlay() override;

private:
	UInstancedStaticMeshComponent* GetLayer(EDwHighlightLayer Layer) const;
	FTransform MakeInstanceTransform(FIntPoint Tile, EDwHighlightLayer Layer) const;
	void ApplyLayerMaterials();

	UPROPERTY(VisibleAnywhere, Category = "Board")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Board")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> HighlightLayers;

	UPROPERTY(Transient)
	FDwGridModel Grid;

	UPROPERTY(Transient)
	float TileSize = 100.f;
};
