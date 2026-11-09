// Copyright Brandon Hillig 2026

#include "Abilities/Tasks/DwAT_PlayPresentation.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "Presentation/DwPresentationSubsystem.h"

UDwAT_PlayPresentation::UDwAT_PlayPresentation()
{
	bTickingTask = true;
}

UDwAT_PlayPresentation* UDwAT_PlayPresentation::PlayPresentation(UGameplayAbility* OwningAbility, UAnimMontage* InMontage, FGameplayTag InImpactEvent, float InFallbackSeconds)
{
	UDwAT_PlayPresentation* Task = NewAbilityTask<UDwAT_PlayPresentation>(OwningAbility);
	Task->Montage = InMontage;
	Task->ImpactEvent = InImpactEvent;
	Task->FallbackSeconds = InFallbackSeconds;
	return Task;
}

void UDwAT_PlayPresentation::Activate()
{
	Super::Activate();
	UWorld* World = GetWorld();
	if (UDwPresentationSubsystem::IsInstant(this) || !World)
	{
		Impact();
		Complete();
		return;
	}
	const float Rate = UDwPresentationSubsystem::GetPlaybackRate(this);

	UAbilitySystemComponent* Asc = AbilitySystemComponent.Get();
	UAnimInstance* AnimInstance = Ability && Ability->GetCurrentActorInfo() ? Ability->GetCurrentActorInfo()->GetAnimInstance() : nullptr;
	if (Montage && Asc && AnimInstance)
	{
		if (ImpactEvent.IsValid())
		{
			ImpactEventHandle = Asc->GenericGameplayEventCallbacks.FindOrAdd(ImpactEvent).AddUObject(this, &ThisClass::HandleImpactEvent);
		}
		if (Asc->PlayMontage(Ability, Ability->GetCurrentActivationInfo(), Montage, Rate) > 0.f)
		{
			FOnMontageEnded Ended = FOnMontageEnded::CreateUObject(this, &ThisClass::HandleMontageEnded);
			AnimInstance->Montage_SetEndDelegate(Ended, Montage);
			return;
		}
	}

	// No montage yet (placeholder art): two ticked beats so the player can follow what happened.
	if (FallbackSeconds <= 0.f)
	{
		Impact();
		Complete();
		return;
	}
	Elapsed = 0.f; // TickTask runs the beat
}

void UDwAT_PlayPresentation::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);
	if (Elapsed < 0.f || bCompleted)
	{
		return;
	}
	Elapsed += DeltaTime * UDwPresentationSubsystem::GetPlaybackRate(this);
	if (Elapsed >= FallbackSeconds * 0.5f)
	{
		Impact();
	}
	if (Elapsed >= FallbackSeconds)
	{
		Complete();
	}
}

void UDwAT_PlayPresentation::HandleImpactEvent(const FGameplayEventData* /*Payload*/)
{
	Impact();
}

void UDwAT_PlayPresentation::HandleMontageEnded(UAnimMontage* /*EndedMontage*/, bool /*bInterrupted*/)
{
	Complete();
}

void UDwAT_PlayPresentation::Impact()
{
	if (bImpacted)
	{
		return;
	}
	bImpacted = true;
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnImpact.Broadcast();
	}
}

void UDwAT_PlayPresentation::Complete()
{
	if (bCompleted)
	{
		return;
	}
	Impact(); // a montage without the notify still lands its hit
	bCompleted = true;
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnCompleted.Broadcast();
	}
	EndTask();
}

void UDwAT_PlayPresentation::OnDestroy(bool bInOwnerFinished)
{
	if (UAbilitySystemComponent* Asc = AbilitySystemComponent.Get())
	{
		if (ImpactEventHandle.IsValid())
		{
			if (FGameplayEventMulticastDelegate* Delegate = Asc->GenericGameplayEventCallbacks.Find(ImpactEvent))
			{
				Delegate->Remove(ImpactEventHandle);
			}
		}
	}
	Super::OnDestroy(bInOwnerFinished);
}
