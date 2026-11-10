// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Combat/DwCombatSideController.h"
#include "DwPassiveSideController.generated.h"

/**
 * A side that never acts: every unit ends at once and the player side ends its turn at once.
 * The enemy default when no enemy has an AI profile (otherwise UDwAiSideController, DW-106); also useful in tests that
 * only exercise one side.
 */
UCLASS()
class DEADWATERRUN_API UDwPassiveSideController : public UObject, public IDwCombatSideController
{
	GENERATED_BODY()

public:
	virtual void BeginSideTurn(UDwCombatSubsystem& Combat, EDwSide Side) override;
	virtual void BeginUnitTurn(UDwCombatSubsystem& Combat, FDwUnitHandle Unit) override;
};
