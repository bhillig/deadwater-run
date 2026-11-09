// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DwMenuGameMode.generated.h"

/**
 * Game mode for L_MainMenu. Stub for DW-002.
 * DW-210 shows the main menu through the UI manager; DW-201 ends any stale run here.
 * See Docs/Architecture/16-Shell.md.
 */
UCLASS()
class DEADWATERRUN_API ADwMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADwMenuGameMode();
};
