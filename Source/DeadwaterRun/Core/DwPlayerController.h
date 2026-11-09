// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DwPlayerController.generated.h"

/**
 * The single local player's controller (mouse + keyboard).
 * Stub for DW-002. DW-105 implements IDwCombatSideController, Enhanced Input (IMC_Combat) and UDwCombatInteraction.
 * See Docs/Architecture/03-Combat-Turns.md.
 */
UCLASS()
class DEADWATERRUN_API ADwPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADwPlayerController();
};
