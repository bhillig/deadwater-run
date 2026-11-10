// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/DwTypes.h"
#include "DwCombatSideController.generated.h"

class UDwCombatSubsystem;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDwCombatSideController : public UInterface
{
	GENERATED_BODY()
};

/**
 * Drives one side of a fight by submitting FDwCombatCommand to UDwCombatSubsystem (Docs/Architecture/03 D1).
 * Implementations: ADwPlayerController (DW-105), UDwAiSideController (DW-106), UDwSimPlayerSideController (DW-212),
 * and UDwPassiveSideController (ends every unit; the enemy default when no enemy has an AI profile).
 *
 * Calls arrive from the turn manager's step loop, never from inside SubmitCommand, so a controller may submit
 * commands synchronously. The "loop until latent" pattern: keep submitting while !Combat.IsResolving(); when an ability
 * plays out over time, return and continue in OnCommandResolved.
 */
class DEADWATERRUN_API IDwCombatSideController
{
	GENERATED_BODY()

public:
	/** Side's turn started (budgets refreshed, durations ticked). The player side then acts in any order until EndTurn. */
	virtual void BeginSideTurn(UDwCombatSubsystem& Combat, EDwSide Side) = 0;

	/** Enemy-style sides: it is Unit's turn in the fixed sequence; finish it with an EndUnit command. */
	virtual void BeginUnitTurn(UDwCombatSubsystem& Combat, FDwUnitHandle Unit) {}

	/** A command of this side that resolved latently (after SubmitCommand had returned). */
	virtual void OnCommandResolved(UDwCombatSubsystem& Combat) {}
};
