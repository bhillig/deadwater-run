// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DwGameplayAbility.generated.h"

/**
 * Base of every Deadwater ability. Declared in DW-004 so UDwAbilityDefinition::AbilityClass is typed.
 * DW-101 adds costs/budget, targeting, GatherValidTargets, PreviewOutcome, ResolveTargets and MakeTunedSpec
 * (Docs/Architecture/02-GAS-Foundation.md "Ability classes").
 */
UCLASS(Abstract)
class DEADWATERRUN_API UDwGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
};
