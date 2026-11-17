// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/DwGA_EffectPayload.h"
#include "DwGA_Charge.generated.h"

/**
 * The Conqueror's Charge (Docs/Architecture/05, K12 a): dash in a straight 4-way line up to the tuning Range (6) and
 * strike the first enemy met (FDwTargetingQuery::FirstUnitAlongLine). The line stops at obstacles, water, the nest and
 * any unit; only a line that ends on an enemy is a valid charge. The captain ends on the tile before the enemy; the hit
 * is the payload's Applications (GE_Damage, numbers from DT_AbilityTuning). Aimed by the enemy it would hit.
 */
UCLASS(Blueprintable)
class DEADWATERRUN_API UDwGA_Charge : public UDwGA_EffectPayload
{
	GENERATED_BODY()

public:
	UDwGA_Charge();

	/** The enemies a charge from Context's source can reach, one per direction (as unit targets). */
	virtual void GatherValidTargets(const FDwTargetingContext& Context, TArray<FDwAbilityTarget>& OutTargets) const override;
	/** The aimed enemy, if a straight charge reaches it. */
	virtual void ResolveTargets(const FDwTargetingContext& Context, const FDwAbilityTarget& Target, TArray<FDwAbilityTarget>& OutTargets) const override;

	/** Where the captain stops when charging Target (the tile before it on the line); unset if no charge reaches it. */
	static TOptional<FIntPoint> GetStopTile(const FDwTargetingContext& Context, int32 Range, FDwUnitHandle Target);

protected:
	virtual bool PrepareActivation(const FDwTargetingContext& Context, const FDwAbilityTarget& Aimed) override;
	virtual void ApplyPayload() override;

private:
	/** Set at activation: where the dash ends. */
	TOptional<FIntPoint> StopTile;
};
