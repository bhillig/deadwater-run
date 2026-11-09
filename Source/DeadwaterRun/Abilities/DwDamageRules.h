// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"

/** Everything that decides one hit's damage. Docs/Architecture/02-GAS-Foundation.md "Damage pipeline". */
struct FDwDamageInput
{
	/** Flat damage from the ability's tuning (SetByCaller Data.Damage). Cannon shots put the ship's CannonDamage here. */
	float Flat = 0.f;
	/** Multiplier of the source's AttackDamage (SetByCaller Data.DamageScale). */
	float Scale = 0.f;
	/** Source AttackDamage attribute. */
	float AttackDamage = 0.f;
	/** Source additive % bonus (Conqueror kill bonus, perks): 0.2 = +20%. */
	float DamageBonus = 0.f;
	/** Situational multipliers passed in by the caller, e.g. Team Leader synergy (SetByCaller Data.DamageMultiplier). */
	float SituationalMultiplier = 1.f;
	/** Target additive % reduction, clamped to [0, MaxReduction]. Ships have none. */
	float DamageReduction = 0.f;
};

struct FDwDamageOutput
{
	int32 Damage = 0;
};

/**
 * The one damage formula. Pure, so UI previews, AI scoring and the damage execution all agree ("previews never lie"):
 *   round((Flat + Scale * AttackDamage) * (1 + DamageBonus) * SituationalMultiplier * (1 - clamp(DamageReduction))), never below 0.
 * Rounding is half away from zero on the final value only. Elevation gives range, never damage.
 */
struct DEADWATERRUN_API FDwDamageRules
{
	static constexpr float MaxReduction = 0.9f;

	static FDwDamageOutput Compute(const FDwDamageInput& Input);
};
