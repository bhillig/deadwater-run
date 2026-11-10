// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Combat/DwCombatTypes.h"
#include "Run/DwRunTypes.h"
#include "DwGameMode.generated.h"

class UDwRun;
class UDwSeaMood;

/**
 * Game mode for L_Sea, the level that hosts the whole run (map, combat, shop, events are phases inside it).
 * Docs/Architecture/16-Shell.md. The phase bridge: when the active UDwRun enters Combat it builds the node's encounter
 * and starts it; when the fight ends it gives the result back to the run (after a short beat so the outcome reads) and
 * clears the board.
 * Without a run, L_Sea starts one from -DwCaptain=<name> [-DwSeed=<n>], else DA_GameData's dev fight (bStartDevFight).
 */
UCLASS()
class DEADWATERRUN_API ADwGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADwGameMode();

	virtual void StartPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Starts DA_GameData's dev fight with a fresh seed. False (logged) if the data is missing. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Dev")
	bool StartDevFight(int32 Seed = -1);

	/**
	 * Restyles the level's ADwSeaEnvironment for a fight: a weighted roll over DA_GameData.SeaMoods that depends only on
	 * Seed, or the mood named by -DwMood= (dev). Returns the mood applied, or null without moods.
	 */
	const UDwSeaMood* ApplySeaMood(int32 Seed);

	/** Without a run and without -DwCaptain: start the dev fight when L_Sea opens. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater|Dev")
	bool bStartDevFight = true;

	/** Seconds between a fight's outcome and returning to the map (the HUD shows the result). 0 in instant mode. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater|Run", meta = (ClampMin = "0"))
	float PostCombatSeconds = 2.f;

private:
	UFUNCTION() void HandleRunStarted(UDwRun* Run);
	UFUNCTION() void HandleRunPhaseChanged(EDwRunPhase NewPhase, EDwRunPhase OldPhase);
	UFUNCTION() void HandleRunCombatEnded(const FDwCombatResult& Result);

	void BindRun(UDwRun* Run);
	void UnbindRun();
	/** Builds and starts the current node's fight. False (logged) if the run's content can't make one. */
	bool StartRunEncounter();
	void FinishRunEncounter();

	UPROPERTY() TObjectPtr<UDwRun> BoundRun;
	FDwCombatResult PendingResult;
	FTimerHandle PostCombatTimer;
	bool bRunEncounterActive = false;
};
