// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "Core/DwTypes.h"
#include "Presentation/DwMeshPiece.h"
#include "DwShip.generated.h"

class ADwBoard;
class UDwAbilitySystemComponent;
class UDwShipAttributeSet;
class UStaticMeshComponent;
struct FDwShipSetup;

/**
 * Props a ship puts on its deck tiles, so any deck layout is dressed without hand placement (DW-307).
 * Tile-space sets (Cannon, LowObstacle, Ladder, Nest): origin = the tile centre at the tile's height, +X towards the
 * boarding edge (the enemy ship), in world units. Ladder pieces are authored for a nest on the +X side and are turned to
 * face the adjacent nest tile.
 * Mast is hull-space: X/Y relative to the mast foot, Z as in the hull, scaled with the hull, so sails and booms keep
 * the ship's proportions.
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwDeckDressing
{
	GENERATED_BODY()

	/** Tile.Cannon tiles. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dressing") TArray<FDwMeshPiece> Cannon;
	/** Obstacle tiles with BlockerHeight 1 (crates, barrels). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dressing") TArray<FDwMeshPiece> LowObstacle;
	/** Obstacle tiles with BlockerHeight 2 or more. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dressing") TArray<FDwMeshPiece> Mast;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dressing") TArray<FDwMeshPiece> Ladder;
	/** Nest tiles; the origin is the nest floor (one elevation above the deck). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dressing") TArray<FDwMeshPiece> Nest;
};

/**
 * A ship in a fight (Docs/Architecture/06-Ship-Cannons.md). Owns its ASC and ship attribute set, so cannons, repairs
 * and Kraken slams use the same effects as unit damage. Cannons are Tile.Cannon tiles of this side's deck, in board
 * coordinates. No tick; sinking presentation is GameplayCue.Ship.Sink.
 *
 * Visuals (DW-307): the actor stands at the centre of its deck, in deck space (+X towards the boarding edge); the enemy
 * ship is the mirror image (X scale -1), exactly like its layout. BP_Ship_* hold the hull as data (HullPieces) and the
 * props for deck features (Dressing); UDwDeckLayout::MeshAlignment places the hull under the grid.
 */
UCLASS(Blueprintable)
class DEADWATERRUN_API ADwShip : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ADwShip();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UDwAbilitySystemComponent* GetDwAbilitySystem() const { return AbilitySystem; }
	const UDwShipAttributeSet* GetAttributes() const { return Attributes; }

	virtual void PostInitializeComponents() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Applies GE_InitShipStats (max, current, cannon damage) and upgrade effects. */
	void InitializeFromSetup(EDwSide InSide, const FDwShipSetup& Setup, const TArray<FIntPoint>& InCannonTiles);

	/**
	 * Places the hull under the deck (MeshAlignment = hull relative to the deck centre at deck height) and dresses this
	 * side's feature tiles. Call after the actor is at its deck centre. Does nothing in instant mode.
	 */
	void BuildVisuals(const ADwBoard& Board, const FTransform& MeshAlignment);

	UFUNCTION(BlueprintPure, Category = "Deadwater|Ship") EDwSide GetSide() const { return Side; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|Ship") float GetHealth() const;
	UFUNCTION(BlueprintPure, Category = "Deadwater|Ship") float GetMaxHealth() const;
	UFUNCTION(BlueprintPure, Category = "Deadwater|Ship") float GetCannonDamage() const;
	const TArray<FIntPoint>& GetCannonTiles() const { return CannonTiles; }

	/** Number of prop components on this ship's deck tiles (tests). */
	int32 GetDressingCount() const { return DressingComponents.Num(); }

	/** Called once when the ship sinks. Default does nothing; BP_Ship_* play the sink presentation. */
	UFUNCTION(BlueprintNativeEvent, Category = "Deadwater|Ship")
	void OnSunk();

	/** The ship without its masts, in the Synty preset's own space. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater|Visuals")
	TArray<FDwMeshPiece> HullPieces;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater|Visuals")
	FDwDeckDressing Dressing;

	/** Parent of HullPieces; its relative transform is the layout's MeshAlignment. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadwater|Visuals")
	TObjectPtr<USceneComponent> Hull;

private:
	bool WantsVisuals() const;

	UPROPERTY(VisibleAnywhere, Category = "Deadwater")
	TObjectPtr<UDwAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UDwShipAttributeSet> Attributes;

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> HullComponents;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> DressingComponents;

	UPROPERTY(VisibleInstanceOnly, Category = "Deadwater|Ship") EDwSide Side = EDwSide::Player;
	UPROPERTY(VisibleInstanceOnly, Category = "Deadwater|Ship") TArray<FIntPoint> CannonTiles;
};
