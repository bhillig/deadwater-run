// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DwShip.generated.h"

/**
 * A ship in a fight. Declared in DW-004 so encounter definitions and UDwGameData can reference BP_Ship_* classes.
 * DW-103 adds the ASC + UDwShipAttributeSet, deck and cannon tiles (Docs/Architecture/06-Ship-Cannons.md).
 */
UCLASS(Abstract, Blueprintable)
class DEADWATERRUN_API ADwShip : public AActor
{
	GENERATED_BODY()

public:
	ADwShip();
};
