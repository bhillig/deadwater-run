// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Core/DwTypes.h"
#include "DwGameplayEffects.generated.h"

/*
 * Shared GameplayEffects (Docs/Architecture/02-GAS-Foundation.md "Shared GameplayEffects"), as native classes:
 * reviewable in diffs, testable, and impossible to mis-click in the editor. Blueprint subclasses may add cues or
 * tweak tags; UDwGameData::Effects points at whichever class the game uses. Magnitudes come from SetByCallers filled
 * by UDwGameplayAbility::MakeTunedSpec, so no number lives in these classes except fixed rules (1 action, kill).
 * Cost effects take the NEGATIVE delta in their SetByCaller (e.g. Data.Cost.Mana = -10).
 */

/** Base with constructor-time helpers. Native GEs get no PostLoad, so tag caches are rebuilt in PostInitProperties. */
UCLASS(Abstract)
class DEADWATERRUN_API UDwGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	virtual void PostInitProperties() override;

protected:
	void AddModifier(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op, const FGameplayEffectModifierMagnitude& Magnitude);
	void AddSetByCaller(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op, const FGameplayTag& DataTag);
	void AddConstant(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op, float Value);
	/** Value = the target's own BackingAttribute (current value). */
	void AddFromTargetAttribute(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op, const FGameplayAttribute& BackingAttribute);
	void AddGrantedTags(const FGameplayTagContainer& Tags);
	void AddAssetTags(const FGameplayTagContainer& Tags);
	void AddTurnDuration(int32 Turns, EDwTurnTickPoint TickPoint);
};

/** Every unit attribute the spawn writes, in the order they're applied (maximums before currents, so clamps hold). */
struct DEADWATERRUN_API FDwUnitInitValues
{
	float MaxHealth = 0.f, Health = 0.f, MaxMana = 0.f, Mana = 0.f;
	float AttackDamage = 0.f, DamageBonus = 0.f, DamageReduction = 0.f;
	float MoveRange = 0.f, MovePoints = 0.f, MaxActionPoints = 0.f, ActionPoints = 0.f;
	float MaxFear = 0.f, Fear = 0.f;
};

/** Instant. Overrides every unit attribute from SetByCaller values named after the attribute (use SetValues). */
UCLASS()
class DEADWATERRUN_API UDwGE_InitUnitStats : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_InitUnitStats();
	static void SetValues(FGameplayEffectSpec& Spec, const FDwUnitInitValues& Values);
};

/** Instant. Overrides ship MaxHealth, Health, CannonDamage from SetByCaller values named after the attribute. */
UCLASS()
class DEADWATERRUN_API UDwGE_InitShipStats : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_InitShipStats();
	static void SetValues(FGameplayEffectSpec& Spec, float MaxHealth, float Health, float CannonDamage);
};

/** Instant. UDwDamageExecution (Data.Damage, Data.DamageScale, optional Data.DamageMultiplier). Units and ships. */
UCLASS()
class DEADWATERRUN_API UDwGE_Damage : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_Damage();
};

/** Instant. IncomingHeal += Data.Heal. */
UCLASS()
class DEADWATERRUN_API UDwGE_Heal : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_Heal();
};

/** Instant. Mana += Data.Value. */
UCLASS()
class DEADWATERRUN_API UDwGE_RestoreMana : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_RestoreMana();
};

/** Instant. Fear += Data.Fear (clamped to MaxFear). */
UCLASS()
class DEADWATERRUN_API UDwGE_AddFear : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_AddFear();
};

/** Instant. Removes every active effect with asset tag Effect.Debuff (Rally Cry, cleanse items/perks). */
UCLASS()
class DEADWATERRUN_API UDwGE_Cleanse : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_Cleanse();
};

/** 1 turn: grants Status.Incapacitated.Cowering (Fear at max). Debuff. */
UCLASS()
class DEADWATERRUN_API UDwGE_Cowering : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_Cowering();
};

/** 1 turn: grants Status.Incapacitated.Frozen (Eye of the Tiger). Debuff. */
UCLASS()
class DEADWATERRUN_API UDwGE_Frozen : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_Frozen();
};

/** 1 turn: grants Status.Rooted (Tentacle Grab). Debuff. */
UCLASS()
class DEADWATERRUN_API UDwGE_Rooted : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_Rooted();
};

/** Until the guarded unit's next side-turn start: grants Status.Guarded; the instigator is the guardian (K5). Buff. */
UCLASS()
class DEADWATERRUN_API UDwGE_Guarded : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_Guarded();
};

/** Infinite: MaxActionPoints + 1 (Conqueror, K1: a second action, still one move). */
UCLASS()
class DEADWATERRUN_API UDwGE_ConquerorExtraMove : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_ConquerorExtraMove();
};

/** Infinite: DamageBonus += Data.DamageBonus (Conqueror kill bonus, perks). */
UCLASS()
class DEADWATERRUN_API UDwGE_DamageBonus : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_DamageBonus();
};

/**
 * Parent of every perk effect (GE_Perk_*, 12-Progression.md): infinite, asset tag Effect.Perk. A perk Blueprint adds its
 * modifiers with SetByCaller Data.Value; the spawn fills Data.Value from the perk's DT_PerkTuning row.
 */
UCLASS(Abstract)
class DEADWATERRUN_API UDwGE_Perk : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_Perk();
	/** Content tools: add "Attribute (Op) Data.Value" to a perk Blueprint's defaults. */
	void AddValueModifier(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op);
};

/** Instant at side-turn start: ActionPoints = MaxActionPoints, MovePoints = MoveRange. */
UCLASS()
class DEADWATERRUN_API UDwGE_TurnBudgetRefresh : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_TurnBudgetRefresh();
};

/** Instant: ActionPoints - 1. */
UCLASS()
class DEADWATERRUN_API UDwGE_CostAction : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_CostAction();
};

/** Instant: MovePoints += Data.Cost.Move (negative). */
UCLASS()
class DEADWATERRUN_API UDwGE_CostMove : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_CostMove();
};

/** Instant: Mana += Data.Cost.Mana (negative). */
UCLASS()
class DEADWATERRUN_API UDwGE_CostMana : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_CostMana();
};

/** Instant: ActionPoints = 0 and MovePoints = 0 (Conqueror crew single-choice rule). */
UCLASS()
class DEADWATERRUN_API UDwGE_SpendAll : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_SpendAll();
};

/** Instant: lethal IncomingDamage with no kill credit (crew aboard a sinking ship). */
UCLASS()
class DEADWATERRUN_API UDwGE_Kill : public UDwGameplayEffect
{
	GENERATED_BODY()
public:
	UDwGE_Kill();
};
