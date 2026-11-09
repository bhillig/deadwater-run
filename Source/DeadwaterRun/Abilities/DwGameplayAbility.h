// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/DwTargeting.h"
#include "Abilities/DwTargetingRules.h"
#include "DwGameplayAbility.generated.h"

class UDwAbilityDefinition;
struct FDwAbilityTuningRow;
struct FDwSharedEffects;
struct FDwUnitSnapshot;

/** Which per-turn budget an ability spends (Docs/Architecture/02 D3). */
UENUM(BlueprintType)
enum class EDwActionCost : uint8
{
	Action, // 1 ActionPoint
	Move,   // the path cost in MovePoints (UDwGA_Move)
	Free
};

/** Why an ability can't be paid for right now; drives disabled-button reasons in the UI. */
UENUM(BlueprintType)
enum class EDwCostCheck : uint8
{
	Ok,
	NotEnoughMana,
	NoActionLeft,
	NoMoveLeft
};

/**
 * Base of every Deadwater ability (Docs/Architecture/02-GAS-Foundation.md "Ability classes").
 * - The UDwAbilityDefinition is the spec's SourceObject; numbers come from its DT_AbilityTuning row.
 * - Costs: mana from tuning + Action/Move points; Rule.Turn.SingleChoice spends the other budget too.
 * - GatherValidTargets / PreviewOutcome / ResolveTargets are const and run on the CDO against an FDwTargetingContext,
 *   so the UI, the AI and the simulator use exactly the rules activation uses.
 * - Activated by Event.Ability.Activate with FDwGameplayAbilityTargetData_Grid; never reads input.
 */
UCLASS(Abstract)
class DEADWATERRUN_API UDwGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UDwGameplayAbility();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater")
	EDwActionCost ActionCost = EDwActionCost::Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater")
	FDwTargetingRules Targeting;

	/** Definition of the granted spec (SourceObject). Only valid on an instance with a current spec. */
	const UDwAbilityDefinition* GetDefinition() const;

	/** Tuning row of a definition, or nullptr. */
	static const FDwAbilityTuningRow* GetTuning(const UDwAbilityDefinition* Definition);

	/** Pure budget check shared by CheckCost and the UI. */
	EDwCostCheck CheckBudget(const UDwAbilityDefinition* Definition, float Mana, float ActionPoints, float MovePoints) const;

	/** Every target this ability may be aimed at from the context's source (tile). */
	virtual void GatherValidTargets(const FDwTargetingContext& Context, TArray<FDwAbilityTarget>& OutTargets) const;

	/** Whether Target is among GatherValidTargets, with a reason when it isn't. */
	bool IsValidTarget(const FDwTargetingContext& Context, const FDwAbilityTarget& Target, FText* OutReason = nullptr) const;

	/** What happens if aimed at Target: final targets after redirects, damage/heal/fear each would take. */
	virtual FDwAbilityPreview PreviewOutcome(const FDwTargetingContext& Context, const FDwAbilityTarget& Target) const;

	/**
	 * The targets actually hit when aimed at Target: group patterns expand to every valid target, RandomEnemies draws
	 * MinHits..MaxHits from Context.Rng (repeats allowed, K11 placeholder), and a single-target hit on a Status.Guarded
	 * unit goes to its guardian if the guardian is alive and 4-way adjacent (defend, K5 placeholder).
	 */
	virtual void ResolveTargets(const FDwTargetingContext& Context, const FDwAbilityTarget& Target, TArray<FDwAbilityTarget>& OutTargets) const;

	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	/** MovePoints this activation spends when ActionCost is Move (UDwGA_Move returns the path cost). */
	virtual float GetMoveCost(const FGameplayAbilityActorInfo* ActorInfo) const { return 0.f; }

	/** A spec of EffectClass with every standard SetByCaller filled from the tuning row (Damage, DamageScale, Heal, Fear, Turns). */
	FGameplayEffectSpecHandle MakeTunedSpec(TSubclassOf<UGameplayEffect> EffectClass) const;

	/** Fills the standard SetByCallers of an existing spec from a tuning row. */
	static void ApplyTuningToSpec(FGameplayEffectSpec& Spec, const FDwAbilityTuningRow& Tuning);

private:
	bool IsUnitTargetable(const FDwTargetingContext& Context, const FDwUnitSnapshot& Source, FIntPoint SourceTile,
		const FDwUnitSnapshot& Candidate, int32 Range, FText* OutReason) const;
	int32 GetRange(const FDwTargetingContext& Context) const;
};

/** The shared effect classes from DA_GameData, with native defaults for any empty slot. */
namespace DwGas
{
	DEADWATERRUN_API const FDwSharedEffects& GetSharedEffects();
}
