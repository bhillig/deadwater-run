// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "DwShip.generated.h"

class UDwAbilitySystemComponent;
class UDwShipAttributeSet;

/**
 * A ship in a fight. Owns its ASC and ship attribute set, so cannons, repairs and Kraken slams use the same effects as
 * unit damage. DW-103 adds side, deck and cannon tiles (Docs/Architecture/06-Ship-Cannons.md); BP_Ship_* adds the mesh.
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

private:
	UPROPERTY(VisibleAnywhere, Category = "Deadwater")
	TObjectPtr<UDwAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UDwShipAttributeSet> Attributes;
};
