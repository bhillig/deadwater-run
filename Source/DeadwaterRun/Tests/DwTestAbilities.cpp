// Copyright Brandon Hillig 2026

#include "Tests/DwTestAbilities.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Combat/DwCombatSubsystem.h"

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

UDwTestAbility_Broadside::UDwTestAbility_Broadside()
{
	Targeting.Pattern = EDwTargetPattern::EnemyShip;
}

UDwTestAbility_Latent::UDwTestAbility_Latent()
{
	ActionCost = EDwActionCost::Action;
	Targeting.Pattern = EDwTargetPattern::Self;
	Targeting.Side = EDwTargetSide::Ally;
}

void UDwTestAbility_Latent::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
	}
	// Otherwise stays active until the test cancels it (an animation that hasn't finished).
}

void UDwTestScriptedController::BeginSideTurn(UDwCombatSubsystem& Combat, EDwSide Side)
{
	++SideTurns;
	if (OnSideTurn)
	{
		OnSideTurn(Combat, Side);
	}
}

void UDwTestScriptedController::BeginUnitTurn(UDwCombatSubsystem& Combat, FDwUnitHandle Unit)
{
	++UnitTurns;
	if (OnUnitTurn)
	{
		OnUnitTurn(Combat, Unit);
	}
	if (bAutoEndUnit && !Combat.IsResolving() && Combat.GetSnapshot().ActingUnit == Unit)
	{
		Combat.SubmitCommand(FDwCombatCommand::MakeEndUnit(Unit));
	}
}

void UDwTestCombatListener::Bind(UDwCombatSubsystem& Combat)
{
	Combat.OnCombatEvent.AddDynamic(this, &ThisClass::HandleEvent);
	Combat.OnCommandResolved.AddDynamic(this, &ThisClass::HandleResolved);
	Combat.OnCombatEnded.AddDynamic(this, &ThisClass::HandleEnded);
	Combat.OnEndTurnSuggested.AddDynamic(this, &ThisClass::HandleEndTurnSuggested);
}

void UDwTestCombatListener::Reset()
{
	Events.Reset();
	Resolved.Reset();
	Ended.Reset();
	EndTurnSuggestions = 0;
}

int32 UDwTestCombatListener::Count(EDwCombatEventType Type, FDwUnitHandle Target) const
{
	return Events.FilterByPredicate([&](const FDwCombatEvent& E) { return E.Type == Type && (!Target.IsValid() || E.Target == Target); }).Num();
}
