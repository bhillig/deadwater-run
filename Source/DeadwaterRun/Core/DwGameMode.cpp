// Copyright Brandon Hillig 2026

#include "Core/DwGameMode.h"

#include "Core/DwPlayerController.h"

ADwGameMode::ADwGameMode()
{
	PlayerControllerClass = ADwPlayerController::StaticClass();

	// Units are board actors driven by commands, not possessed pawns; the camera rig arrives in DW-105.
	DefaultPawnClass = nullptr;
}
