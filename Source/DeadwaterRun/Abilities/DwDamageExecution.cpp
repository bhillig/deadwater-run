// Copyright Brandon Hillig 2026

#include "Abilities/DwDamageExecution.h"

#include "AbilitySystemComponent.h"
#include "Abilities/DwAttributeSets.h"
#include "Abilities/DwDamageRules.h"
#include "Core/DwGameplayTags.h"

namespace DwDamageCapture
{
	struct FDefs
	{
		FGameplayEffectAttributeCaptureDefinition AttackDamage;
		FGameplayEffectAttributeCaptureDefinition DamageBonus;
		FGameplayEffectAttributeCaptureDefinition DamageReduction;

		FDefs()
			: AttackDamage(UDwUnitAttributeSet::GetAttackDamageAttribute(), EGameplayEffectAttributeCaptureSource::Source, /*bSnapshot*/ true)
			, DamageBonus(UDwUnitAttributeSet::GetDamageBonusAttribute(), EGameplayEffectAttributeCaptureSource::Source, true)
			, DamageReduction(UDwUnitAttributeSet::GetDamageReductionAttribute(), EGameplayEffectAttributeCaptureSource::Target, false)
		{
		}
	};

	static const FDefs& Get()
	{
		static const FDefs Defs;
		return Defs;
	}
}

UDwDamageExecution::UDwDamageExecution()
{
	const DwDamageCapture::FDefs& Defs = DwDamageCapture::Get();
	RelevantAttributesToCapture.Add(Defs.AttackDamage);
	RelevantAttributesToCapture.Add(Defs.DamageBonus);
	RelevantAttributesToCapture.Add(Defs.DamageReduction);
}

void UDwDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const DwDamageCapture::FDefs& Defs = DwDamageCapture::Get();

	FAggregatorEvaluateParameters Params;
	Params.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Params.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	FDwDamageInput Input;
	Input.Flat = Spec.GetSetByCallerMagnitude(DwTags::Data_Damage, /*WarnIfNotFound*/ false, 0.f);
	Input.Scale = Spec.GetSetByCallerMagnitude(DwTags::Data_DamageScale, false, 0.f);
	Input.SituationalMultiplier = Spec.GetSetByCallerMagnitude(DwTags::Data_DamageMultiplier, false, 1.f);
	// Missing captures (a ship or the environment as the source) leave the defaults: 0 attack, 0 bonus.
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Defs.AttackDamage, Params, Input.AttackDamage);

	const UAbilitySystemComponent* Target = ExecutionParams.GetTargetAbilitySystemComponent();
	const bool bTargetIsUnit = Target && Target->GetSet<UDwUnitAttributeSet>() != nullptr;
	const bool bTargetIsShip = !bTargetIsUnit && Target && Target->GetSet<UDwShipAttributeSet>() != nullptr;
	// Unit-only terms: a ship takes flat ship damage (cannon = the firing ship's CannonDamage), never the gunner's bonus.
	if (bTargetIsUnit)
	{
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Defs.DamageReduction, Params, Input.DamageReduction);
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Defs.DamageBonus, Params, Input.DamageBonus);
	}

	const FDwDamageOutput Output = FDwDamageRules::Compute(Input);
	if (Output.Damage <= 0)
	{
		return;
	}
	if (bTargetIsUnit)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UDwUnitAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::AddBase, Output.Damage));
	}
	else if (bTargetIsShip)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UDwShipAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::AddBase, Output.Damage));
	}
}
