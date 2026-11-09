// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/DwGameplayAbility.h"
#include "DwGA_EffectPayload.generated.h"

class UAnimMontage;

/** One effect a payload ability applies, in order (Docs/Architecture/02 "UDwGA_EffectPayload"). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwEffectApplication
{
	GENERATED_BODY()

	/** Usually a shared GE (GE_Damage, GE_Heal, GE_AddFear, ...); magnitudes come from the ability's tuning row. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Payload")
	TSubclassOf<UGameplayEffect> Effect;

	/** Applied to a target only if it has all of these at that moment (Smoke and Fire: damage only the already afraid). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Payload")
	FGameplayTagContainer RequiredTargetTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Payload")
	FGameplayTagContainer BlockedTargetTags;

	/** Applied once to the user instead of to the targets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Payload")
	bool bApplyToSelf = false;
};

/**
 * The workhorse ability: resolve targets -> commit -> presentation -> at impact apply each FDwEffectApplication in
 * order -> end. Basic attacks, Rally Cry, Smoke and Fire, Six Pistols, Eye of the Tiger, defend and the Kraken moves are
 * GA_* Blueprint subclasses that only set Targeting, Applications and presentation (numbers come from DT_AbilityTuning).
 * Random patterns draw from the combat random stream when targets are resolved, before commit.
 */
UCLASS(Abstract, Blueprintable)
class DEADWATERRUN_API UDwGA_EffectPayload : public UDwGameplayAbility
{
	GENERATED_BODY()

public:
	UDwGA_EffectPayload();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Payload")
	TArray<FDwEffectApplication> Applications;

	/** Cue executed on each target hit (GameplayCue.Hit.Melee, ...). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (Categories = "GameplayCue"))
	FGameplayTag HitCue;

	/** Played on the user's mesh; its ImpactEvent notify is the moment effects land. Empty = timed placeholder beat. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UAnimMontage> Montage;

	/** The anim notify event that marks the impact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	FGameplayTag PresentationEvent;

	/** Length of the placeholder beat when there is no montage (seconds at playback rate 1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0"))
	float PlaceholderSeconds = 0.5f;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** Last checks before commit, with the live context (FireCannon picks its cannon). False cancels the activation. */
	virtual bool PrepareActivation(const FDwTargetingContext& Context, const FDwAbilityTarget& Aimed) { return true; }

	/** After costs are paid, before the presentation (FireCannon marks the cannon used, UseItem spends the item). */
	virtual void OnCommitted() {}

	/** Applies the payload to the resolved targets at impact. UseItem replaces it with the item's effect. */
	virtual void ApplyPayload();

	/** Last-moment spec changes per target (FireCannon: damage = the ship's CannonDamage). */
	virtual void CustomizeSpec(FGameplayEffectSpec& Spec, AActor* Target) const {}

	/** Applies EffectClass, tuned from this ability's row, to Target's ASC. */
	void ApplyTunedEffect(TSubclassOf<UGameplayEffect> EffectClass, UAbilitySystemComponent& Target, AActor* TargetActor);

	/** Targets resolved at activation, in hit order (repeats allowed for random patterns). */
	TArray<FDwAbilityTarget> ResolvedTargets;
	/** Situational damage multiplier passed by the combat subsystem (synergy, DW-207); 1 = none. */
	float DamageMultiplier = 1.f;

private:
	UFUNCTION() void HandleImpact();
	UFUNCTION() void HandleCompleted();
};
