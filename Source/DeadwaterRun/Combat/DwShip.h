// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "Core/DwTypes.h"
#include "DwShip.generated.h"

class UDwAbilitySystemComponent;
class UDwShipAttributeSet;
struct FDwShipSetup;

/**
 * A ship in a fight (Docs/Architecture/06-Ship-Cannons.md). Owns its ASC and ship attribute set, so cannons, repairs
 * and Kraken slams use the same effects as unit damage. Cannons are Tile.Cannon tiles of this side's deck, in board
 * coordinates. No tick; sinking presentation is GameplayCue.Ship.Sink. BP_Ship_* adds the Synty mesh.
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

	/** Applies GE_InitShipStats (max, current, cannon damage) and upgrade effects. */
	void InitializeFromSetup(EDwSide InSide, const FDwShipSetup& Setup, const TArray<FIntPoint>& InCannonTiles);

	UFUNCTION(BlueprintPure, Category = "Deadwater|Ship") EDwSide GetSide() const { return Side; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|Ship") float GetHealth() const;
	UFUNCTION(BlueprintPure, Category = "Deadwater|Ship") float GetMaxHealth() const;
	UFUNCTION(BlueprintPure, Category = "Deadwater|Ship") float GetCannonDamage() const;
	const TArray<FIntPoint>& GetCannonTiles() const { return CannonTiles; }

	/** Called once when the ship sinks. Default does nothing; BP_Ship_* play the sink presentation. */
	UFUNCTION(BlueprintNativeEvent, Category = "Deadwater|Ship")
	void OnSunk();

private:
	UPROPERTY(VisibleAnywhere, Category = "Deadwater")
	TObjectPtr<UDwAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UDwShipAttributeSet> Attributes;

	UPROPERTY(VisibleInstanceOnly, Category = "Deadwater|Ship") EDwSide Side = EDwSide::Player;
	UPROPERTY(VisibleInstanceOnly, Category = "Deadwater|Ship") TArray<FIntPoint> CannonTiles;
};
