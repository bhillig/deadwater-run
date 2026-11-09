// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DwCombatUnit.generated.h"

/**
 * A combatant on the board (crew, enemy pirate, Kraken body, tentacle). Declared in DW-004 so unit definitions can
 * reference BP_Unit_* classes. DW-103 adds the ASC, attribute set and spawn/write-back
 * (Docs/Architecture/05-Crew-Captains.md).
 */
UCLASS(Abstract, Blueprintable)
class DEADWATERRUN_API ADwCombatUnit : public AActor
{
	GENERATED_BODY()

public:
	ADwCombatUnit();
};
