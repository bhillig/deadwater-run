// Copyright Brandon Hillig 2026

#include "Abilities/DwGA_EffectPayload.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/DwAT_PlayPresentation.h"
#include "Combat/DwCombatSubsystem.h"
#include "Combat/DwCombatUnit.h"
#include "Data/DwAbilityDefinition.h"
#include "Core/DwGameplayTags.h"

UDwGA_EffectPayload::UDwGA_EffectPayload()
{
	PresentationEvent = DwTags::Event_Presentation_Impact;
}

void UDwGA_EffectPayload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ResolvedTargets.Reset();
	const FDwGameplayAbilityTargetData_Grid* Grid = GetGridTarget(TriggerEventData);
	UDwCombatSubsystem* Combat = GetCombat();
	const ADwCombatUnit* Unit = GetOwnerUnit();
	if (!Grid || !Combat || !Unit)
	{
		CancelActivation();
		return;
	}
	DamageMultiplier = TriggerEventData->EventMagnitude > 0.f ? TriggerEventData->EventMagnitude : 1.f;

	// Resolve against the fight as it is now: redirects (defend) and random picks (combat stream) happen here, once.
	const FDwCombatSnapshot Snapshot = Combat->GetSnapshot();
	FDwTargetingContext Context;
	Context.Snapshot = &Snapshot;
	Context.Ability = GetDefinition();
	Context.Source = Unit->GetHandle();
	Context.Rng = &Combat->GetRandom();
	Context.ItemId = Grid->ItemId;
	Context.DamageMultiplier = DamageMultiplier;
	const FDwAbilityTarget Aimed = Grid->ToTarget();
	ResolveTargets(Context, Aimed, ResolvedTargets);
	// Each hit's damage multiplier from the same snapshot the preview used (synergy, guard): the preview is the result.
	HitMultipliers.Reset(ResolvedTargets.Num());
	for (const FDwAbilityTarget& Hit : ResolvedTargets)
	{
		HitMultipliers.Add(GetHitDamageMultiplier(Context, Hit));
	}
	// A tile aim at a tile nobody stands on still lands (and spends the action): a miss the player earned by moving.
	const FDwTargetingRules& Rules = GetTargetingRules(Context);
	MissedTile.Reset();
	if (ResolvedTargets.IsEmpty() && Rules.bAimAtTile && !Aimed.IsUnit() && !Aimed.bShip)
	{
		MissedTile = Aimed.Tile;
	}
	if ((ResolvedTargets.IsEmpty() && !MissedTile.IsSet()) || !PrepareActivation(Context, Aimed))
	{
		CancelActivation();
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		CancelActivation();
		return;
	}
	OnCommitted();

	UDwAT_PlayPresentation* Task = UDwAT_PlayPresentation::PlayPresentation(this, Montage, PresentationEvent, PlaceholderSeconds);
	Task->OnImpact.AddDynamic(this, &ThisClass::HandleImpact);
	Task->OnCompleted.AddDynamic(this, &ThisClass::HandleCompleted);
	Task->ReadyForActivation(); // instant mode: impact and completion happen inside this call
}

void UDwGA_EffectPayload::HandleImpact()
{
	ApplyPayload();
	if (MissedTile.IsSet())
	{
		const ADwCombatUnit* Unit = GetOwnerUnit();
		const UDwAbilityDefinition* Definition = GetDefinition();
		if (UDwCombatSubsystem* Combat = GetCombat(); Combat && Unit)
		{
			Combat->ReportMiss(Unit->GetHandle(), MissedTile.GetValue(), Definition ? Definition->AbilityTag : FGameplayTag());
		}
	}
}

void UDwGA_EffectPayload::HandleCompleted()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), false, false);
}

void UDwGA_EffectPayload::ApplyPayload()
{
	const UDwCombatSubsystem* Combat = GetCombat();
	UAbilitySystemComponent* Self = GetAbilitySystemComponentFromActorInfo();
	if (!Combat || !Self)
	{
		return;
	}
	for (const FDwEffectApplication& Application : Applications)
	{
		if (!Application.Effect)
		{
			continue;
		}
		if (Application.bApplyToSelf)
		{
			ApplyTunedEffect(Application.Effect, *Self, GetAvatarActorFromActorInfo(), DamageMultiplier);
			continue;
		}
		for (int32 Index = 0; Index < ResolvedTargets.Num(); ++Index)
		{
			const FDwAbilityTarget& Target = ResolvedTargets[Index];
			AActor* Actor = Combat->GetTargetActor(Target);
			UAbilitySystemComponent* Asc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
			// The target's tags right now: an earlier application of this payload may have changed them.
			if (!Asc || Asc->HasMatchingGameplayTag(DwTags::State_Dead)
				|| !Asc->HasAllMatchingGameplayTags(Application.RequiredTargetTags) || Asc->HasAnyMatchingGameplayTags(Application.BlockedTargetTags))
			{
				continue;
			}
			ApplyTunedEffect(Application.Effect, *Asc, Actor, HitMultipliers.IsValidIndex(Index) ? HitMultipliers[Index] : DamageMultiplier);
		}
	}
	if (HitCue.IsValid())
	{
		for (const FDwAbilityTarget& Target : ResolvedTargets)
		{
			AActor* Actor = Combat->GetTargetActor(Target);
			if (UAbilitySystemComponent* Asc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor))
			{
				FGameplayCueParameters Cue;
				Cue.Location = Actor->GetActorLocation();
				Cue.Instigator = GetAvatarActorFromActorInfo();
				Asc->ExecuteGameplayCue(HitCue, Cue);
			}
		}
	}
}

void UDwGA_EffectPayload::ApplyTunedEffect(TSubclassOf<UGameplayEffect> EffectClass, UAbilitySystemComponent& Target, AActor* TargetActor,
	float Multiplier)
{
	FGameplayEffectSpecHandle Spec = MakeTunedSpec(EffectClass);
	if (!Spec.IsValid())
	{
		return;
	}
	if (!FMath::IsNearlyEqual(Multiplier, 1.f))
	{
		Spec.Data->SetSetByCallerMagnitude(DwTags::Data_DamageMultiplier, Multiplier);
	}
	CustomizeSpec(*Spec.Data, TargetActor);
	GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*Spec.Data, &Target);
}
