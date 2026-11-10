// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Run/DwRunTypes.h"
#include "DwRunSubsystem.generated.h"

class UDwCaptainDefinition;
class UDwEncounterDefinition;
class UDwRun;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDwOnRunStarted, UDwRun*, Run);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDwOnRunCleared);

/**
 * Holds the active UDwRun for the game (Docs/Architecture/09 D1, 16-Shell.md). A game-instance subsystem, so the run
 * survives the menu -> L_Sea travel. No saving: quitting drops the run (Scope: no mid-run saves).
 * The headless simulator never uses this; it creates UDwRun directly.
 */
UCLASS()
class DEADWATERRUN_API UDwRunSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UDwRunSubsystem* Get(const UObject* WorldContext);

	/**
	 * Set Sail: replaces any run with a new one for Captain (Seed < 0 = from the clock), then opens DA_GameData.SeaLevel
	 * unless the current world already is it. Null (logged) if the content is missing.
	 */
	UDwRun* StartRun(const UDwCaptainDefinition* Captain, int32 Seed = -1, bool bOpenSeaLevel = true);

	UFUNCTION(BlueprintCallable, Category = "Deadwater|Run", meta = (DisplayName = "Start Run"))
	UDwRun* K2_StartRun(const UDwCaptainDefinition* Captain, int32 Seed = -1) { return StartRun(Captain, Seed); }

	UFUNCTION(BlueprintPure, Category = "Deadwater|Run")
	UDwRun* GetRun() const { return Run; }

	/** Abandon (U5): the run ends as Defeat (Abandoned) and stays readable for the summary until ClearRun. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Run")
	bool AbandonRun();

	/** The summary is closed (or a new run starts): drops the run. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Run")
	void ClearRun();

	/** A captain from DA_GameData.Captains by asset name ("DA_Captain_TeamLeader") or short name ("TeamLeader"). */
	static const UDwCaptainDefinition* FindCaptain(const FString& Name);

	/** Every ship-battle encounter the Asset Manager knows, sorted by name so map picks are stable for a seed. */
	static TArray<const UDwEncounterDefinition*> GatherEncounterPool();

	UPROPERTY(BlueprintAssignable, Category = "Deadwater|Run") FDwOnRunStarted OnRunStarted;
	UPROPERTY(BlueprintAssignable, Category = "Deadwater|Run") FDwOnRunCleared OnRunCleared;

	// UGameInstanceSubsystem
	virtual void Deinitialize() override;

private:
	UFUNCTION() void HandlePhaseChanged(EDwRunPhase NewPhase, EDwRunPhase OldPhase);

	UPROPERTY() TObjectPtr<UDwRun> Run;
	double StartSeconds = 0.0;
};
