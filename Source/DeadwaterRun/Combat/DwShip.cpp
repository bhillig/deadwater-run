// Copyright Brandon Hillig 2026

#include "Combat/DwShip.h"

#include "Abilities/DwAbilitySystemComponent.h"
#include "Abilities/DwAttributeSets.h"
#include "Abilities/DwGameplayAbility.h"
#include "Abilities/DwGameplayEffects.h"
#include "Combat/DwCombatTypes.h"
#include "Core/DwGameData.h"

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

void ADwShip::InitializeFromSetup(EDwSide InSide, const FDwShipSetup& Setup, const TArray<FIntPoint>& InCannonTiles)
{
	Side = InSide;
	CannonTiles = InCannonTiles;

	FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(DwGas::GetSharedEffects().InitShipStats, 1, AbilitySystem->MakeEffectContext());
	if (Spec.IsValid())
	{
		const float Health = Setup.Health < 0.f ? Setup.MaxHealth : FMath::Min(Setup.Health, Setup.MaxHealth);
		UDwGE_InitShipStats::SetValues(*Spec.Data, Setup.MaxHealth, Health, Setup.CannonDamage);
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}
	for (const TSubclassOf<UGameplayEffect>& Upgrade : Setup.UpgradeEffects)
	{
		if (Upgrade)
		{
			AbilitySystem->ApplyGameplayEffectToSelf(Upgrade->GetDefaultObject<UGameplayEffect>(), 1, AbilitySystem->MakeEffectContext());
		}
	}
}

float ADwShip::GetHealth() const
{
	return Attributes ? Attributes->GetHealth() : 0.f;
}

float ADwShip::GetMaxHealth() const
{
	return Attributes ? Attributes->GetMaxHealth() : 0.f;
}

float ADwShip::GetCannonDamage() const
{
	return Attributes ? Attributes->GetCannonDamage() : 0.f;
}

void ADwShip::OnSunk_Implementation()
{
}
