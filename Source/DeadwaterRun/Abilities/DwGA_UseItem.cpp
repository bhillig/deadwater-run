// Copyright Brandon Hillig 2026

#include "Abilities/DwGA_UseItem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/DwAttributeSets.h"
#include "Combat/DwCombatSnapshot.h"
#include "Combat/DwCombatSubsystem.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwItemDefinition.h"
#include "Data/Tuning/DwTuningRows.h"

#define LOCTEXT_NAMESPACE "DwGA_UseItem"

namespace DwUseItem
{
	static const UDwItemDefinition* FindItem(const FDwTargetingContext& Context)
	{
		return Context.Snapshot && !Context.ItemId.IsNone() ? Context.Snapshot->FindItem(Context.ItemId) : nullptr;
	}

	/** What an item effect restores, read from its modifiers so new item GEs need no code. */
	static void Restores(TSubclassOf<UGameplayEffect> Effect, bool& bOutHealth, bool& bOutMana)
	{
		bOutHealth = bOutMana = false;
		const UGameplayEffect* CDO = Effect ? Effect->GetDefaultObject<UGameplayEffect>() : nullptr;
		for (const FGameplayModifierInfo& Modifier : CDO ? CDO->Modifiers : TArray<FGameplayModifierInfo>())
		{
			bOutHealth |= Modifier.Attribute == UDwUnitAttributeSet::GetIncomingHealAttribute() || Modifier.Attribute == UDwUnitAttributeSet::GetHealthAttribute();
			bOutMana |= Modifier.Attribute == UDwUnitAttributeSet::GetManaAttribute();
		}
	}
}

UDwGA_UseItem::UDwGA_UseItem()
{
	ActionCost = EDwActionCost::Action;
	// Only used when an item can't be found; every item brings its own rules.
	Targeting.Pattern = EDwTargetPattern::Self;
	Targeting.Side = EDwTargetSide::Ally;
	PlaceholderSeconds = 0.4f;
	HitCue = DwTags::GameplayCue_Unit_Heal;
}

int32 UDwGA_UseItem::GetAmount(const UDwItemDefinition& Item)
{
	const FDwItemTuningRow* Row = Item.GetTuningRow<FDwItemTuningRow>();
	return Row ? Row->Amount : 0;
}

const FDwTargetingRules& UDwGA_UseItem::GetTargetingRules(const FDwTargetingContext& Context) const
{
	const UDwItemDefinition* Item = DwUseItem::FindItem(Context);
	return Item ? Item->Targeting : Targeting;
}

int32 UDwGA_UseItem::GetRange(const FDwTargetingContext& Context) const
{
	const UDwItemDefinition* Item = DwUseItem::FindItem(Context);
	const FDwItemTuningRow* Row = Item ? Item->GetTuningRow<FDwItemTuningRow>() : nullptr;
	return Item && Item->Targeting.bIgnoreRange ? MAX_int32 / 2 : (Row ? Row->Range : 0);
}

void UDwGA_UseItem::GatherValidTargets(const FDwTargetingContext& Context, TArray<FDwAbilityTarget>& OutTargets) const
{
	OutTargets.Reset();
	const UDwItemDefinition* Item = DwUseItem::FindItem(Context);
	if (!Item || !Item->Effect || Context.Snapshot->Items.FindRef(Context.ItemId) <= 0)
	{
		return;
	}
	Super::GatherValidTargets(Context, OutTargets);
}

FDwAbilityPreview UDwGA_UseItem::PreviewOutcome(const FDwTargetingContext& Context, const FDwAbilityTarget& Target) const
{
	FDwAbilityPreview Preview;
	const UDwItemDefinition* Item = DwUseItem::FindItem(Context);
	if (!Item)
	{
		Preview.Reason = LOCTEXT("NoItem", "No item chosen");
		return Preview;
	}
	if (!IsValidTarget(Context, Target, &Preview.Reason))
	{
		if (Context.Snapshot->Items.FindRef(Context.ItemId) <= 0)
		{
			Preview.Reason = LOCTEXT("NoneLeft", "None left");
		}
		return Preview;
	}
	Preview.bValid = true;
	Preview.Reason = FText::GetEmpty();
	bool bHealth = false;
	bool bMana = false;
	DwUseItem::Restores(Item->Effect, bHealth, bMana);
	TArray<FDwAbilityTarget> Final;
	ResolveTargets(Context, Target, Final);
	for (const FDwAbilityTarget& Hit : Final)
	{
		FDwTargetPreview& Entry = Preview.Targets.AddDefaulted_GetRef();
		Entry.Target = Hit;
		const FDwUnitSnapshot* Unit = Context.Snapshot->FindUnit(Hit.Unit);
		// What it actually restores: capped by what is missing, so the preview matches the result.
		Entry.Heal = bHealth && Unit ? FMath::Min(GetAmount(*Item), FMath::RoundToInt(Unit->MaxHealth - Unit->Health)) : 0;
		Entry.Mana = bMana && Unit ? FMath::Min(GetAmount(*Item), FMath::RoundToInt(Unit->MaxMana - Unit->Mana)) : 0;
	}
	return Preview;
}

bool UDwGA_UseItem::PrepareActivation(const FDwTargetingContext& Context, const FDwAbilityTarget& /*Aimed*/)
{
	PendingItem = DwUseItem::FindItem(Context);
	PendingItemId = Context.ItemId;
	return PendingItem && PendingItem->Effect && Context.Snapshot->Items.FindRef(PendingItemId) > 0;
}

void UDwGA_UseItem::OnCommitted()
{
	GetCombat()->ConsumeItem(PendingItemId);
}

void UDwGA_UseItem::ApplyPayload()
{
	const UDwCombatSubsystem* Combat = GetCombat();
	UAbilitySystemComponent* Self = GetAbilitySystemComponentFromActorInfo();
	if (!Combat || !Self || !PendingItem)
	{
		return;
	}
	const float Amount = static_cast<float>(GetAmount(*PendingItem));
	for (const FDwAbilityTarget& Target : ResolvedTargets)
	{
		AActor* Actor = Combat->GetTargetActor(Target);
		UAbilitySystemComponent* Asc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
		if (!Asc || Asc->HasMatchingGameplayTag(DwTags::State_Dead))
		{
			continue;
		}
		FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(PendingItem->Effect, GetAbilityLevel());
		if (!Spec.IsValid())
		{
			continue;
		}
		Spec.Data->SetSetByCallerMagnitude(DwTags::Data_Heal, Amount);
		Spec.Data->SetSetByCallerMagnitude(DwTags::Data_Value, Amount);
		Self->ApplyGameplayEffectSpecToTarget(*Spec.Data, Asc);
		if (HitCue.IsValid())
		{
			FGameplayCueParameters Cue;
			Cue.Location = Actor->GetActorLocation();
			Asc->ExecuteGameplayCue(HitCue, Cue);
		}
	}
}

#undef LOCTEXT_NAMESPACE
