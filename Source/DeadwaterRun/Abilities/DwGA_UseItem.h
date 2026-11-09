// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/DwGA_EffectPayload.h"
#include "DwGA_UseItem.generated.h"

class UDwItemDefinition;

/**
 * Ability.Item (Docs/Architecture/02 "UDwGA_UseItem", 10 items): the command names an item id; the item's definition
 * decides targeting (its FDwTargetingRules + tuning Range) and effect (its GE with Data.Heal / Data.Value = Amount).
 * Spends the unit's action and one item from the fight's inventory view at commit.
 */
UCLASS(Blueprintable)
class DEADWATERRUN_API UDwGA_UseItem : public UDwGA_EffectPayload
{
	GENERATED_BODY()

public:
	UDwGA_UseItem();

	virtual const FDwTargetingRules& GetTargetingRules(const FDwTargetingContext& Context) const override;
	virtual void GatherValidTargets(const FDwTargetingContext& Context, TArray<FDwAbilityTarget>& OutTargets) const override;
	virtual FDwAbilityPreview PreviewOutcome(const FDwTargetingContext& Context, const FDwAbilityTarget& Target) const override;

	/** Amount from the item's tuning row (0 if it has none). */
	static int32 GetAmount(const UDwItemDefinition& Item);

protected:
	virtual int32 GetRange(const FDwTargetingContext& Context) const override;
	virtual bool PrepareActivation(const FDwTargetingContext& Context, const FDwAbilityTarget& Aimed) override;
	virtual void OnCommitted() override;
	virtual void ApplyPayload() override;

private:
	UPROPERTY() TObjectPtr<const UDwItemDefinition> PendingItem;
	FName PendingItemId;
};
