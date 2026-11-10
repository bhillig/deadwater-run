// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "DwAT_MoveAlongPath.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDwMoveAlongPathSignal);

/**
 * Walks the avatar along world points (Docs/Architecture/02 "Presentation tasks"). Presentation only: the grid move
 * already happened at commit. Ticks while walking; snaps and completes synchronously in instant mode.
 */
UCLASS()
class DEADWATERRUN_API UDwAT_MoveAlongPath : public UAbilityTask
{
	GENERATED_BODY()

public:
	UDwAT_MoveAlongPath();

	UPROPERTY(BlueprintAssignable) FDwMoveAlongPathSignal OnCompleted;

	/** Points in walking order (the start is the avatar's current location). Speed in units per second at rate 1. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Ability Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static UDwAT_MoveAlongPath* MoveAlongPath(UGameplayAbility* OwningAbility, const TArray<FVector>& Points, float Speed = 500.f);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void Finish();

	TArray<FVector> Points;
	float Speed = 500.f;
	int32 NextPoint = 0;
};
