// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DwMenuGameMode.generated.h"

/**
 * Game mode for L_MainMenu. The UI manager shows the main menu here (DW-210); any run left over is cleared on arrival.
 * See Docs/Architecture/16-Shell.md.
 */
UCLASS()
class DEADWATERRUN_API ADwMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADwMenuGameMode();

protected:
	virtual void BeginPlay() override;
};
