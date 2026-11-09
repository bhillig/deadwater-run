// Copyright Brandon Hillig 2026

#include "Tests/DwTestAbilities.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"

UDwTestAbility_Strike::UDwTestAbility_Strike()
{
	ActionCost = EDwActionCost::Action;
	Targeting.Pattern = EDwTargetPattern::SingleUnit;
	Targeting.Side = EDwTargetSide::Enemy;
}

void UDwTestAbility_Strike::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ false, /*bWasCancelled*/ true);
		return;
	}

	if (TriggerEventData && TriggerEventData->Target)
	{
		if (UAbilitySystemComponent* Target = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(TriggerEventData->Target.Get())))
		{
			FGameplayEffectSpecHandle Spec = MakeTunedSpec(DwGas::GetSharedEffects().Damage);
			if (Spec.IsValid())
			{
				// The caller's situational multiplier travels in EventMagnitude (0 = none).
				if (TriggerEventData->EventMagnitude > 0.f)
				{
					Spec.Data->SetSetByCallerMagnitude(DwTags::Data_DamageMultiplier, TriggerEventData->EventMagnitude);
				}
				GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*Spec.Data, Target);
			}
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}
