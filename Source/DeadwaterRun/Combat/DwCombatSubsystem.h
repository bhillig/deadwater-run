// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayAbilitySpecHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/DwCombatSnapshot.h"
#include "Combat/DwCombatTypes.h"
#include "DwCombatSubsystem.generated.h"

class ADwBoard;
class ADwCombatUnit;
class ADwShip;
class IDwCombatSideController;
class UDwAbilitySystemComponent;
class UDwPassiveSideController;
class UDwTurnManager;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDwOnCombatEvent, const FDwCombatEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDwOnCommandResolved, const FDwCombatCommand&, Command);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDwOnCombatEnded, const FDwCombatResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDwOnCombatSignal);

/**
 * Owns one fight (Docs/Architecture/03-Combat-Turns.md D1/D2): the grid, the unit registry, the turn manager and the
 * single validated entry point for every actor - player input, AI, tests and the simulator all call SubmitCommand.
 * GAS owns attributes and tags; GetSnapshot() copies everything into plain data for rules, AI and UI.
 * Exists in any game world, including the simulator's transient one. Never ticks.
 */
UCLASS()
class DEADWATERRUN_API UDwCombatSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- Lifecycle -------------------------------------------------------------------------------------------------

	/**
	 * Builds the board, spawns ships and units, initialises GAS (Health from setup, Mana = MaxMana, C9), seeds the
	 * combat random stream and starts round 1 on the player side. Returns false (and logs why) on bad setup data;
	 * nothing is left spawned in that case.
	 */
	bool StartEncounter(const FDwEncounterSetup& Setup);

	/** Destroys spawned actors. Call after OnCombatEnded listeners have read the result. */
	void EndEncounter();

	bool IsActive() const { return bActive; }

	// --- Commands --------------------------------------------------------------------------------------------------

	/**
	 * Validates and executes one command (FDwCombatQueries::ValidateCommand plus "busy"). Accepted commands are final
	 * (no undo, C13) and appended to the record. OutReason gets a player-facing reason on rejection.
	 */
	EDwCommandResult SubmitCommand(const FDwCombatCommand& Command, FText* OutReason = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Deadwater|Combat", meta = (DisplayName = "Submit Command"))
	EDwCommandResult K2_SubmitCommand(const FDwCombatCommand& Command) { return SubmitCommand(Command); }

	/** An ability is playing out; every command is refused with Busy until it ends. */
	UFUNCTION(BlueprintPure, Category = "Deadwater|Combat")
	bool IsResolving() const { return bResolving; }

	/** The controller for a side. Null player side = commands come from outside (UI, tests). Null enemy side = passive. */
	void SetSideController(EDwSide Side, TScriptInterface<IDwCombatSideController> Controller);
	IDwCombatSideController* GetSideController(EDwSide Side) const;

	// --- Queries ---------------------------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Deadwater|Combat")
	FDwCombatSnapshot GetSnapshot() const;

	const FDwGridModel& GetGrid() const { return Grid; }
	const FDwGridRules& GetGridRules() const { return GridRules; }
	FRandomStream& GetRandom() { return Random; }

	UFUNCTION(BlueprintPure, Category = "Deadwater|Combat")
	ADwCombatUnit* FindUnit(FDwUnitHandle Handle) const;
	ADwCombatUnit* FindUnitByAbilitySystem(const UDwAbilitySystemComponent* AbilitySystem) const;
	const TArray<TObjectPtr<ADwCombatUnit>>& GetUnits() const { return Units; }

	UFUNCTION(BlueprintPure, Category = "Deadwater|Combat")
	ADwShip* GetShip(EDwSide Side) const;

	UFUNCTION(BlueprintPure, Category = "Deadwater|Combat")
	ADwBoard* GetBoard() const { return Board; }

	/** The actor an ability target names: a unit, a ship, or nullptr for a tile. */
	AActor* GetTargetActor(const FDwAbilityTarget& Target) const;

	UFUNCTION(BlueprintPure, Category = "Deadwater|Combat") int32 GetRound() const;
	UFUNCTION(BlueprintPure, Category = "Deadwater|Combat") EDwSide GetActiveSide() const;
	UFUNCTION(BlueprintPure, Category = "Deadwater|Combat") EDwCombatOutcome GetOutcome() const { return Outcome; }
	const FDwCombatResult& GetResult() const { return Result; }
	const FDwCombatRecord& GetRecord() const { return Record; }
	const FDwEncounterSetup& GetSetup() const { return Setup; }

	// --- Called by abilities (DW-104) ------------------------------------------------------------------------------

	/** Moves a unit on the grid (logic is instant; presentation animates separately) and emits UnitMoved. */
	void MoveUnit(FDwUnitHandle Unit, FIntPoint Tile);
	/** Marks a cannon tile as fired for the rest of this side turn (C7). */
	void MarkCannonUsed(FIntPoint CannonTile);
	/** Spends one item from the run inventory view. False if none left. */
	bool ConsumeItem(FName ItemId);

	// --- Called by the turn manager --------------------------------------------------------------------------------

	void HandleSideTurnBegin(EDwSide Side);
	void HandleSideTurnEnd(EDwSide Side);
	/** Cower check and skip rules at an enemy's turn start. False = the unit does not act this turn. */
	bool BeginEnemyUnitTurn(FDwUnitHandle Unit);

	// --- Events ----------------------------------------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Deadwater|Combat") FDwOnCombatEvent OnCombatEvent;
	/** Every accepted Move/Ability once it has fully resolved, plus EndUnit and EndTurn. */
	UPROPERTY(BlueprintAssignable, Category = "Deadwater|Combat") FDwOnCommandResolved OnCommandResolved;
	UPROPERTY(BlueprintAssignable, Category = "Deadwater|Combat") FDwOnCombatEnded OnCombatEnded;
	UPROPERTY(BlueprintAssignable, Category = "Deadwater|Combat") FDwOnCombatSignal OnCombatStarted;
	/** No player unit has anything left to do: the UI asks "End turn?" (C14). Never ends the turn itself. Once per turn. */
	UPROPERTY(BlueprintAssignable, Category = "Deadwater|Combat") FDwOnCombatSignal OnEndTurnSuggested;

	// UWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	bool SpawnFight();
	ADwCombatUnit* SpawnUnit(const FDwCombatantSetup& Combatant, EDwSide Side, FIntPoint Tile);
	ADwShip* SpawnShip(EDwSide Side, const FDwShipSetup& ShipSetup);
	void BindUnit(ADwCombatUnit& Unit);
	void BindShip(ADwShip& Ship);

	void HandleAbilityEnded(UDwAbilitySystemComponent* Owner, FGameplayAbilitySpecHandle Ability);
	void HandleHealthDepleted(UDwAbilitySystemComponent* Victim, AActor* Instigator);
	void HandleUnitDied(ADwCombatUnit& Victim, AActor* Instigator);
	void HandleShipSunk(ADwShip& Ship);
	void HandleUnitHealthChanged(const FOnAttributeChangeData& Data, FDwUnitHandle Unit);
	void HandleUnitFearChanged(const FOnAttributeChangeData& Data, FDwUnitHandle Unit);
	void HandleShipHealthChanged(const FOnAttributeChangeData& Data, EDwSide Side);
	void HandleUnitTagChanged(const FGameplayTag Tag, int32 NewCount, FDwUnitHandle Unit);

	void FinishCommand();
	void RefreshDerivedState();
	/** Returns true if this call ended the fight. */
	bool EvaluateOutcome();
	/** EvaluateOutcome outside a command (turn steps); broadcasts OnCombatEnded if the fight ended. */
	void CheckOutcome();
	void BuildResult();
	void SuggestEndTurnIfIdle();
	void Emit(const FDwCombatEvent& Event);
	FActiveGameplayEffectHandle ApplyEffectToSelf(UDwAbilitySystemComponent& Target, TSubclassOf<UGameplayEffect> EffectClass, bool bNoInstigator = false);

	UPROPERTY() TObjectPtr<UDwTurnManager> TurnManager;
	UPROPERTY() TObjectPtr<ADwBoard> Board;
	UPROPERTY() TArray<TObjectPtr<ADwCombatUnit>> Units; // Units[Handle.Id - 1]
	UPROPERTY() TObjectPtr<ADwShip> PlayerShip;
	UPROPERTY() TObjectPtr<ADwShip> EnemyShip;
	UPROPERTY() TScriptInterface<IDwCombatSideController> PlayerController;
	UPROPERTY() TScriptInterface<IDwCombatSideController> EnemyController;
	UPROPERTY() TObjectPtr<UDwPassiveSideController> DefaultEnemyController;

	FDwEncounterSetup Setup;
	FDwGridModel Grid;
	FDwGridRules GridRules;
	FRandomStream Random;
	FDwCombatRecord Record;
	FDwCombatResult Result;
	EDwCombatOutcome Outcome = EDwCombatOutcome::None;
	int32 PlayerCrewCount = 0;

	TArray<FIntPoint> CannonsUsed;
	TMap<FName, int32> Items;
	TMap<FName, int32> ItemsUsed;
	TArray<FDwKillRecord> Kills;
	TMap<FDwUnitHandle, FActiveGameplayEffectHandle> CoweringEffects;

	bool bActive = false;
	bool bResolving = false;
	FDwCombatCommand ResolvingCommand;
	FDwUnitHandle ResolvingUnit;
	FGameplayAbilitySpecHandle ResolvingSpec;
	/** > 0 while inside SubmitCommand: synchronous resolution is reported to the caller, not to its controller. */
	int32 SubmitDepth = 0;
	bool bEndTurnSuggested = false;
};
