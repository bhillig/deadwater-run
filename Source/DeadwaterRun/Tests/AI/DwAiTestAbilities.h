// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/DwGA_EffectPayload.h"
#include "DwAiTestAbilities.generated.h"

/** Test-only rifle for DeadwaterRun.AI.*: single enemy, needs line of sight, GE_Damage (range from tuning). */
UCLASS(NotBlueprintable, HideDropdown, meta = (HiddenNode))
class DEADWATERRUN_API UDwAiTestPayload_Rifle : public UDwGA_EffectPayload
{
	GENERATED_BODY()
public:
	UDwAiTestPayload_Rifle();
};

/** Test-only heal: single ally (self included), GE_Heal with Heal from tuning. */
UCLASS(NotBlueprintable, HideDropdown, meta = (HiddenNode))
class DEADWATERRUN_API UDwAiTestPayload_Heal : public UDwGA_EffectPayload
{
	GENERATED_BODY()
public:
	UDwAiTestPayload_Heal();
};
