// Copyright Brandon Hillig 2026

#include "Abilities/DwAttributeSets.h"

#include "Abilities/DwAbilitySystemComponent.h"
#include "Abilities/DwDamageRules.h"
#include "Core/DwGameplayTags.h"
#include "GameplayEffectExtension.h"

namespace DwAttributes
{
	static AActor* GetInstigator(const FGameplayEffectModCallbackData& Data)
	{
		return Data.EffectSpec.GetContext().GetOriginalInstigator();
	}

	/** Moves a meta attribute into Health and reports depletion once. Shared by both sets. */
	template <typename TSet>
	static void ApplyDamage(TSet& Set, const FGameplayEffectModCallbackData& Data)
	{
		const float Damage = Set.GetIncomingDamage();
		Set.SetIncomingDamage(0.f);
		if (Damage <= 0.f)
		{
			return;
		}
		if (UDwAbilitySystemComponent* DamagedAsc = Cast<UDwAbilitySystemComponent>(Set.GetOwningAbilitySystemComponent()))
		{
			DamagedAsc->SetLastHealthInstigator(GetInstigator(Data)); // Health is set directly below, so its change delegate has no GE data
		}
		const float OldHealth = Set.GetHealth();
		Set.SetHealth(FMath::Clamp(OldHealth - Damage, 0.f, Set.GetMaxHealth()));
		if (OldHealth > 0.f && Set.GetHealth() <= 0.f)
		{
			if (UDwAbilitySystemComponent* ASC = Cast<UDwAbilitySystemComponent>(Set.GetOwningAbilitySystemComponent()))
			{
				ASC->NotifyHealthDepleted(GetInstigator(Data));
			}
		}
	}
}

void UDwUnitAttributeSet::Clamp(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetManaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxMana());
	}
	else if (Attribute == GetFearAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxFear());
	}
	else if (Attribute == GetDamageReductionAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, FDwDamageRules::MaxReduction);
	}
	else if (Attribute == GetMovePointsAttribute() || Attribute == GetActionPointsAttribute()
		|| Attribute == GetMaxHealthAttribute() || Attribute == GetMaxManaAttribute() || Attribute == GetMaxFearAttribute()
		|| Attribute == GetMoveRangeAttribute() || Attribute == GetMaxActionPointsAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UDwUnitAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	Clamp(Attribute, NewValue);
}

void UDwUnitAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	Clamp(Attribute, NewValue);
}

void UDwUnitAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// "Already afraid" is a tag so abilities (Smoke and Fire) can require it.
	if (Attribute == GetFearAttribute() && (OldValue > 0.f) != (NewValue > 0.f))
	{
		ASC->SetLooseGameplayTagCount(DwTags::Status_Afraid, NewValue > 0.f ? 1 : 0);
	}

	// Lowering a maximum drags the current value down with it.
	if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
	{
		ASC->SetNumericAttributeBase(GetHealthAttribute(), NewValue);
	}
	else if (Attribute == GetMaxManaAttribute() && GetMana() > NewValue)
	{
		ASC->SetNumericAttributeBase(GetManaAttribute(), NewValue);
	}
	else if (Attribute == GetMaxFearAttribute() && GetFear() > NewValue)
	{
		ASC->SetNumericAttributeBase(GetFearAttribute(), NewValue);
	}
}

void UDwUnitAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		DwAttributes::ApplyDamage(*this, Data);
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingHealAttribute())
	{
		const float Heal = GetIncomingHeal();
		SetIncomingHeal(0.f);
		if (UDwAbilitySystemComponent* HealedAsc = Cast<UDwAbilitySystemComponent>(GetOwningAbilitySystemComponent()))
		{
			HealedAsc->SetLastHealthInstigator(DwAttributes::GetInstigator(Data));
		}
		if (Heal > 0.f && GetHealth() > 0.f) // the dead are never healed (permadeath)
		{
			SetHealth(FMath::Clamp(GetHealth() + Heal, 0.f, GetMaxHealth()));
		}
	}
}

void UDwShipAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
}

void UDwShipAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
}

void UDwShipAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		DwAttributes::ApplyDamage(*this, Data);
	}
}
