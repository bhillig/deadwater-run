// Copyright Brandon Hillig 2026

#include "Abilities/DwGA_FireCannon.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/DwGameplayEffects.h"
#include "Board/DwBoard.h"
#include "Combat/DwCombatRules.h"
#include "Combat/DwCombatSnapshot.h"
#include "Combat/DwCombatSubsystem.h"
#include "Combat/DwCombatUnit.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"

#define LOCTEXT_NAMESPACE "DwGA_FireCannon"

UDwGA_FireCannon::UDwGA_FireCannon()
{
	ActionCost = EDwActionCost::Action;
	Targeting.Pattern = EDwTargetPattern::EnemyShip;
	Targeting.Side = EDwTargetSide::Enemy;
	Targeting.bIgnoreRange = true;
	HitCue = DwTags::GameplayCue_Cannon_Impact;
	FireCue = DwTags::GameplayCue_Cannon_Fire;
}

FText UDwGA_FireCannon::GetNoCannonReason()
{
	return LOCTEXT("NoCannon", "Stand on or next to one of your loaded cannons");
}

void UDwGA_FireCannon::GatherValidTargets(const FDwTargetingContext& Context, TArray<FDwAbilityTarget>& OutTargets) const
{
	OutTargets.Reset();
	const FDwUnitSnapshot* Unit = Context.Snapshot ? Context.Snapshot->FindUnit(Context.Source) : nullptr;
	const TOptional<FIntPoint> From = Context.GetSourceTile();
	if (!Unit || !From.IsSet()
		|| !FDwCannonRules::FindUsableCannon(Context.Snapshot->Grid, From.GetValue(), Unit->Side, Context.Snapshot->CannonsUsed).IsSet())
	{
		return;
	}
	Super::GatherValidTargets(Context, OutTargets);
}

FDwAbilityPreview UDwGA_FireCannon::PreviewOutcome(const FDwTargetingContext& Context, const FDwAbilityTarget& Target) const
{
	FDwAbilityPreview Preview = Super::PreviewOutcome(Context, Target);
	if (!Preview.bValid)
	{
		const FDwUnitSnapshot* Unit = Context.Snapshot ? Context.Snapshot->FindUnit(Context.Source) : nullptr;
		if (Unit && Context.GetSourceTile().IsSet()
			&& !FDwCannonRules::FindUsableCannon(Context.Snapshot->Grid, Context.GetSourceTile().GetValue(), Unit->Side, Context.Snapshot->CannonsUsed).IsSet())
		{
			Preview.Reason = GetNoCannonReason();
		}
		return Preview;
	}
	// Ships take the firing ship's CannonDamage, unmodified by unit stats.
	const FDwUnitSnapshot* Unit = Context.Snapshot->FindUnit(Context.Source);
	const FDwShipSnapshot* Own = Unit ? Context.Snapshot->FindShip(Unit->Side) : nullptr;
	for (FDwTargetPreview& Entry : Preview.Targets)
	{
		const FDwShipSnapshot* Victim = Entry.Target.bShip ? Context.Snapshot->FindShip(Entry.Target.ShipSide) : nullptr;
		Entry.Damage = Own ? FMath::RoundToInt(Own->CannonDamage) : 0;
		Entry.bLethal = Victim && Entry.Damage >= Victim->Health;
	}
	return Preview;
}

bool UDwGA_FireCannon::PrepareActivation(const FDwTargetingContext& Context, const FDwAbilityTarget& Aimed)
{
	const FDwUnitSnapshot* Unit = Context.Snapshot->FindUnit(Context.Source);
	const TOptional<FIntPoint> Cannon = Unit
		? FDwCannonRules::FindUsableCannon(Context.Snapshot->Grid, Unit->Tile, Unit->Side, Context.Snapshot->CannonsUsed) : TOptional<FIntPoint>();
	const FDwShipSnapshot* Own = Unit ? Context.Snapshot->FindShip(Unit->Side) : nullptr;
	if (!Cannon.IsSet() || !Own)
	{
		return false;
	}
	CannonTile = Cannon.GetValue();
	CannonDamage = Own->CannonDamage;
	return true;
}

void UDwGA_FireCannon::OnCommitted()
{
	UDwCombatSubsystem* Combat = GetCombat();
	Combat->MarkCannonUsed(CannonTile);
	if (FireCue.IsValid())
	{
		FGameplayCueParameters Cue;
		Cue.Location = Combat->GetBoard() ? Combat->GetBoard()->TileToWorld(CannonTile) : FVector::ZeroVector;
		Cue.Instigator = GetAvatarActorFromActorInfo();
		GetAbilitySystemComponentFromActorInfo()->ExecuteGameplayCue(FireCue, Cue);
	}
}

void UDwGA_FireCannon::ApplyPayload()
{
	// The shot itself always uses the shared GE_Damage; Applications can add extras (e.g. a burning-deck upgrade).
	const UDwCombatSubsystem* Combat = GetCombat();
	for (const FDwAbilityTarget& Target : ResolvedTargets)
	{
		AActor* Ship = Combat ? Combat->GetTargetActor(Target) : nullptr;
		if (UAbilitySystemComponent* Asc = Ship ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Ship) : nullptr)
		{
			ApplyTunedEffect(DwGas::GetSharedEffects().Damage, *Asc, Ship);
		}
	}
	Super::ApplyPayload();
}

void UDwGA_FireCannon::CustomizeSpec(FGameplayEffectSpec& Spec, AActor* /*Target*/) const
{
	Spec.SetSetByCallerMagnitude(DwTags::Data_Damage, CannonDamage);
	Spec.SetSetByCallerMagnitude(DwTags::Data_DamageScale, 0.f);
}

#undef LOCTEXT_NAMESPACE
