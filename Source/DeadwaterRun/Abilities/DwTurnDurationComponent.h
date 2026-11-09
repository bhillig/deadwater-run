// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectComponent.h"
#include "Core/DwTypes.h"
#include "DwTurnDurationComponent.generated.h"

/**
 * Makes an Infinite GameplayEffect last N turns instead of seconds (Docs/Architecture/02 "Turn-based durations").
 * UDwAbilitySystemComponent counts it down at the turn boundaries the turn manager announces; nothing ticks.
 * Convention (C15): "for one turn" = Turns 1, TickPoint OwnerSideTurnEnd.
 */
UCLASS(DisplayName = "Lasts N Turns (Deadwater)")
class DEADWATERRUN_API UDwTurnDurationComponent : public UGameplayEffectComponent
{
	GENERATED_BODY()

public:
	/** Overridden by SetByCaller Data.Turns when the spec sets a positive value. */
	UPROPERTY(EditDefaultsOnly, Category = "Turns", meta = (ClampMin = "1"))
	int32 Turns = 1;

	/** Which boundary of the owner's side turn counts down. */
	UPROPERTY(EditDefaultsOnly, Category = "Turns")
	EDwTurnTickPoint TickPoint = EDwTurnTickPoint::OwnerSideTurnEnd;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
