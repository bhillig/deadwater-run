// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/DwGameplayAbility.h"
#include "DwGA_Move.generated.h"

/**
 * Ability.Move (Docs/Architecture/02 "UDwGA_Move"): one path to a reachable tile, paying its cost in MovePoints.
 * One move per turn (C3): the rest of MovePoints is zeroed after the move, so move-act or act-move, never move-act-move.
 * The grid move happens at commit (logic is instant); UDwAT_MoveAlongPath animates it. Blocked by Status.Rooted.
 */
UCLASS(Blueprintable)
class DEADWATERRUN_API UDwGA_Move : public UDwGameplayAbility
{
	GENERATED_BODY()

public:
	UDwGA_Move();

	/** Walking speed of the presentation, units per second at playback rate 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "1"))
	float WalkSpeed = 450.f;

	/** Every tile reachable with the unit's MovePoints (enemies block, allies can be passed, never end on a unit). */
	virtual void GatherValidTargets(const FDwTargetingContext& Context, TArray<FDwAbilityTarget>& OutTargets) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	virtual float GetMoveCost(const FGameplayAbilityActorInfo* ActorInfo) const override { return PendingCost; }

private:
	UFUNCTION() void HandleArrived();

	float PendingCost = 0.f;
};
