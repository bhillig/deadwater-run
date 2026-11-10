// Copyright Brandon Hillig 2026

#include "Abilities/DwGameplayEffects.h"

#include "Abilities/DwAttributeSets.h"
#include "Abilities/DwDamageExecution.h"
#include "Abilities/DwTurnDurationComponent.h"
#include "Core/DwGameplayTags.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/RemoveOtherGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

using FUnit = UDwUnitAttributeSet;
using FShip = UDwShipAttributeSet;

// ---------------------------------------------------------------------------------------------------------------------
// Base helpers

void UDwGameplayEffect::PostInitProperties()
{
	Super::PostInitProperties();
	// Native classes never get PostLoad; rebuild the component tag caches the way a loaded asset would.
	if (HasAnyFlags(RF_ClassDefaultObject) && !GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint) && !HasAnyFlags(RF_NeedPostLoad))
	{
		OnGameplayEffectChanged();
	}
}

void UDwGameplayEffect::AddModifier(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op, const FGameplayEffectModifierMagnitude& Magnitude)
{
	FGameplayModifierInfo& Info = Modifiers.AddDefaulted_GetRef();
	Info.Attribute = Attribute;
	Info.ModifierOp = Op;
	Info.ModifierMagnitude = Magnitude;
}

void UDwGameplayEffect::AddSetByCaller(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op, const FGameplayTag& DataTag)
{
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = DataTag;
	AddModifier(Attribute, Op, FGameplayEffectModifierMagnitude(SetByCaller));
}

void UDwGameplayEffect::AddConstant(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op, float Value)
{
	AddModifier(Attribute, Op, FGameplayEffectModifierMagnitude(FScalableFloat(Value)));
}

void UDwGameplayEffect::AddFromTargetAttribute(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op, const FGameplayAttribute& BackingAttribute)
{
	FAttributeBasedFloat Based;
	Based.BackingAttribute = FGameplayEffectAttributeCaptureDefinition(BackingAttribute, EGameplayEffectAttributeCaptureSource::Target, /*bSnapshot*/ false);
	AddModifier(Attribute, Op, FGameplayEffectModifierMagnitude(Based));
}

void UDwGameplayEffect::AddGrantedTags(const FGameplayTagContainer& Tags)
{
	UTargetTagsGameplayEffectComponent* Component = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(Component);
	FInheritedTagContainer Container;
	Container.Added = Tags;
	Container.CombinedTags = Tags;
	Component->SetAndApplyTargetTagChanges(Container);
}

void UDwGameplayEffect::AddAssetTags(const FGameplayTagContainer& Tags)
{
	UAssetTagsGameplayEffectComponent* Component = CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("AssetTags"));
	GEComponents.Add(Component);
	FInheritedTagContainer Container;
	Container.Added = Tags;
	Container.CombinedTags = Tags;
	Component->SetAndApplyAssetTagChanges(Container);
}

void UDwGameplayEffect::AddTurnDuration(int32 Turns, EDwTurnTickPoint TickPoint)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	UDwTurnDurationComponent* Component = CreateDefaultSubobject<UDwTurnDurationComponent>(TEXT("TurnDuration"));
	Component->Turns = Turns;
	Component->TickPoint = TickPoint;
	GEComponents.Add(Component);
}

// ---------------------------------------------------------------------------------------------------------------------
// Init

UDwGE_InitUnitStats::UDwGE_InitUnitStats()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	const FGameplayAttribute Order[] = {
		FUnit::GetMaxHealthAttribute(), FUnit::GetHealthAttribute(), FUnit::GetMaxManaAttribute(), FUnit::GetManaAttribute(),
		FUnit::GetAttackDamageAttribute(), FUnit::GetDamageBonusAttribute(), FUnit::GetDamageReductionAttribute(),
		FUnit::GetMoveRangeAttribute(), FUnit::GetMovePointsAttribute(), FUnit::GetMaxActionPointsAttribute(), FUnit::GetActionPointsAttribute(),
		FUnit::GetMaxFearAttribute(), FUnit::GetFearAttribute(),
	};
	for (const FGameplayAttribute& Attribute : Order)
	{
		FSetByCallerFloat ByName;
		ByName.DataName = FName(Attribute.GetName());
		AddModifier(Attribute, EGameplayModOp::Override, FGameplayEffectModifierMagnitude(ByName));
	}
}

void UDwGE_InitUnitStats::SetValues(FGameplayEffectSpec& Spec, const FDwUnitInitValues& V)
{
	auto Set = [&Spec](const FGameplayAttribute& Attribute, float Value) { Spec.SetSetByCallerMagnitude(FName(Attribute.GetName()), Value); };
	Set(FUnit::GetMaxHealthAttribute(), V.MaxHealth);
	Set(FUnit::GetHealthAttribute(), V.Health);
	Set(FUnit::GetMaxManaAttribute(), V.MaxMana);
	Set(FUnit::GetManaAttribute(), V.Mana);
	Set(FUnit::GetAttackDamageAttribute(), V.AttackDamage);
	Set(FUnit::GetDamageBonusAttribute(), V.DamageBonus);
	Set(FUnit::GetDamageReductionAttribute(), V.DamageReduction);
	Set(FUnit::GetMoveRangeAttribute(), V.MoveRange);
	Set(FUnit::GetMovePointsAttribute(), V.MovePoints);
	Set(FUnit::GetMaxActionPointsAttribute(), V.MaxActionPoints);
	Set(FUnit::GetActionPointsAttribute(), V.ActionPoints);
	Set(FUnit::GetMaxFearAttribute(), V.MaxFear);
	Set(FUnit::GetFearAttribute(), V.Fear);
}

UDwGE_InitShipStats::UDwGE_InitShipStats()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	for (const FGameplayAttribute& Attribute : { FShip::GetMaxHealthAttribute(), FShip::GetHealthAttribute(), FShip::GetCannonDamageAttribute() })
	{
		FSetByCallerFloat ByName;
		ByName.DataName = FName(Attribute.GetName());
		AddModifier(Attribute, EGameplayModOp::Override, FGameplayEffectModifierMagnitude(ByName));
	}
}

void UDwGE_InitShipStats::SetValues(FGameplayEffectSpec& Spec, float MaxHealth, float Health, float CannonDamage)
{
	Spec.SetSetByCallerMagnitude(FName(FShip::GetMaxHealthAttribute().GetName()), MaxHealth);
	Spec.SetSetByCallerMagnitude(FName(FShip::GetHealthAttribute().GetName()), Health);
	Spec.SetSetByCallerMagnitude(FName(FShip::GetCannonDamageAttribute().GetName()), CannonDamage);
}

// ---------------------------------------------------------------------------------------------------------------------
// Core

UDwGE_Damage::UDwGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition& Execution = Executions.AddDefaulted_GetRef();
	Execution.CalculationClass = UDwDamageExecution::StaticClass();
}

UDwGE_Heal::UDwGE_Heal()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddSetByCaller(FUnit::GetIncomingHealAttribute(), EGameplayModOp::AddBase, DwTags::Data_Heal);
}

UDwGE_RestoreMana::UDwGE_RestoreMana()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddSetByCaller(FUnit::GetManaAttribute(), EGameplayModOp::AddBase, DwTags::Data_Value);
}

UDwGE_AddFear::UDwGE_AddFear()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddSetByCaller(FUnit::GetFearAttribute(), EGameplayModOp::AddBase, DwTags::Data_Fear);
}

UDwGE_Cleanse::UDwGE_Cleanse()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	URemoveOtherGameplayEffectComponent* Remove = CreateDefaultSubobject<URemoveOtherGameplayEffectComponent>(TEXT("RemoveDebuffs"));
	Remove->RemoveGameplayEffectQueries.Add(FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(FGameplayTagContainer(DwTags::Effect_Debuff)));
	GEComponents.Add(Remove);
}

// ---------------------------------------------------------------------------------------------------------------------
// Statuses (turn durations)

UDwGE_Cowering::UDwGE_Cowering()
{
	AddTurnDuration(1, EDwTurnTickPoint::OwnerSideTurnEnd);
	AddGrantedTags(FGameplayTagContainer(DwTags::Status_Incapacitated_Cowering));
	AddAssetTags(FGameplayTagContainer(DwTags::Effect_Debuff));
}

UDwGE_Frozen::UDwGE_Frozen()
{
	AddTurnDuration(1, EDwTurnTickPoint::OwnerSideTurnEnd);
	AddGrantedTags(FGameplayTagContainer(DwTags::Status_Incapacitated_Frozen));
	AddAssetTags(FGameplayTagContainer(DwTags::Effect_Debuff));
}

UDwGE_Rooted::UDwGE_Rooted()
{
	AddTurnDuration(1, EDwTurnTickPoint::OwnerSideTurnEnd);
	AddGrantedTags(FGameplayTagContainer(DwTags::Status_Rooted));
	AddAssetTags(FGameplayTagContainer(DwTags::Effect_Debuff));
}

UDwGE_Guarded::UDwGE_Guarded()
{
	// Protects through the enemy turn, ends when the guarded unit's side starts its next turn (K5 placeholder).
	AddTurnDuration(1, EDwTurnTickPoint::OwnerSideTurnStart);
	AddGrantedTags(FGameplayTagContainer(DwTags::Status_Guarded));
	AddAssetTags(FGameplayTagContainer(DwTags::Effect_Buff));
}

UDwGE_ConquerorExtraMove::UDwGE_ConquerorExtraMove()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddConstant(FUnit::GetMaxActionPointsAttribute(), EGameplayModOp::AddBase, 1.f);
}

UDwGE_DamageBonus::UDwGE_DamageBonus()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddSetByCaller(FUnit::GetDamageBonusAttribute(), EGameplayModOp::AddBase, DwTags::Data_DamageBonus);
	AddAssetTags(FGameplayTagContainer(DwTags::Effect_Buff));
}

UDwGE_Perk::UDwGE_Perk()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTags(FGameplayTagContainer(DwTags::Effect_Perk));
}

void UDwGE_Perk::AddValueModifier(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op)
{
	AddSetByCaller(Attribute, Op, DwTags::Data_Value);
}

// ---------------------------------------------------------------------------------------------------------------------
// Turn budget and costs

UDwGE_TurnBudgetRefresh::UDwGE_TurnBudgetRefresh()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddFromTargetAttribute(FUnit::GetActionPointsAttribute(), EGameplayModOp::Override, FUnit::GetMaxActionPointsAttribute());
	AddFromTargetAttribute(FUnit::GetMovePointsAttribute(), EGameplayModOp::Override, FUnit::GetMoveRangeAttribute());
}

UDwGE_CostAction::UDwGE_CostAction()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddConstant(FUnit::GetActionPointsAttribute(), EGameplayModOp::AddBase, -1.f);
}

UDwGE_CostMove::UDwGE_CostMove()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddSetByCaller(FUnit::GetMovePointsAttribute(), EGameplayModOp::AddBase, DwTags::Data_Cost_Move);
}

UDwGE_CostMana::UDwGE_CostMana()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddSetByCaller(FUnit::GetManaAttribute(), EGameplayModOp::AddBase, DwTags::Data_Cost_Mana);
}

UDwGE_SpendAll::UDwGE_SpendAll()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddConstant(FUnit::GetActionPointsAttribute(), EGameplayModOp::Override, 0.f);
	AddConstant(FUnit::GetMovePointsAttribute(), EGameplayModOp::Override, 0.f);
}

UDwGE_Kill::UDwGE_Kill()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddConstant(FUnit::GetIncomingDamageAttribute(), EGameplayModOp::AddBase, 1.0e9f);
}
