// Copyright Brandon Hillig 2026

#include "Abilities/DwGA_Charge.h"

#include "Board/DwGridQueries.h"
#include "Combat/DwCombatSnapshot.h"
#include "Combat/DwCombatSubsystem.h"
#include "Combat/DwCombatUnit.h"

UDwGA_Charge::UDwGA_Charge()
{
	Targeting.Pattern = EDwTargetPattern::SingleUnit;
	Targeting.Side = EDwTargetSide::Enemy;
	Targeting.bCanTargetElevated = false;
	PlaceholderSeconds = 0.6f;
}

namespace DwCharge
{
	/** Every direction's line hit from the source, up to Range. */
	void ForEachLine(const FDwTargetingContext& Context, int32 Range, TFunctionRef<void(const FDwLineHit&)> Visit)
	{
		const TOptional<FIntPoint> Source = Context.GetSourceTile();
		if (!Context.Snapshot || !Source.IsSet())
		{
			return;
		}
		for (const FIntPoint& Dir : FDwGridModel::Directions)
		{
			Visit(FDwTargetingQuery::FirstUnitAlongLine(Context.Snapshot->Grid, Context.Snapshot->GridRules, Source.GetValue(), Dir, Range));
		}
	}
}

TOptional<FIntPoint> UDwGA_Charge::GetStopTile(const FDwTargetingContext& Context, int32 Range, FDwUnitHandle Target)
{
	TOptional<FIntPoint> Stop;
	DwCharge::ForEachLine(Context, Range, [&Stop, Target](const FDwLineHit& Hit)
	{
		if (Hit.Unit.IsSet() && Hit.Unit.Unit == Target)
		{
			Stop = Hit.StopTile;
		}
	});
	return Stop;
}

void UDwGA_Charge::GatherValidTargets(const FDwTargetingContext& Context, TArray<FDwAbilityTarget>& OutTargets) const
{
	// The base rules (enemy side, alive, tags), then only the ones a straight line reaches first.
	TArray<FDwAbilityTarget> Candidates;
	Super::GatherValidTargets(Context, Candidates);
	OutTargets.Reset();
	const int32 Range = GetRange(Context);
	DwCharge::ForEachLine(Context, Range, [&](const FDwLineHit& Hit)
	{
		const FDwAbilityTarget* Candidate = Hit.Unit.IsSet()
			? Candidates.FindByPredicate([&Hit](const FDwAbilityTarget& T) { return T.Unit == Hit.Unit.Unit; }) : nullptr;
		if (Candidate)
		{
			OutTargets.AddUnique(*Candidate);
		}
	});
}

void UDwGA_Charge::ResolveTargets(const FDwTargetingContext& Context, const FDwAbilityTarget& Target, TArray<FDwAbilityTarget>& OutTargets) const
{
	OutTargets.Reset();
	TArray<FDwAbilityTarget> Valid;
	GatherValidTargets(Context, Valid);
	if (const FDwAbilityTarget* Hit = Valid.FindByKey(Target))
	{
		OutTargets.Add(*Hit); // a charge is never redirected: the captain crashes into whoever is in front of him
	}
}

bool UDwGA_Charge::PrepareActivation(const FDwTargetingContext& Context, const FDwAbilityTarget& Aimed)
{
	StopTile = ResolvedTargets.IsEmpty() ? TOptional<FIntPoint>() : GetStopTile(Context, GetRange(Context), ResolvedTargets[0].Unit);
	return StopTile.IsSet();
}

void UDwGA_Charge::ApplyPayload()
{
	UDwCombatSubsystem* Combat = GetCombat();
	const ADwCombatUnit* Self = GetOwnerUnit();
	if (Combat && Self && StopTile.IsSet() && StopTile.GetValue() != Self->GetTile())
	{
		Combat->MoveUnit(Self->GetHandle(), StopTile.GetValue(), /*bSnapActor*/ true);
	}
	Super::ApplyPayload();
}
