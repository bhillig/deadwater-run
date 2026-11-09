// Copyright Brandon Hillig 2026

#include "Core/DwMenuGameMode.h"

#include "Core/DwPlayerController.h"

ADwMenuGameMode::ADwMenuGameMode()
{
	PlayerControllerClass = ADwPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}
