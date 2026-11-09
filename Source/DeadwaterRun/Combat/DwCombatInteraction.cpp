// Copyright Brandon Hillig 2026

#include "Combat/DwCombatInteraction.h"

#include "Abilities/DwGA_FireCannon.h"
#include "Abilities/DwGameplayAbility.h"
#include "Board/DwBoard.h"
#include "Board/DwGridQueries.h"
#include "Combat/DwCombatRules.h"
#include "Combat/DwCombatSubsystem.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwItemDefinition.h"

#define LOCTEXT_NAMESPACE "DwCombatInteraction"

namespace DwInteraction
{
	/** The unit's basic attack: the first granted Ability.Attack.* (it is always granted first). */
	static const UDwAbilityDefinition* FindBasicAttack(const FDwUnitSnapshot& Unit)
	{
		for (const UDwAbilityDefinition* Ability : Unit.Abilities)
		{
			if (Ability && Ability->AbilityTag.MatchesTag(DwTags::Ability_Attack))
			{
				return Ability;
			}
		}
		return nullptr;
	}

	static bool IsGroup(EDwTargetPattern Pattern)
	{
		return Pattern == EDwTargetPattern::AllEnemies || Pattern == EDwTargetPattern::AllAllies || Pattern == EDwTargetPattern::RandomEnemies;
	}
}

void UDwCombatInteraction::Initialize(UDwCombatSubsystem& InCombat)
{
	Shutdown();
	Combat = &InCombat;
	Combat->OnCommandResolved.AddDynamic(this, &ThisClass::HandleCommandResolved);
	Combat->OnCombatEnded.AddDynamic(this, &ThisClass::HandleCombatEnded);
	Combat->OnEndTurnSuggested.AddDynamic(this, &ThisClass::HandleEndTurnSuggested);
}

void UDwCombatInteraction::Shutdown()
{
	if (Combat)
	{
		ClearHighlights();
		Combat->OnCommandResolved.RemoveAll(this);
		Combat->OnCombatEnded.RemoveAll(this);
		Combat->OnEndTurnSuggested.RemoveAll(this);
	}
	Combat = nullptr;
	Mode = EDwInteractionMode::Idle;
	SelectedUnit = FDwUnitHandle();
	Disarm();
}

bool UDwCombatInteraction::CanAct() const
{
	return Combat && Combat->IsActive() && !Combat->IsResolving() && Combat->GetActiveSide() == EDwSide::Player
		&& Combat->GetOutcome() == EDwCombatOutcome::None;
}

ADwBoard* UDwCombatInteraction::GetBoard() const
{
	return Combat ? Combat->GetBoard() : nullptr;
}

FDwTargetingContext UDwCombatInteraction::MakeContext(const FDwCombatSnapshot& Snapshot, const UDwAbilityDefinition* Ability, FName ItemId) const
{
	FDwTargetingContext Context;
	Context.Snapshot = &Snapshot;
	Context.Ability = Ability;
	Context.Source = SelectedUnit;
	Context.ItemId = ItemId;
	return Context;
}

// ---------------------------------------------------------------------------------------------------------------------
// Selection

void UDwCombatInteraction::BeginPlayerTurn()
{
	bEndTurnSuggested = false;
	Message = FText::GetEmpty();
	Disarm();
	const FDwCombatSnapshot Snapshot = Combat ? Combat->GetSnapshot() : FDwCombatSnapshot();
	const FDwUnitSnapshot* Current = Snapshot.FindUnit(SelectedUnit);
	if (!Current || !Current->bAlive)
	{
		SelectedUnit = FDwUnitHandle();
		for (const FDwUnitSnapshot& Unit : Snapshot.Units)
		{
			if (Unit.Side == EDwSide::Player && Unit.bAlive)
			{
				SelectedUnit = Unit.Handle;
				break;
			}
		}
	}
	Mode = SelectedUnit.IsValid() ? EDwInteractionMode::Move : EDwInteractionMode::Idle;
	Refresh();
}

void UDwCombatInteraction::SelectUnit(FDwUnitHandle Unit)
{
	if (!Combat)
	{
		return;
	}
	const FDwCombatSnapshot Snapshot = Combat->GetSnapshot();
	const FDwUnitSnapshot* Data = Snapshot.FindUnit(Unit);
	if (!Data || !Data->bAlive || Data->Side != EDwSide::Player)
	{
		return;
	}
	SelectedUnit = Unit;
	Disarm();
	Mode = EDwInteractionMode::Move;
	Message = FText::GetEmpty();
	Refresh();
}

bool UDwCombatInteraction::SelectAbility(const UDwAbilityDefinition* Ability, FName ItemId)
{
	if (!CanAct() || !SelectedUnit.IsValid() || !Ability)
	{
		return false;
	}
	const FDwCombatSnapshot Snapshot = Combat->GetSnapshot();
	const FDwUnitSnapshot* Unit = Snapshot.FindUnit(SelectedUnit);
	const UDwGameplayAbility* CDO = FDwCombatQueries::GetAbilityCDO(Ability);
	if (!Unit || !CDO)
	{
		return false;
	}
	FText Reason;
	if (FDwCombatQueries::CanActivate(Snapshot, *Unit, Ability, ItemId, &Reason) != EDwCommandResult::Accepted)
	{
		Message = Reason;
		OnChanged.Broadcast();
		return false;
	}
	TArray<FDwAbilityTarget> Targets;
	CDO->GatherValidTargets(MakeContext(Snapshot, Ability, ItemId), Targets);
	if (Targets.IsEmpty())
	{
		Message = Ability->AbilityTag == DwTags::Ability_Cannon ? UDwGA_FireCannon::GetNoCannonReason() : LOCTEXT("NoTarget", "No target in reach");
		OnChanged.Broadcast();
		return false;
	}
	ArmedAbility = Ability;
	ArmedItem = ItemId;
	Mode = EDwInteractionMode::Targeting;
	Message = FText::GetEmpty();
	Refresh();
	return true;
}

bool UDwCombatInteraction::SelectSlot(int32 Index)
{
	const TArray<FDwActionSlot> Bar = GetActionBar();
	return Bar.IsValidIndex(Index) && SelectAbility(Bar[Index].Ability, Bar[Index].ItemId);
}

bool UDwCombatInteraction::Cancel()
{
	if (Mode == EDwInteractionMode::Targeting)
	{
		Disarm();
		Mode = EDwInteractionMode::Move;
		Refresh();
		return true;
	}
	if (Mode == EDwInteractionMode::Move)
	{
		SelectedUnit = FDwUnitHandle();
		Mode = EDwInteractionMode::Idle;
		Refresh();
		return true;
	}
	return false;
}

void UDwCombatInteraction::SelectNextUnit()
{
	if (!Combat)
	{
		return;
	}
	const FDwCombatSnapshot Snapshot = Combat->GetSnapshot();
	TArray<const FDwUnitSnapshot*> Crew;
	for (const FDwUnitSnapshot& Unit : Snapshot.Units)
	{
		if (Unit.Side == EDwSide::Player && Unit.bAlive)
		{
			Crew.Add(&Unit);
		}
	}
	if (Crew.IsEmpty())
	{
		return;
	}
	const int32 Current = Crew.IndexOfByPredicate([this](const FDwUnitSnapshot* Unit) { return Unit->Handle == SelectedUnit; });
	// Prefer the next unit that still has something to do; otherwise just the next one.
	for (int32 Step = 1; Step <= Crew.Num(); ++Step)
	{
		const FDwUnitSnapshot* Candidate = Crew[(Current + Step + Crew.Num()) % Crew.Num()];
		const bool bCanDoSomething = Candidate->Abilities.ContainsByPredicate([&](const UDwAbilityDefinition* Ability)
			{ return FDwCombatQueries::HasAnyUse(Snapshot, *Candidate, Ability); });
		if (bCanDoSomething)
		{
			SelectUnit(Candidate->Handle);
			return;
		}
	}
	SelectUnit(Crew[(Current + 1 + Crew.Num()) % Crew.Num()]->Handle);
}

void UDwCombatInteraction::RequestEndTurn()
{
	if (CanAct())
	{
		Disarm();
		Submit(FDwCombatCommand::MakeEndTurn(EDwSide::Player));
	}
}

void UDwCombatInteraction::Disarm()
{
	ArmedAbility = nullptr;
	ArmedItem = NAME_None;
	ValidTargets.Reset();
	Preview = FDwAbilityPreview();
}

// ---------------------------------------------------------------------------------------------------------------------
// Pointer

void UDwCombatInteraction::HoverTile(TOptional<FIntPoint> Tile)
{
	if (HoveredTile == Tile || !Combat || !Combat->IsActive())
	{
		return;
	}
	HoveredTile = Tile;
	const FDwCombatSnapshot Snapshot = Combat->GetSnapshot();
	UpdateHover(Snapshot);
	DrawHighlights(Snapshot);
	OnChanged.Broadcast();
}

void UDwCombatInteraction::ClickTile(FIntPoint Tile)
{
	if (!CanAct())
	{
		Message = Combat && Combat->IsResolving() ? LOCTEXT("Wait", "Wait for the action to finish") : LOCTEXT("NotYourTurn", "Not your turn");
		OnChanged.Broadcast();
		return;
	}
	const FDwCombatSnapshot Snapshot = Combat->GetSnapshot();
	const FDwOccupant Occupant = Snapshot.Grid.GetOccupant(Tile);
	const bool bOwnUnit = Occupant.IsSet() && Occupant.Side == EDwSide::Player;

	if (Mode == EDwInteractionMode::Targeting && ArmedAbility)
	{
		if (const TOptional<FDwAbilityTarget> Target = TargetAt(Snapshot, Tile, ArmedAbility, ValidTargets))
		{
			const FGameplayTag Tag = ArmedAbility->AbilityTag;
			const FName Item = ArmedItem;
			Disarm();
			Mode = EDwInteractionMode::Move;
			Submit(FDwCombatCommand::MakeAbility(SelectedUnit, Tag, Target.GetValue(), Item));
			return;
		}
		if (bOwnUnit)
		{
			SelectUnit(Occupant.Unit);
			return;
		}
		Message = LOCTEXT("NotATarget", "Not a valid target");
		OnChanged.Broadcast();
		return;
	}

	if (bOwnUnit)
	{
		SelectUnit(Occupant.Unit);
		return;
	}
	if (Mode != EDwInteractionMode::Move)
	{
		return;
	}
	if (Reachable.Contains(Tile))
	{
		Submit(FDwCombatCommand::MakeMove(SelectedUnit, Tile));
		return;
	}
	// Convenience: clicking an enemy in reach uses the basic attack (the hover already showed its preview).
	const FDwUnitSnapshot* Unit = Snapshot.FindUnit(SelectedUnit);
	const UDwAbilityDefinition* Attack = Unit ? DwInteraction::FindBasicAttack(*Unit) : nullptr;
	if (Occupant.IsSet() && Attack)
	{
		Submit(FDwCombatCommand::MakeAbility(SelectedUnit, Attack->AbilityTag, FDwAbilityTarget::MakeUnit(Occupant.Unit, Tile)));
		return;
	}
	Message = LOCTEXT("CantMove", "Can't move there");
	OnChanged.Broadcast();
}

EDwCommandResult UDwCombatInteraction::Submit(const FDwCombatCommand& Command)
{
	FText Reason;
	const EDwCommandResult Result = Combat->SubmitCommand(Command, &Reason);
	Message = Result == EDwCommandResult::Accepted ? FText::GetEmpty()
		: (Reason.IsEmpty() ? StaticEnum<EDwCommandResult>()->GetDisplayNameTextByValue(static_cast<int64>(Result)) : Reason);
	// Rejected: show why. Accepted and still playing: drop the move/target highlights until it resolves. Accepted
	// and already resolved (instant mode) refreshed through OnCommandResolved.
	if (Result != EDwCommandResult::Accepted || (Combat && Combat->IsResolving()))
	{
		Refresh();
	}
	return Result;
}

TOptional<FDwAbilityTarget> UDwCombatInteraction::TargetAt(const FDwCombatSnapshot& Snapshot, FIntPoint Tile, const UDwAbilityDefinition* Ability,
	const TArray<FDwAbilityTarget>& Valid) const
{
	const FDwOccupant Occupant = Snapshot.Grid.GetOccupant(Tile);
	const FDwTile* TileData = Snapshot.Grid.GetTile(Tile);
	for (const FDwAbilityTarget& Target : Valid)
	{
		if (Target.IsUnit() && Occupant.IsSet() && Target.Unit == Occupant.Unit)
		{
			return Target;
		}
		if (Target.bShip && TileData && TileData->Owner == Target.ShipSide)
		{
			return Target; // any tile of that ship's deck aims at the ship
		}
		if (!Target.IsUnit() && !Target.bShip && Target.Tile == Tile)
		{
			return Target;
		}
	}
	return TOptional<FDwAbilityTarget>();
}

// ---------------------------------------------------------------------------------------------------------------------
// Refresh and highlights

void UDwCombatInteraction::Refresh()
{
	Reachable.Reset();
	ValidTargets.Reset();
	if (!Combat || !Combat->IsActive())
	{
		Mode = EDwInteractionMode::Idle;
		return;
	}
	const FDwCombatSnapshot Snapshot = Combat->GetSnapshot();
	const FDwUnitSnapshot* Unit = Snapshot.FindUnit(SelectedUnit);
	if (!Unit || !Unit->bAlive || Unit->Side != EDwSide::Player)
	{
		SelectedUnit = FDwUnitHandle();
		Mode = EDwInteractionMode::Idle;
		Disarm();
	}
	else if (const UDwAbilityDefinition* MoveAbility = FDwCombatQueries::FindAbility(*Unit, DwTags::Ability_Move))
	{
		if (FDwCombatQueries::CanActivate(Snapshot, *Unit, MoveAbility) == EDwCommandResult::Accepted)
		{
			TArray<FDwAbilityTarget> Tiles;
			FDwCombatQueries::GetAbilityCDO(MoveAbility)->GatherValidTargets(MakeContext(Snapshot, MoveAbility, NAME_None), Tiles);
			for (const FDwAbilityTarget& Tile : Tiles)
			{
				Reachable.Add(Tile.Tile);
			}
		}
	}

	if (Mode == EDwInteractionMode::Targeting && ArmedAbility && Unit)
	{
		// Still usable? (MP spent elsewhere, the target died, the turn changed.)
		if (FDwCombatQueries::CanActivate(Snapshot, *Unit, ArmedAbility, ArmedItem) == EDwCommandResult::Accepted)
		{
			FDwCombatQueries::GetAbilityCDO(ArmedAbility)->GatherValidTargets(MakeContext(Snapshot, ArmedAbility, ArmedItem), ValidTargets);
		}
		if (ValidTargets.IsEmpty())
		{
			Disarm();
			Mode = EDwInteractionMode::Move;
		}
	}
	UpdateHover(Snapshot);
	DrawHighlights(Snapshot);
	OnChanged.Broadcast();
}

void UDwCombatInteraction::UpdateHover(const FDwCombatSnapshot& Snapshot)
{
	Path.Reset();
	Preview = FDwAbilityPreview();
	const FDwUnitSnapshot* Unit = Snapshot.FindUnit(SelectedUnit);
	if (!HoveredTile.IsSet() || !Unit || !CanAct())
	{
		return;
	}
	const FIntPoint Tile = HoveredTile.GetValue();
	if (Mode == EDwInteractionMode::Targeting && ArmedAbility)
	{
		if (const TOptional<FDwAbilityTarget> Target = TargetAt(Snapshot, Tile, ArmedAbility, ValidTargets))
		{
			Preview = FDwCombatQueries::GetAbilityCDO(ArmedAbility)->PreviewOutcome(MakeContext(Snapshot, ArmedAbility, ArmedItem), Target.GetValue());
		}
		return;
	}
	if (Mode == EDwInteractionMode::Move)
	{
		if (Reachable.Contains(Tile))
		{
			Path = FDwPathfinder::FindPath(Snapshot.Grid, Snapshot.GridRules, Unit->Tile, Tile, Unit->Side);
			return;
		}
		const FDwOccupant Occupant = Snapshot.Grid.GetOccupant(Tile);
		const UDwAbilityDefinition* Attack = DwInteraction::FindBasicAttack(*Unit);
		if (Occupant.IsSet() && Occupant.Side != Unit->Side && Attack)
		{
			Preview = FDwCombatQueries::GetAbilityCDO(Attack)->PreviewOutcome(MakeContext(Snapshot, Attack, NAME_None),
				FDwAbilityTarget::MakeUnit(Occupant.Unit, Tile));
		}
	}
}

void UDwCombatInteraction::ClearHighlights()
{
	if (ADwBoard* Board = GetBoard())
	{
		Board->ClearAllHighlights();
	}
}

void UDwCombatInteraction::DrawHighlights(const FDwCombatSnapshot& Snapshot)
{
	ADwBoard* Board = GetBoard();
	if (!Board)
	{
		return;
	}
	Board->ClearAllHighlights();
	TArray<FIntPoint> Hover;
	if (HoveredTile.IsSet())
	{
		Hover.Add(HoveredTile.GetValue());
	}
	if (const FDwUnitSnapshot* Unit = Snapshot.FindUnit(SelectedUnit); Unit && Unit->bAlive)
	{
		Hover.AddUnique(Unit->Tile); // the selected pirate's tile
	}
	Board->ShowHighlights(EDwHighlightLayer::Hover, Hover);
	if (!CanAct() || Mode == EDwInteractionMode::Idle)
	{
		return;
	}

	if (Mode == EDwInteractionMode::Move)
	{
		Board->ShowHighlights(EDwHighlightLayer::Move, Reachable);
		Board->ShowHighlights(EDwHighlightLayer::Path, Path);
		// Enemies the basic attack can hit right now.
		const FDwUnitSnapshot* Unit = Snapshot.FindUnit(SelectedUnit);
		const UDwAbilityDefinition* Attack = Unit ? DwInteraction::FindBasicAttack(*Unit) : nullptr;
		TArray<FIntPoint> Attackable;
		if (Attack && FDwCombatQueries::CanActivate(Snapshot, *Unit, Attack) == EDwCommandResult::Accepted)
		{
			TArray<FDwAbilityTarget> Targets;
			FDwCombatQueries::GetAbilityCDO(Attack)->GatherValidTargets(MakeContext(Snapshot, Attack, NAME_None), Targets);
			for (const FDwAbilityTarget& Target : Targets)
			{
				Attackable.Add(Target.Tile);
			}
		}
		Board->ShowHighlights(EDwHighlightLayer::Target, Attackable);
		return;
	}

	// Targeting: units and tiles light up as targets; a ship target lights its whole deck.
	TArray<FIntPoint> Targets;
	TArray<FIntPoint> Area;
	const bool bGroup = ArmedAbility && DwInteraction::IsGroup(FDwCombatQueries::GetAbilityCDO(ArmedAbility)->GetTargetingRules(
		MakeContext(Snapshot, ArmedAbility, ArmedItem)).Pattern);
	for (const FDwAbilityTarget& Target : ValidTargets)
	{
		if (Target.bShip)
		{
			for (int32 Index = 0; Index < Snapshot.Grid.Tiles.Num(); ++Index)
			{
				if (Snapshot.Grid.Tiles[Index].Owner == Target.ShipSide && Snapshot.Grid.IsStandable(Snapshot.Grid.ToTile(Index)))
				{
					Area.Add(Snapshot.Grid.ToTile(Index));
				}
			}
		}
		else
		{
			(bGroup ? Area : Targets).Add(Target.Tile);
		}
	}
	Board->ShowHighlights(EDwHighlightLayer::Target, Targets);
	Board->ShowHighlights(EDwHighlightLayer::AreaOfEffect, Area);
}

// ---------------------------------------------------------------------------------------------------------------------
// Action bar and combat events

TArray<FDwActionSlot> UDwCombatInteraction::GetActionBar() const
{
	TArray<FDwActionSlot> Bar;
	if (!Combat || !Combat->IsActive())
	{
		return Bar;
	}
	const FDwCombatSnapshot Snapshot = Combat->GetSnapshot();
	const FDwUnitSnapshot* Unit = Snapshot.FindUnit(SelectedUnit);
	if (!Unit)
	{
		return Bar;
	}
	auto AddSlot = [&](const UDwAbilityDefinition* Ability, FName ItemId, const FText& Label, int32 Count)
	{
		FDwActionSlot& Slot = Bar.AddDefaulted_GetRef();
		Slot.Ability = Ability;
		Slot.ItemId = ItemId;
		Slot.Label = Label;
		Slot.Count = Count;
		FText Reason;
		if (FDwCombatQueries::CanActivate(Snapshot, *Unit, Ability, ItemId, &Reason) != EDwCommandResult::Accepted)
		{
			Slot.Reason = Reason;
			return;
		}
		TArray<FDwAbilityTarget> Targets;
		FDwCombatQueries::GetAbilityCDO(Ability)->GatherValidTargets(MakeContext(Snapshot, Ability, ItemId), Targets);
		Slot.bAvailable = !Targets.IsEmpty();
		if (!Slot.bAvailable)
		{
			Slot.Reason = Ability->AbilityTag == DwTags::Ability_Cannon ? UDwGA_FireCannon::GetNoCannonReason() : LOCTEXT("NoTargetSlot", "No target in reach");
		}
	};

	for (const UDwAbilityDefinition* Ability : Unit->Abilities)
	{
		if (!Ability || Ability->AbilityTag == DwTags::Ability_Move || !FDwCombatQueries::GetAbilityCDO(Ability))
		{
			continue;
		}
		if (Ability->AbilityTag != DwTags::Ability_Item)
		{
			AddSlot(Ability, NAME_None, Ability->DisplayName, 0);
			continue;
		}
		TArray<FName> ItemIds;
		Snapshot.Items.GetKeys(ItemIds);
		ItemIds.Sort(FNameLexicalLess());
		for (const FName ItemId : ItemIds)
		{
			const int32 Count = Snapshot.Items.FindRef(ItemId);
			const UDwItemDefinition* Item = Snapshot.FindItem(ItemId);
			if (Count > 0 && Item)
			{
				AddSlot(Ability, ItemId, Item->DisplayName, Count);
			}
		}
	}
	return Bar;
}

void UDwCombatInteraction::HandleCommandResolved(const FDwCombatCommand& /*Command*/)
{
	Refresh();
}

void UDwCombatInteraction::HandleCombatEnded(const FDwCombatResult& /*Result*/)
{
	Disarm();
	Mode = EDwInteractionMode::Idle;
	ClearHighlights();
	OnChanged.Broadcast();
}

void UDwCombatInteraction::HandleEndTurnSuggested()
{
	bEndTurnSuggested = true;
	OnChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE
