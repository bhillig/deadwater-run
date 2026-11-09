// Copyright Brandon Hillig 2026

#include "Abilities/DwGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/DwAttributeSets.h"
#include "Abilities/DwDamageRules.h"
#include "Abilities/DwGameplayEffects.h"
#include "Board/DwGridQueries.h"
#include "Combat/DwCombatSnapshot.h"
#include "Combat/DwCombatSubsystem.h"
#include "Combat/DwCombatUnit.h"
#include "Engine/World.h"
#include "Core/DwAssetManager.h"
#include "Engine/Engine.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/Tuning/DwTuningRows.h"

#define LOCTEXT_NAMESPACE "DwGameplayAbility"

// ---------------------------------------------------------------------------------------------------------------------
// Shared effects

const FDwSharedEffects& DwGas::GetSharedEffects()
{
	static FDwSharedEffects Effects;
	FDwSharedEffects Configured;
	if (GEngine && GEngine->AssetManager)
	{
		if (const UDwGameData* GameData = UDwAssetManager::Get().GetGameData())
		{
			Configured = GameData->Effects;
		}
	}
	auto Pick = [](TSubclassOf<UGameplayEffect> FromData, UClass* Native) { return FromData ? FromData : TSubclassOf<UGameplayEffect>(Native); };
	Effects.InitUnitStats = Pick(Configured.InitUnitStats, UDwGE_InitUnitStats::StaticClass());
	Effects.InitShipStats = Pick(Configured.InitShipStats, UDwGE_InitShipStats::StaticClass());
	Effects.Damage = Pick(Configured.Damage, UDwGE_Damage::StaticClass());
	Effects.Heal = Pick(Configured.Heal, UDwGE_Heal::StaticClass());
	Effects.RestoreMana = Pick(Configured.RestoreMana, UDwGE_RestoreMana::StaticClass());
	Effects.AddFear = Pick(Configured.AddFear, UDwGE_AddFear::StaticClass());
	Effects.Cleanse = Pick(Configured.Cleanse, UDwGE_Cleanse::StaticClass());
	Effects.Cowering = Pick(Configured.Cowering, UDwGE_Cowering::StaticClass());
	Effects.Frozen = Pick(Configured.Frozen, UDwGE_Frozen::StaticClass());
	Effects.Rooted = Pick(Configured.Rooted, UDwGE_Rooted::StaticClass());
	Effects.Guarded = Pick(Configured.Guarded, UDwGE_Guarded::StaticClass());
	Effects.ConquerorExtraMove = Pick(Configured.ConquerorExtraMove, UDwGE_ConquerorExtraMove::StaticClass());
	Effects.DamageBonus = Pick(Configured.DamageBonus, UDwGE_DamageBonus::StaticClass());
	Effects.TurnBudgetRefresh = Pick(Configured.TurnBudgetRefresh, UDwGE_TurnBudgetRefresh::StaticClass());
	Effects.CostAction = Pick(Configured.CostAction, UDwGE_CostAction::StaticClass());
	Effects.CostMove = Pick(Configured.CostMove, UDwGE_CostMove::StaticClass());
	Effects.CostMana = Pick(Configured.CostMana, UDwGE_CostMana::StaticClass());
	Effects.SpendAll = Pick(Configured.SpendAll, UDwGE_SpendAll::StaticClass());
	Effects.Kill = Pick(Configured.Kill, UDwGE_Kill::StaticClass());
	return Effects;
}

// ---------------------------------------------------------------------------------------------------------------------
// Targeting types

TOptional<FIntPoint> FDwTargetingContext::GetSourceTile() const
{
	if (SourceTileOverride.IsSet())
	{
		return SourceTileOverride;
	}
	const FDwUnitSnapshot* Unit = Snapshot ? Snapshot->FindUnit(Source) : nullptr;
	return Unit ? TOptional<FIntPoint>(Unit->Tile) : TOptional<FIntPoint>();
}

FDwAbilityTarget FDwGameplayAbilityTargetData_Grid::ToTarget() const
{
	FDwAbilityTarget Target;
	Target.Unit = Unit;
	Target.Tile = Tile;
	Target.bShip = bShip;
	Target.ShipSide = ShipSide;
	return Target;
}

FGameplayAbilityTargetDataHandle FDwGameplayAbilityTargetData_Grid::MakeHandle(const FDwAbilityTarget& Target, FName InItemId)
{
	FDwGameplayAbilityTargetData_Grid* Data = new FDwGameplayAbilityTargetData_Grid();
	Data->Tile = Target.Tile;
	Data->Unit = Target.Unit;
	Data->bShip = Target.bShip;
	Data->ShipSide = Target.ShipSide;
	Data->ItemId = InItemId;
	return FGameplayAbilityTargetDataHandle(Data); // the handle owns the data
}

FString FDwGameplayAbilityTargetData_Grid::ToString() const
{
	return FString::Printf(TEXT("Grid(Tile=%d,%d Unit=%d Ship=%d Item=%s)"), Tile.X, Tile.Y, Unit.Id, bShip ? 1 : 0, *ItemId.ToString());
}

bool FDwGameplayAbilityTargetData_Grid::NetSerialize(FArchive& Ar, UPackageMap* /*Map*/, bool& bOutSuccess)
{
	uint8 Side = static_cast<uint8>(ShipSide);
	Ar << Tile << Unit.Id << ItemId << bShip << Side;
	ShipSide = static_cast<EDwSide>(Side);
	bOutSuccess = true;
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Ability base

UDwGameplayAbility::UDwGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	ActivationBlockedTags.AddTag(DwTags::Status_Incapacitated);
	ActivationBlockedTags.AddTag(DwTags::State_Dead);
}

const UDwAbilityDefinition* UDwGameplayAbility::GetDefinition() const
{
	return Cast<UDwAbilityDefinition>(GetCurrentSourceObject());
}

const FDwAbilityTuningRow* UDwGameplayAbility::GetTuning(const UDwAbilityDefinition* Definition)
{
	return Definition ? Definition->GetTuningRow<FDwAbilityTuningRow>() : nullptr;
}

EDwCostCheck UDwGameplayAbility::CheckBudget(const UDwAbilityDefinition* Definition, float Mana, float ActionPoints, float MovePoints) const
{
	const FDwAbilityTuningRow* Tuning = GetTuning(Definition);
	if (Tuning && Tuning->ManaCost > Mana)
	{
		return EDwCostCheck::NotEnoughMana;
	}
	if (ActionCost == EDwActionCost::Action && ActionPoints < 1.f)
	{
		return EDwCostCheck::NoActionLeft;
	}
	if (ActionCost == EDwActionCost::Move && MovePoints <= 0.f)
	{
		return EDwCostCheck::NoMoveLeft;
	}
	return EDwCostCheck::Ok;
}

bool UDwGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags) || !ActorInfo)
	{
		return false;
	}
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		return false;
	}
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	const UDwAbilityDefinition* Definition = Spec ? Cast<UDwAbilityDefinition>(Spec->SourceObject.Get()) : nullptr;
	return CheckBudget(Definition,
		ASC->GetNumericAttribute(UDwUnitAttributeSet::GetManaAttribute()),
		ASC->GetNumericAttribute(UDwUnitAttributeSet::GetActionPointsAttribute()),
		ASC->GetNumericAttribute(UDwUnitAttributeSet::GetMovePointsAttribute())) == EDwCostCheck::Ok;
}

void UDwGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return;
	}
	const FDwSharedEffects& Effects = DwGas::GetSharedEffects();
	auto ApplyToOwner = [&](TSubclassOf<UGameplayEffect> EffectClass, const FGameplayTag& DataTag, float Value)
	{
		FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, EffectClass, GetAbilityLevel(Handle, ActorInfo));
		if (Spec.IsValid())
		{
			if (DataTag.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(DataTag, Value);
			}
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	};

	const FGameplayAbilitySpec* AbilitySpec = ASC->FindAbilitySpecFromHandle(Handle);
	const FDwAbilityTuningRow* Tuning = GetTuning(AbilitySpec ? Cast<UDwAbilityDefinition>(AbilitySpec->SourceObject.Get()) : nullptr);
	if (Tuning && Tuning->ManaCost > 0)
	{
		ApplyToOwner(Effects.CostMana, DwTags::Data_Cost_Mana, -static_cast<float>(Tuning->ManaCost));
	}
	if (ActionCost == EDwActionCost::Action)
	{
		ApplyToOwner(Effects.CostAction, FGameplayTag(), 0.f);
	}
	else if (ActionCost == EDwActionCost::Move)
	{
		ApplyToOwner(Effects.CostMove, DwTags::Data_Cost_Move, -GetMoveCost(ActorInfo));
	}

	// Conqueror's crew: paying either budget spends both (K1: "attack or move, not both").
	if (ActionCost != EDwActionCost::Free && ASC->HasMatchingGameplayTag(DwTags::Rule_Turn_SingleChoice))
	{
		ApplyToOwner(Effects.SpendAll, FGameplayTag(), 0.f);
	}
}

int32 UDwGameplayAbility::GetRange(const FDwTargetingContext& Context) const
{
	const FDwTargetingRules& Rules = GetTargetingRules(Context);
	if (Rules.bIgnoreRange)
	{
		return MAX_int32 / 2;
	}
	const FDwAbilityTuningRow* Tuning = GetTuning(Context.Ability);
	return Tuning ? Tuning->Range : 0;
}

bool UDwGameplayAbility::IsUnitTargetable(const FDwTargetingContext& Context, const FDwUnitSnapshot& Source, FIntPoint SourceTile,
	const FDwUnitSnapshot& Candidate, int32 Range, FText* OutReason) const
{
	const FDwTargetingRules& Rules = GetTargetingRules(Context);
	auto Fail = [OutReason](const FText& Reason) { if (OutReason) { *OutReason = Reason; } return false; };
	const FDwGridModel& Grid = Context.Snapshot->Grid;

	if (!Candidate.bAlive)
	{
		return Fail(LOCTEXT("Dead", "Already dead"));
	}
	const bool bSameSide = Candidate.Side == Source.Side;
	if ((Rules.Side == EDwTargetSide::Enemy && bSameSide) || (Rules.Side == EDwTargetSide::Ally && !bSameSide))
	{
		return Fail(LOCTEXT("Side", "Wrong side"));
	}
	if (!Candidate.Tags.HasAll(Rules.RequiredTargetTags) || Candidate.Tags.HasAny(Rules.BlockedTargetTags))
	{
		return Fail(LOCTEXT("Tags", "Can't target this unit"));
	}
	if (!Rules.bCanTargetElevated)
	{
		const FDwTile* TargetTile = Grid.GetTile(Candidate.Tile);
		const FDwTile* FromTile = Grid.GetTile(SourceTile);
		// C5: melee can't reach a unit in the nest; C5a placeholder: nor can a unit in the nest melee down.
		if ((TargetTile && TargetTile->Elevation > 0) || (FromTile && FromTile->Elevation > 0))
		{
			return Fail(LOCTEXT("Elevated", "Out of reach (sniper's nest)"));
		}
	}
	if (&Candidate != &Source || Rules.Pattern != EDwTargetPattern::Self)
	{
		if (!FDwTargetingQuery::IsInRange(Grid, Context.Snapshot->GridRules, SourceTile, Candidate.Tile, Range))
		{
			return Fail(LOCTEXT("Range", "Out of range"));
		}
		if (Rules.bRequiresLineOfSight && !FDwLineOfSight::HasLineOfSight(Grid, SourceTile, Candidate.Tile))
		{
			return Fail(LOCTEXT("LOS", "No line of sight"));
		}
	}
	return true;
}

void UDwGameplayAbility::GatherValidTargets(const FDwTargetingContext& Context, TArray<FDwAbilityTarget>& OutTargets) const
{
	const FDwTargetingRules& Rules = GetTargetingRules(Context);
	OutTargets.Reset();
	const FDwUnitSnapshot* Source = Context.Snapshot ? Context.Snapshot->FindUnit(Context.Source) : nullptr;
	const TOptional<FIntPoint> SourceTile = Context.GetSourceTile();
	if (!Source || !Source->bAlive || !SourceTile.IsSet())
	{
		return;
	}
	const FDwGridModel& Grid = Context.Snapshot->Grid;
	const FDwTile* FromTile = Grid.GetTile(SourceTile.GetValue());
	if (!Rules.RequiredSourceTileTags.IsEmpty() && (!FromTile || !FromTile->Tags.HasAll(Rules.RequiredSourceTileTags)))
	{
		return;
	}

	const int32 Range = GetRange(Context);
	const EDwSide EnemySide = Source->Side == EDwSide::Player ? EDwSide::Enemy : EDwSide::Player;

	switch (Rules.Pattern)
	{
	case EDwTargetPattern::Self:
		OutTargets.Add(FDwAbilityTarget::MakeUnit(Source->Handle, SourceTile.GetValue()));
		break;

	case EDwTargetPattern::SingleUnit:
	case EDwTargetPattern::SingleUnitOrShip:
	case EDwTargetPattern::AllEnemies:
	case EDwTargetPattern::AllAllies:
	case EDwTargetPattern::RandomEnemies:
		for (const FDwUnitSnapshot& Candidate : Context.Snapshot->Units)
		{
			if (IsUnitTargetable(Context, *Source, SourceTile.GetValue(), Candidate, Range, nullptr))
			{
				OutTargets.Add(FDwAbilityTarget::MakeUnit(Candidate.Handle, Candidate.Tile));
			}
		}
		if (Rules.Pattern == EDwTargetPattern::SingleUnitOrShip && Rules.Side != EDwTargetSide::Ally && Context.Snapshot->FindShip(EnemySide))
		{
			OutTargets.Add(FDwAbilityTarget::MakeShip(EnemySide));
		}
		break;

	case EDwTargetPattern::EnemyShip:
		if (Context.Snapshot->FindShip(EnemySide))
		{
			OutTargets.Add(FDwAbilityTarget::MakeShip(EnemySide));
		}
		break;

	case EDwTargetPattern::Tile:
		for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
		{
			const FIntPoint Tile = Grid.ToTile(Index);
			if (Grid.IsStandable(Tile) && FDwTargetingQuery::IsInRange(Grid, Context.Snapshot->GridRules, SourceTile.GetValue(), Tile, Range)
				&& (!Rules.bRequiresLineOfSight || FDwLineOfSight::HasLineOfSight(Grid, SourceTile.GetValue(), Tile)))
			{
				OutTargets.Add(FDwAbilityTarget::MakeTile(Tile));
			}
		}
		break;

	case EDwTargetPattern::Line:
		// A direction is aimed by its first tile.
		for (const FIntPoint& Dir : FDwGridModel::Directions)
		{
			if (Grid.IsInBounds(SourceTile.GetValue() + Dir))
			{
				OutTargets.Add(FDwAbilityTarget::MakeTile(SourceTile.GetValue() + Dir));
			}
		}
		break;
	}
}

bool UDwGameplayAbility::IsValidTarget(const FDwTargetingContext& Context, const FDwAbilityTarget& Target, FText* OutReason) const
{
	const FDwTargetingRules& Rules = GetTargetingRules(Context);
	TArray<FDwAbilityTarget> Valid;
	GatherValidTargets(Context, Valid);
	const bool bGroup = Rules.Pattern == EDwTargetPattern::AllEnemies || Rules.Pattern == EDwTargetPattern::AllAllies
		|| Rules.Pattern == EDwTargetPattern::RandomEnemies;
	if ((bGroup && Valid.Num() > 0) || Valid.Contains(Target))
	{
		return true;
	}
	if (OutReason)
	{
		*OutReason = LOCTEXT("NoTarget", "No valid target");
		const FDwUnitSnapshot* Source = Context.Snapshot ? Context.Snapshot->FindUnit(Context.Source) : nullptr;
		const FDwUnitSnapshot* Candidate = Context.Snapshot ? Context.Snapshot->FindUnit(Target.Unit) : nullptr;
		const TOptional<FIntPoint> SourceTile = Context.GetSourceTile();
		if (Source && Candidate && SourceTile.IsSet())
		{
			IsUnitTargetable(Context, *Source, SourceTile.GetValue(), *Candidate, GetRange(Context), OutReason);
		}
	}
	return false;
}

void UDwGameplayAbility::ResolveTargets(const FDwTargetingContext& Context, const FDwAbilityTarget& Target, TArray<FDwAbilityTarget>& OutTargets) const
{
	const FDwTargetingRules& Rules = GetTargetingRules(Context);
	OutTargets.Reset();
	TArray<FDwAbilityTarget> Valid;
	GatherValidTargets(Context, Valid);

	switch (Rules.Pattern)
	{
	case EDwTargetPattern::AllEnemies:
	case EDwTargetPattern::AllAllies:
		OutTargets = Valid;
		return;

	case EDwTargetPattern::RandomEnemies:
	{
		if (Valid.IsEmpty())
		{
			return;
		}
		if (!Context.Rng)
		{
			OutTargets = Valid; // no stream (previews): every possible target, once
			return;
		}
		const FDwAbilityTuningRow* Tuning = GetTuning(Context.Ability);
		const int32 MinHits = FMath::Max(1, Tuning ? Tuning->MinHits : 1);
		const int32 MaxHits = FMath::Max(MinHits, Tuning ? Tuning->MaxHits : 1);
		const int32 Hits = Context.Rng->RandRange(MinHits, MaxHits);
		for (int32 Hit = 0; Hit < Hits; ++Hit)
		{
			OutTargets.Add(Valid[Context.Rng->RandRange(0, Valid.Num() - 1)]);
		}
		return;
	}

	default:
		break;
	}

	if (!Valid.Contains(Target))
	{
		return;
	}

	// Defend redirect: a single-target enemy hit on a guarded unit goes to its guardian, if alive and next to it.
	const bool bSingle = Rules.Pattern == EDwTargetPattern::SingleUnit || Rules.Pattern == EDwTargetPattern::SingleUnitOrShip;
	const FDwUnitSnapshot* Source = Context.Snapshot->FindUnit(Context.Source);
	const FDwUnitSnapshot* Victim = Context.Snapshot->FindUnit(Target.Unit);
	if (bSingle && Source && Victim && Victim->Side != Source->Side && Victim->Tags.HasTagExact(DwTags::Status_Guarded))
	{
		const FDwUnitSnapshot* Guardian = Context.Snapshot->FindUnit(Victim->GuardedBy);
		if (Guardian && Guardian->bAlive && FDwLineOfSight::GetDistance(Context.Snapshot->GridRules, Guardian->Tile, Victim->Tile) == 1)
		{
			OutTargets.Add(FDwAbilityTarget::MakeUnit(Guardian->Handle, Guardian->Tile));
			return;
		}
	}
	OutTargets.Add(Target);
}

FDwAbilityPreview UDwGameplayAbility::PreviewOutcome(const FDwTargetingContext& Context, const FDwAbilityTarget& Target) const
{
	const FDwTargetingRules& Rules = GetTargetingRules(Context);
	FDwAbilityPreview Preview;
	if (!IsValidTarget(Context, Target, &Preview.Reason))
	{
		return Preview;
	}
	Preview.bValid = true;
	Preview.Reason = FText::GetEmpty();

	const FDwAbilityTuningRow* Tuning = GetTuning(Context.Ability);
	const FDwUnitSnapshot* Source = Context.Snapshot->FindUnit(Context.Source);
	if (Rules.Pattern == EDwTargetPattern::RandomEnemies && Tuning)
	{
		Preview.MinHits = FMath::Max(1, Tuning->MinHits);
		Preview.MaxHits = FMath::Max(Preview.MinHits, Tuning->MaxHits);
	}

	FDwTargetingContext PreviewContext = Context;
	PreviewContext.Rng = nullptr; // previews never consume the combat random stream
	TArray<FDwAbilityTarget> Final;
	ResolveTargets(PreviewContext, Target, Final);

	for (const FDwAbilityTarget& Hit : Final)
	{
		FDwTargetPreview& Entry = Preview.Targets.AddDefaulted_GetRef();
		Entry.Target = Hit;
		if (!Tuning || !Source)
		{
			continue;
		}
		FDwDamageInput Input;
		Input.Flat = Tuning->Damage;
		Input.Scale = Tuning->DamageScale;
		Input.AttackDamage = Source->AttackDamage;
		Input.DamageBonus = Source->DamageBonus;
		Input.SituationalMultiplier = Context.DamageMultiplier;

		float Health = 0.f;
		if (const FDwUnitSnapshot* Victim = Context.Snapshot->FindUnit(Hit.Unit))
		{
			Input.DamageReduction = Victim->DamageReduction;
			Health = Victim->Health;
		}
		else if (const FDwShipSnapshot* Ship = Hit.bShip ? Context.Snapshot->FindShip(Hit.ShipSide) : nullptr)
		{
			Input.DamageBonus = 0.f; // ships take flat damage (UDwDamageExecution skips unit-only terms)
			Health = Ship->Health;
		}
		Entry.Damage = (Tuning->Damage > 0 || Tuning->DamageScale > 0.f) ? FDwDamageRules::Compute(Input).Damage : 0;
		Entry.Heal = Tuning->Heal;
		Entry.Fear = Tuning->Fear;
		Entry.bLethal = Entry.Damage > 0 && Entry.Damage >= Health;
	}
	return Preview;
}

FGameplayEffectSpecHandle UDwGameplayAbility::MakeTunedSpec(TSubclassOf<UGameplayEffect> EffectClass) const
{
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(EffectClass, GetAbilityLevel());
	if (Spec.IsValid())
	{
		if (const FDwAbilityTuningRow* Tuning = GetTuning(GetDefinition()))
		{
			ApplyTuningToSpec(*Spec.Data, *Tuning);
		}
	}
	return Spec;
}

void UDwGameplayAbility::ApplyTuningToSpec(FGameplayEffectSpec& Spec, const FDwAbilityTuningRow& Tuning)
{
	Spec.SetSetByCallerMagnitude(DwTags::Data_Damage, Tuning.Damage);
	Spec.SetSetByCallerMagnitude(DwTags::Data_DamageScale, Tuning.DamageScale);
	Spec.SetSetByCallerMagnitude(DwTags::Data_Heal, Tuning.Heal);
	Spec.SetSetByCallerMagnitude(DwTags::Data_Fear, Tuning.Fear);
	if (Tuning.DurationTurns > 0)
	{
		Spec.SetSetByCallerMagnitude(DwTags::Data_Turns, Tuning.DurationTurns);
	}
}


// ---------------------------------------------------------------------------------------------------------------------
// Combat helpers

UDwCombatSubsystem* UDwGameplayAbility::GetCombat() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDwCombatSubsystem>() : nullptr;
}

ADwCombatUnit* UDwGameplayAbility::GetOwnerUnit() const
{
	return Cast<ADwCombatUnit>(GetAvatarActorFromActorInfo());
}

const FDwGameplayAbilityTargetData_Grid* UDwGameplayAbility::GetGridTarget(const FGameplayEventData* EventData)
{
	if (!EventData || !EventData->TargetData.IsValid(0))
	{
		return nullptr;
	}
	const FGameplayAbilityTargetData* Data = EventData->TargetData.Get(0);
	return Data && Data->GetScriptStruct() == FDwGameplayAbilityTargetData_Grid::StaticStruct()
		? static_cast<const FDwGameplayAbilityTargetData_Grid*>(Data) : nullptr;
}

void UDwGameplayAbility::CancelActivation()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), /*bReplicateEndAbility*/ false, /*bWasCancelled*/ true);
}

#undef LOCTEXT_NAMESPACE
