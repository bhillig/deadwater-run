// Copyright Brandon Hillig 2026

#include "Combat/DwPassiveSideController.h"

#include "Combat/DwCombatSubsystem.h"

void UDwPassiveSideController::BeginSideTurn(UDwCombatSubsystem& Combat, EDwSide Side)
{
	// Enemy-style sides act unit by unit (BeginUnitTurn); the player side has no sequence, so it ends here.
	if (Side == EDwSide::Player)
	{
		Combat.SubmitCommand(FDwCombatCommand::MakeEndTurn(Side));
	}
}

void UDwPassiveSideController::BeginUnitTurn(UDwCombatSubsystem& Combat, FDwUnitHandle Unit)
{
	Combat.SubmitCommand(FDwCombatCommand::MakeEndUnit(Unit));
}
