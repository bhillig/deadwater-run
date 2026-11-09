// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DwGameMode.generated.h"

/**
 * Game mode for L_Sea, the level that hosts the whole run (map, combat, shop, events are phases inside it).
 * Stub for DW-002. DW-201 binds UDwRun::OnPhaseChanged here and starts/ends encounters.
 * See Docs/Architecture/16-Shell.md.
 */
UCLASS()
class DEADWATERRUN_API ADwGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADwGameMode();
};
