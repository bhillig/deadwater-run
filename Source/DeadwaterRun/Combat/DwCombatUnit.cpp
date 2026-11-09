// Copyright Brandon Hillig 2026

#include "Combat/DwCombatUnit.h"

#include "Abilities/DwAbilitySystemComponent.h"
#include "Abilities/DwAttributeSets.h"

ADwCombatUnit::ADwCombatUnit()
{
	// Turn logic never ticks (architecture rule 4); presentation components enable their own tick if needed.
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	AbilitySystem = CreateDefaultSubobject<UDwAbilitySystemComponent>(TEXT("AbilitySystem"));
	Attributes = CreateDefaultSubobject<UDwUnitAttributeSet>(TEXT("Attributes")); // found by the ASC as an owner subobject
}

UAbilitySystemComponent* ADwCombatUnit::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ADwCombatUnit::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AbilitySystem->InitAbilityActorInfo(this, this);
}
