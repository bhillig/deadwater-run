// Copyright Brandon Hillig 2026

#include "Combat/DwShip.h"

#include "Abilities/DwAbilitySystemComponent.h"
#include "Abilities/DwAttributeSets.h"
#include "Abilities/DwGameplayAbility.h"
#include "Abilities/DwGameplayEffects.h"
#include "Combat/DwCombatTypes.h"
#include "Core/DwGameData.h"
#include "Components/TextRenderComponent.h"

ADwShip::ADwShip()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	AbilitySystem = CreateDefaultSubobject<UDwAbilitySystemComponent>(TEXT("AbilitySystem"));
	Attributes = CreateDefaultSubobject<UDwShipAttributeSet>(TEXT("Attributes"));

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(GetRootComponent());
	Label->SetRelativeLocation(FVector(0.f, 280.f, 120.f)); // over the near rail (the camera is on the +Y side)
	Label->SetUsingAbsoluteRotation(true);
	Label->SetWorldRotation(FRotator(30.f, 90.f, 0.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(34.f);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
	UpdateLabel();
	AbilitySystem->GetGameplayAttributeValueChangeDelegate(UDwShipAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);
	AbilitySystem->GetGameplayAttributeValueChangeDelegate(UDwShipAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);
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

void ADwShip::HandleHealthChanged(const FOnAttributeChangeData& /*Data*/)
{
	UpdateLabel();
}

void ADwShip::UpdateLabel()
{
	if (Label && Attributes)
	{
		Label->SetText(FText::FromString(FString::Printf(TEXT("%s  %d / %d"), Side == EDwSide::Player ? TEXT("YOUR SHIP") : TEXT("ENEMY SHIP"),
			FMath::RoundToInt(Attributes->GetHealth()), FMath::RoundToInt(Attributes->GetMaxHealth()))));
		Label->SetTextRenderColor(Side == EDwSide::Player ? FColor(120, 180, 255) : FColor(255, 120, 100));
	}
}
