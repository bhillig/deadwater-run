// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DwHUD.generated.h"

/**
 * L_Sea's AHUD. Every player-facing screen is CommonUI (UDwUIManagerSubsystem); this draws nothing in Shipping.
 * Development builds draw a text readout of the active run outside combat - phase, crew, ship, gold, the numbered
 * routes for Dw.Run.Choose - so the run is playable and reviewable before the map and summary screens exist
 * (DW-210, DW-211). Hidden with dw.Run.Readout 0.
 */
UCLASS()
class DEADWATERRUN_API ADwHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
#if !UE_BUILD_SHIPPING
	void DrawRunReadout();
#endif
};
