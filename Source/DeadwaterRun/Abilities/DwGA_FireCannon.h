// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/DwGA_EffectPayload.h"
#include "DwGA_FireCannon.generated.h"

/**
 * Ability.Cannon (Docs/Architecture/06-Ship-Cannons.md, C7): the unit stands on or 4-way next to an unused cannon of
 * its own ship, spends its action, and the enemy ship takes the firing ship's CannonDamage. Each cannon fires once per
 * side turn; crew can't use the other side's cannons; cannons never hit units (C8).
 */
UCLASS(Blueprintable)
class DEADWATERRUN_API UDwGA_FireCannon : public UDwGA_EffectPayload
{
	GENERATED_BODY()

public:
	UDwGA_FireCannon();

	/** Cue at the cannon that fired. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (Categories = "GameplayCue"))
	FGameplayTag FireCue;

	/** Nothing when no usable cannon is in reach (the action bar shows "Stand next to a cannon"). */
	virtual void GatherValidTargets(const FDwTargetingContext& Context, TArray<FDwAbilityTarget>& OutTargets) const override;
	virtual FDwAbilityPreview PreviewOutcome(const FDwTargetingContext& Context, const FDwAbilityTarget& Target) const override;

	/** Player-facing reason when GatherValidTargets is empty. */
	static FText GetNoCannonReason();

protected:
	virtual bool PrepareActivation(const FDwTargetingContext& Context, const FDwAbilityTarget& Aimed) override;
	virtual void OnCommitted() override;
	virtual void ApplyPayload() override;
	virtual void CustomizeSpec(FGameplayEffectSpec& Spec, AActor* Target) const override;

private:
	FIntPoint CannonTile = FIntPoint::ZeroValue;
	float CannonDamage = 0.f;
};
