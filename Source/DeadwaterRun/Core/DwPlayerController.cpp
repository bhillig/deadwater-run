// Copyright Brandon Hillig 2026

#include "Core/DwPlayerController.h"

ADwPlayerController::ADwPlayerController()
{
	// Turn-based, mouse-driven game: cursor always visible, click events on for tile picking.
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}
