// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Run/DwRunTypes.h"
#include "DwRunTestListener.generated.h"

/** Test-only: records UDwRun::OnPhaseChanged / OnStateChanged broadcasts for DeadwaterRun.*.Run specs. */
UCLASS(NotBlueprintable, HideDropdown)
class DEADWATERRUN_API UDwRunTestListener : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION() void HandlePhaseChanged(EDwRunPhase NewPhase, EDwRunPhase OldPhase)
	{
		Phases.Add(TPair<EDwRunPhase, EDwRunPhase>(NewPhase, OldPhase));
	}

	UFUNCTION() void HandleStateChanged() { ++StateChanges; }

	TArray<TPair<EDwRunPhase, EDwRunPhase>> Phases;
	int32 StateChanges = 0;
};
