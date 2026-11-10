// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/DwTypes.h"
#include "DwUITestListener.generated.h"

/** Test-only: records UDwCombatHudViewModel::OnTurnBanner broadcasts for DeadwaterRun.UI.* specs. */
UCLASS(NotBlueprintable, HideDropdown)
class DEADWATERRUN_API UDwUITestListener : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION() void HandleTurnBanner(EDwSide Side, int32 Round)
	{
		Banners.Add(TPair<EDwSide, int32>(Side, Round));
	}

	TArray<TPair<EDwSide, int32>> Banners;
};
