// Copyright Brandon Hillig 2026

#include "Abilities/DwGA_Grapple.h"

#include "Board/DwGridQueries.h"
#include "Combat/DwCombatSnapshot.h"
#include "Combat/DwCombatSubsystem.h"
#include "Combat/DwCombatUnit.h"
#include "Core/DwGameplayTags.h"

UDwGA_Grapple::UDwGA_Grapple()
{
	Targeting.Pattern = EDwTargetPattern::SingleUnit;
	Targeting.Side = EDwTargetSide::Any;
	Targeting.bRequiresLineOfSight = true;
	Targeting.BlockedTargetTags.AddTag(DwTags::Unit_Immune_Grapple);
	PlaceholderSeconds = 0.6f;
}

TOptional<FIntPoint> UDwGA_Grapple::GetLandingTile(const FDwGridModel& Grid, FIntPoint SourceTile, FIntPoint TargetTile)
{
	if (FMath::Abs(TargetTile.X - SourceTile.X) + FMath::Abs(TargetTile.Y - SourceTile.Y) <= 1)
	{
		return {}; // already beside the captain (or the captain himself)
	}
	// The free 4-way neighbour closest to where the target stood, at the captain's own height: a pull never lands a unit
	// up in the sniper's nest (or down from it beside a captain who is up there).
	const FDwTile* Source = Grid.GetTile(SourceTile);
	TOptional<FIntPoint> Best;
	int32 BestDistance = MAX_int32;
	for (const FIntPoint& Dir : FDwGridModel::Directions)
	{
		const FIntPoint Candidate = SourceTile + Dir;
		const FDwTile* Tile = Grid.GetTile(Candidate);
		if (!Tile || !Grid.IsStandable(Candidate) || Grid.IsOccupied(Candidate) || (Source && Tile->Elevation != Source->Elevation))
		{
			continue;
		}
		const int32 Distance = FMath::Abs(Candidate.X - TargetTile.X) + FMath::Abs(Candidate.Y - TargetTile.Y);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Candidate;
		}
	}
	return Best;
}

void UDwGA_Grapple::GatherValidTargets(const FDwTargetingContext& Context, TArray<FDwAbilityTarget>& OutTargets) const
{
	Super::GatherValidTargets(Context, OutTargets);
	const TOptional<FIntPoint> SourceTile = Context.GetSourceTile();
	if (!Context.Snapshot || !SourceTile.IsSet())
	{
		OutTargets.Reset();
		return;
	}
	OutTargets.RemoveAll([&Context, &SourceTile](const FDwAbilityTarget& Target)
		{ return !Target.IsUnit() || !GetLandingTile(Context.Snapshot->Grid, SourceTile.GetValue(), Target.Tile).IsSet(); });
}

void UDwGA_Grapple::ApplyPayload()
{
	UDwCombatSubsystem* Combat = GetCombat();
	const ADwCombatUnit* Self = GetOwnerUnit();
	if (Combat && Self)
	{
		const FDwCombatSnapshot Snapshot = Combat->GetSnapshot(); // the board right now
		for (const FDwAbilityTarget& Target : ResolvedTargets)
		{
			const ADwCombatUnit* Pulled = Combat->FindUnit(Target.Unit);
			const TOptional<FIntPoint> Landing = Pulled && Pulled->IsAlive() ? GetLandingTile(Snapshot.Grid, Self->GetTile(), Pulled->GetTile()) : TOptional<FIntPoint>();
			if (Landing.IsSet())
			{
				Combat->MoveUnit(Target.Unit, Landing.GetValue(), /*bSnapActor*/ true);
			}
		}
	}
	Super::ApplyPayload();
}
