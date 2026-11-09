// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayTagContainer.h"
#include "DwAT_PlayPresentation.generated.h"

class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDwPresentationSignal);

/**
 * Plays an ability's presentation and reports the impact moment (Docs/Architecture/02 "Presentation tasks").
 * - Montage on the avatar's mesh: OnImpact on the ImpactEvent anim notify (or at blend-out if the notify is missing),
 *   OnCompleted when the montage ends.
 * - No montage (placeholder art): OnImpact halfway through FallbackSeconds, OnCompleted at the end, so actions still
 *   read as separate beats. Ticks with the ability system (pauses with the game), scaled by the playback rate.
 * - Instant mode: OnImpact then OnCompleted synchronously in Activate.
 * OnImpact always fires exactly once before OnCompleted.
 */
UCLASS()
class DEADWATERRUN_API UDwAT_PlayPresentation : public UAbilityTask
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable) FDwPresentationSignal OnImpact;
	UPROPERTY(BlueprintAssignable) FDwPresentationSignal OnCompleted;

	UFUNCTION(BlueprintCallable, Category = "Deadwater|Ability Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static UDwAT_PlayPresentation* PlayPresentation(UGameplayAbility* OwningAbility, UAnimMontage* Montage, FGameplayTag ImpactEvent, float FallbackSeconds = 0.4f);

	UDwAT_PlayPresentation();

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void Impact();
	void Complete();
	void HandleImpactEvent(const struct FGameplayEventData* Payload);
	void HandleMontageEnded(UAnimMontage* EndedMontage, bool bInterrupted);

	UPROPERTY() TObjectPtr<UAnimMontage> Montage;
	FGameplayTag ImpactEvent;
	float FallbackSeconds = 0.4f;
	bool bImpacted = false;
	bool bCompleted = false;
	FDelegateHandle ImpactEventHandle;
	/** Placeholder beat: seconds elapsed (scaled by playback rate), or < 0 while a montage drives the task. */
	float Elapsed = -1.f;
};
