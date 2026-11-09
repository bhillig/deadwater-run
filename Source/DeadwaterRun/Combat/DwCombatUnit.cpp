// Copyright Brandon Hillig 2026

#include "Combat/DwCombatUnit.h"

ADwCombatUnit::ADwCombatUnit()
{
	// Turn logic never ticks (architecture rule 4); presentation components enable their own tick if needed.
	PrimaryActorTick.bCanEverTick = false;
}
