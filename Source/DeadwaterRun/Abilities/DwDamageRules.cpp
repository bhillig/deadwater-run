// Copyright Brandon Hillig 2026

#include "Abilities/DwDamageRules.h"

FDwDamageOutput FDwDamageRules::Compute(const FDwDamageInput& Input)
{
	const float Base = Input.Flat + Input.Scale * Input.AttackDamage;
	const float Bonus = FMath::Max(0.f, 1.f + Input.DamageBonus);
	const float Reduction = 1.f - FMath::Clamp(Input.DamageReduction, 0.f, MaxReduction);
	const float Raw = Base * Bonus * FMath::Max(Input.SituationalMultiplier, 0.f) * Reduction;

	FDwDamageOutput Output;
	Output.Damage = FMath::Max(0, static_cast<int32>(FMath::RoundHalfFromZero(Raw)));
	return Output;
}
