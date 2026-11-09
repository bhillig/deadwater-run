// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/DwGameplayAbility.h"
#include "DwTestAbilities.generated.h"

/**
 * Test-only ability: commits (costs), then applies the tuned shared GE_Damage to TriggerEventData.Target and ends.
 * Used by DeadwaterRun.Gas.* to check costs, budgets and "preview == result". Not content: hidden from pickers.
 * The real workhorse is UDwGA_EffectPayload (DW-104).
 */
UCLASS(NotBlueprintable, HideDropdown, meta = (HiddenNode))
class DEADWATERRUN_API UDwTestAbility_Strike : public UDwGameplayAbility
{
	GENERATED_BODY()

public:
	UDwTestAbility_Strike();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
