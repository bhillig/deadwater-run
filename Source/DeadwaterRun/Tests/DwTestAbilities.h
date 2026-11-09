// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/DwGameplayAbility.h"
#include "Combat/DwCombatSideController.h"
#include "Combat/DwCombatTypes.h"
#include "DwTestAbilities.generated.h"

/**
 * Test-only ability: commits (costs), then applies the tuned shared GE_Damage to TriggerEventData.Target and ends.
 * Used by DeadwaterRun.Gas.* to check costs, budgets and "preview == result". Not content: hidden from pickers.
 * The real workhorse is UDwGA_EffectPayload (DW-104).
 */
UCLASS(NotBlueprintable, HideDropdown, meta = (HiddenNode))
class DEADWATERRUN_API UDwTestAbility_Strike : public UDwGameplayAbility
{
	GENERATED_BODY()

public:
	UDwTestAbility_Strike();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** Test-only: the strike aimed at the enemy ship (pattern EnemyShip). Sinking tests. */
UCLASS(NotBlueprintable, HideDropdown, meta = (HiddenNode))
class DEADWATERRUN_API UDwTestAbility_Broadside : public UDwTestAbility_Strike
{
	GENERATED_BODY()

public:
	UDwTestAbility_Broadside();
};

/** Test-only: commits and stays active until cancelled, like an ability waiting on its animation. "Busy" tests. */
UCLASS(NotBlueprintable, HideDropdown, meta = (HiddenNode))
class DEADWATERRUN_API UDwTestAbility_Latent : public UDwGameplayAbility
{
	GENERATED_BODY()

public:
	UDwTestAbility_Latent();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** Test-only side controller: runs a script for each unit turn (then ends the unit) and optionally for the side turn. */
UCLASS(NotBlueprintable, HideDropdown)
class DEADWATERRUN_API UDwTestScriptedController : public UObject, public IDwCombatSideController
{
	GENERATED_BODY()

public:
	TFunction<void(UDwCombatSubsystem&, EDwSide)> OnSideTurn;
	TFunction<void(UDwCombatSubsystem&, FDwUnitHandle)> OnUnitTurn;
	int32 UnitTurns = 0;
	int32 SideTurns = 0;
	/** false: the unit turn waits for commands from outside (replays). */
	bool bAutoEndUnit = true;

	virtual void BeginSideTurn(UDwCombatSubsystem& Combat, EDwSide Side) override;
	virtual void BeginUnitTurn(UDwCombatSubsystem& Combat, FDwUnitHandle Unit) override;
};

/** Test-only: records everything UDwCombatSubsystem broadcasts (dynamic delegates need UFUNCTION targets). */
UCLASS(NotBlueprintable, HideDropdown)
class DEADWATERRUN_API UDwTestCombatListener : public UObject
{
	GENERATED_BODY()

public:
	void Bind(UDwCombatSubsystem& Combat);
	void Reset();

	int32 Count(EDwCombatEventType Type, FDwUnitHandle Target = FDwUnitHandle()) const;

	TArray<FDwCombatEvent> Events;
	TArray<FDwCombatCommand> Resolved;
	TArray<FDwCombatResult> Ended;
	int32 EndTurnSuggestions = 0;

	UFUNCTION() void HandleEvent(const FDwCombatEvent& Event) { Events.Add(Event); }
	UFUNCTION() void HandleResolved(const FDwCombatCommand& Command) { Resolved.Add(Command); }
	UFUNCTION() void HandleEnded(const FDwCombatResult& Result) { Ended.Add(Result); }
	UFUNCTION() void HandleEndTurnSuggested() { ++EndTurnSuggestions; }
};
