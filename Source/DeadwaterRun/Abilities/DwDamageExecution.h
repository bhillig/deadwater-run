// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "DwDamageExecution.generated.h"

/**
 * GE_Damage's execution (Docs/Architecture/02 "Damage pipeline"). Captures source AttackDamage and DamageBonus and target
 * DamageReduction, reads SetByCaller Data.Damage, Data.DamageScale and Data.DamageMultiplier (default 1), runs
 * FDwDamageRules::Compute and writes IncomingDamage. Ship targets (UDwShipAttributeSet, no unit set) take the same
 * formula without the unit-only DamageReduction term.
 */
UCLASS()
class DEADWATERRUN_API UDwDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UDwDamageExecution();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
