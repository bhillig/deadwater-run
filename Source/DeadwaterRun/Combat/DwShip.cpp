// Copyright Brandon Hillig 2026

#include "Combat/DwShip.h"

#include "Abilities/DwAbilitySystemComponent.h"
#include "Abilities/DwAttributeSets.h"

ADwShip::ADwShip()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	AbilitySystem = CreateDefaultSubobject<UDwAbilitySystemComponent>(TEXT("AbilitySystem"));
	Attributes = CreateDefaultSubobject<UDwShipAttributeSet>(TEXT("Attributes"));
}

UAbilitySystemComponent* ADwShip::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ADwShip::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AbilitySystem->InitAbilityActorInfo(this, this);
}
