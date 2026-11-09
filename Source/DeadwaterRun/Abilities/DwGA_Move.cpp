// Copyright Brandon Hillig 2026

#include "Abilities/DwGA_Move.h"

#include "AbilitySystemComponent.h"
#include "Abilities/DwAttributeSets.h"
#include "Abilities/DwGameplayEffects.h"
#include "Abilities/Tasks/DwAT_MoveAlongPath.h"
#include "Board/DwBoard.h"
#include "Board/DwGridQueries.h"
#include "Combat/DwCombatSnapshot.h"
#include "Combat/DwCombatSubsystem.h"
#include "Combat/DwCombatUnit.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"

UDwGA_Move::UDwGA_Move()
{
	ActionCost = EDwActionCost::Move;
	Targeting.Pattern = EDwTargetPattern::Tile;
	Targeting.Side = EDwTargetSide::Any;
	ActivationBlockedTags.AddTag(DwTags::Status_Rooted);
}

void UDwGA_Move::GatherValidTargets(const FDwTargetingContext& Context, TArray<FDwAbilityTarget>& OutTargets) const
{
	OutTargets.Reset();
	const FDwUnitSnapshot* Unit = Context.Snapshot ? Context.Snapshot->FindUnit(Context.Source) : nullptr;
	const TOptional<FIntPoint> From = Context.GetSourceTile();
	if (!Unit || !Unit->bAlive || !From.IsSet() || Unit->MovePoints <= 0.f)
	{
		return;
	}
	const TMap<FIntPoint, int32> Reachable = FDwPathfinder::ComputeReachable(Context.Snapshot->Grid, Context.Snapshot->GridRules,
		From.GetValue(), FMath::FloorToInt(Unit->MovePoints), Unit->Side);
	TArray<FIntPoint> Tiles;
	Reachable.GetKeys(Tiles);
	const FDwGridModel& Grid = Context.Snapshot->Grid;
	Tiles.Sort([&Grid](const FIntPoint& A, const FIntPoint& B) { return Grid.ToIndex(A) < Grid.ToIndex(B); }); // deterministic order
	for (const FIntPoint& Tile : Tiles)
	{
		OutTargets.Add(FDwAbilityTarget::MakeTile(Tile));
	}
}

void UDwGA_Move::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const FDwGameplayAbilityTargetData_Grid* Target = GetGridTarget(TriggerEventData);
	UDwCombatSubsystem* Combat = GetCombat();
	ADwCombatUnit* Unit = GetOwnerUnit();
	UAbilitySystemComponent* Asc = GetAbilitySystemComponentFromActorInfo();
	if (!Target || !Combat || !Unit || !Asc)
	{
		CancelActivation();
		return;
	}

	const FIntPoint Start = Unit->GetTile();
	const TArray<FIntPoint> Path = FDwPathfinder::FindPath(Combat->GetGrid(), Combat->GetGridRules(), Start, Target->Tile, Unit->GetSide());
	const int32 Cost = FDwPathfinder::GetPathCost(Combat->GetGrid(), Combat->GetGridRules(), Start, Path);
	if (Path.IsEmpty() || Cost > Asc->GetNumericAttribute(UDwUnitAttributeSet::GetMovePointsAttribute()))
	{
		CancelActivation();
		return;
	}

	PendingCost = static_cast<float>(Cost);
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		CancelActivation();
		return;
	}
	// C3: one move per turn - whatever is left of the budget is gone.
	const float Left = Asc->GetNumericAttribute(UDwUnitAttributeSet::GetMovePointsAttribute());
	if (Left > 0.f)
	{
		FGameplayEffectSpecHandle Spend = MakeOutgoingGameplayEffectSpec(DwGas::GetSharedEffects().CostMove, GetAbilityLevel());
		Spend.Data->SetSetByCallerMagnitude(DwTags::Data_Cost_Move, -Left);
		Asc->ApplyGameplayEffectSpecToSelf(*Spend.Data);
	}

	Combat->MoveUnit(Unit->GetHandle(), Path.Last());

	TArray<FVector> Points;
	if (const ADwBoard* Board = Combat->GetBoard())
	{
		for (const FIntPoint& Tile : Path)
		{
			Points.Add(Board->TileToWorld(Tile));
		}
	}
	UDwAT_MoveAlongPath* Walk = UDwAT_MoveAlongPath::MoveAlongPath(this, Points, WalkSpeed);
	Walk->OnCompleted.AddDynamic(this, &ThisClass::HandleArrived);
	Walk->ReadyForActivation();
}

void UDwGA_Move::HandleArrived()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), false, false);
}
