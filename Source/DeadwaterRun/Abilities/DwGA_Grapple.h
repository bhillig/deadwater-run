// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/DwGA_EffectPayload.h"
#include "Board/DwGridModel.h"
#include "DwGA_Grapple.generated.h"

/**
 * Team Leader's Grapple Hook (Docs/Architecture/05, K6 a): a unit of either side in sight and range is pulled to the
 * free tile next to the captain, at his height, closest to where it stood. Works across ships and pulls units out of
 * the sniper's nest (never into it). Not on a unit already next to the captain, with no free tile beside him, or with
 * Unit.Immune.Grapple (the Kraken, K10). Range and costs from DT_AbilityTuning; Applications (if any) land after the pull.
 */
UCLASS(Blueprintable)
class DEADWATERRUN_API UDwGA_Grapple : public UDwGA_EffectPayload
{
	GENERATED_BODY()

public:
	UDwGA_Grapple();

	/** Where Target would land when pulled by Source standing on SourceTile; unset if it can't be pulled. */
	static TOptional<FIntPoint> GetLandingTile(const FDwGridModel& Grid, FIntPoint SourceTile, FIntPoint TargetTile);

	virtual void GatherValidTargets(const FDwTargetingContext& Context, TArray<FDwAbilityTarget>& OutTargets) const override;

protected:
	virtual void ApplyPayload() override;
};
