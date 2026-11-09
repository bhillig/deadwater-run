// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "DwCombatUnit.generated.h"

class UDwAbilitySystemComponent;
class UDwUnitAttributeSet;

/**
 * A combatant on the board (crew, enemy pirate, Kraken body, tentacle). Owns its ASC and unit attribute set
 * (Docs/Architecture/02 D1: the ASC lives on the per-fight actor; the run is pure data).
 * DW-103 adds Handle/Side/Tile/RosterId, spawn from definitions and write-back; BP_Unit_* subclasses add visuals.
 */
UCLASS(Blueprintable)
class DEADWATERRUN_API ADwCombatUnit : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ADwCombatUnit();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UDwAbilitySystemComponent* GetDwAbilitySystem() const { return AbilitySystem; }
	const UDwUnitAttributeSet* GetAttributes() const { return Attributes; }

	virtual void PostInitializeComponents() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Deadwater")
	TObjectPtr<UDwAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UDwUnitAttributeSet> Attributes;
};
