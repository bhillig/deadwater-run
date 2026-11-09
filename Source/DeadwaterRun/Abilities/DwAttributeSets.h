// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "DwAttributeSets.generated.h"

/*
 * Attribute sets (Docs/Architecture/02-GAS-Foundation.md "Attribute sets"). Single-player: no replication.
 * IncomingDamage / IncomingHeal are meta attributes: written by effects, moved into Health here, never persisted.
 * IncomingDamage arrives final (UDwDamageExecution already applied FDwDamageRules, including DamageReduction).
 */

/** Crew, enemies, Kraken body and tentacles. */
UCLASS()
class DEADWATERRUN_API UDwUnitAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Health") FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UDwUnitAttributeSet, Health)
	UPROPERTY(BlueprintReadOnly, Category = "Health") FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UDwUnitAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "Mana") FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS_BASIC(UDwUnitAttributeSet, Mana)
	UPROPERTY(BlueprintReadOnly, Category = "Mana") FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS_BASIC(UDwUnitAttributeSet, MaxMana)

	UPROPERTY(BlueprintReadOnly, Category = "Combat") FGameplayAttributeData AttackDamage;
	ATTRIBUTE_ACCESSORS_BASIC(UDwUnitAttributeSet, AttackDamage)
	/** Additive % damage dealt: 0.2 = +20%. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat") FGameplayAttributeData DamageBonus;
	ATTRIBUTE_ACCESSORS_BASIC(UDwUnitAttributeSet, DamageBonus)
	/** Additive % damage taken reduction, clamped 0..0.9. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat") FGameplayAttributeData DamageReduction;
	ATTRIBUTE_ACCESSORS_BASIC(UDwUnitAttributeSet, DamageReduction)

	UPROPERTY(BlueprintReadOnly, Category = "Turn") FGameplayAttributeData MoveRange;
	ATTRIBUTE_ACCESSORS_BASIC(UDwUnitAttributeSet, MoveRange)
	UPROPERTY(BlueprintReadOnly, Category = "Turn") FGameplayAttributeData MovePoints;
	ATTRIBUTE_ACCESSORS_BASIC(UDwUnitAttributeSet, MovePoints)
	UPROPERTY(BlueprintReadOnly, Category = "Turn") FGameplayAttributeData MaxActionPoints;
	ATTRIBUTE_ACCESSORS_BASIC(UDwUnitAttributeSet, MaxActionPoints)
	UPROPERTY(BlueprintReadOnly, Category = "Turn") FGameplayAttributeData ActionPoints;
	ATTRIBUTE_ACCESSORS_BASIC(UDwUnitAttributeSet, ActionPoints)

	UPROPERTY(BlueprintReadOnly, Category = "Fear") FGameplayAttributeData Fear;
	ATTRIBUTE_ACCESSORS_BASIC(UDwUnitAttributeSet, Fear)
	UPROPERTY(BlueprintReadOnly, Category = "Fear") FGameplayAttributeData MaxFear;
	ATTRIBUTE_ACCESSORS_BASIC(UDwUnitAttributeSet, MaxFear)

	UPROPERTY(BlueprintReadOnly, Category = "Meta") FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS_BASIC(UDwUnitAttributeSet, IncomingDamage)
	UPROPERTY(BlueprintReadOnly, Category = "Meta") FGameplayAttributeData IncomingHeal;
	ATTRIBUTE_ACCESSORS_BASIC(UDwUnitAttributeSet, IncomingHeal)

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

private:
	void Clamp(const FGameplayAttribute& Attribute, float& NewValue) const;
};

/** Ships: one damage pipeline with units, so cannons, repairs, upgrades and Kraken slams are all effects. */
UCLASS()
class DEADWATERRUN_API UDwShipAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Health") FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UDwShipAttributeSet, Health)
	UPROPERTY(BlueprintReadOnly, Category = "Health") FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UDwShipAttributeSet, MaxHealth)
	UPROPERTY(BlueprintReadOnly, Category = "Cannons") FGameplayAttributeData CannonDamage;
	ATTRIBUTE_ACCESSORS_BASIC(UDwShipAttributeSet, CannonDamage)
	UPROPERTY(BlueprintReadOnly, Category = "Meta") FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS_BASIC(UDwShipAttributeSet, IncomingDamage)

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
