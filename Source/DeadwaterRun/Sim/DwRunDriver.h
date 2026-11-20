// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Sim/DwSimRunReport.h"
#include "DwRunDriver.generated.h"

class UDwCaptainDefinition;
class UDwGameData;
class UDwRun;
class UDwSimPlayerSideController;
class UDwSimPolicy;
class UWorld;
struct FDwEncounterSetup;

/**
 * Plays whole runs headless (Docs/Architecture/17 "The simulator"): the real UDwRun, the real UDwCombatSubsystem and GAS
 * in a transient game world with instant presentation. The player's crew is played by UDwSimPlayerSideController, the
 * enemies by their own AI profiles; every choice outside fights comes from a UDwSimPolicy. A run that stops making
 * progress (a refused step, a fight that never ends, the step cap) ends as "Stuck" with the reason in the report.
 * Runtime code, so the packaged autoplay smoke (DW-407) can use it. Owner: tools-engineer.
 */
UCLASS()
class DEADWATERRUN_API UDwRunDriver : public UObject
{
	GENERATED_BODY()

public:
	/** Creates the transient world (once; PlayRun calls it). The owner must call Shutdown before letting the driver go. */
	void Initialize();
	void Shutdown();

	/**
	 * Plays one run to Victory, Defeat or Stuck and returns its report. Deterministic: the same content, captain, seed
	 * and policy give the same report.
	 */
	FDwSimRunReport PlayRun(const UDwGameData& GameData, const UDwCaptainDefinition& Captain, int32 Seed, const UDwSimPolicy& Policy);

	/**
	 * Spawns fights with the plain C++ board, ship and unit classes instead of the art Blueprints (default on). The art
	 * classes only add meshes and animation, so results are the same; the simulator just doesn't pay to spawn them.
	 */
	bool bPlainActors = true;

	/** Phase steps before a run counts as stuck. A full voyage takes well under 200. */
	int32 MaxSteps = 600;

	/** Captain short name: DA_Captain_TeamLeader -> TeamLeader. */
	static FString CaptainName(const UDwCaptainDefinition& Captain);

private:
	bool PlayFight(const UDwSimPolicy& Policy, FDwSimRunReport& Report, FString& OutStuck);
	bool PlayShop(const UDwSimPolicy& Policy, FDwSimRunReport& Report, FString& OutStuck);
	bool PlayEvent(FString& OutStuck);
	bool PlayRewards(const UDwSimPolicy& Policy, FString& OutStuck);
	bool PlayLevelUp(FString& OutStuck);
	bool ChooseNode(const UDwSimPolicy& Policy, FString& OutStuck);
	void StripArt(FDwEncounterSetup& Setup) const;
	void FinishReport(const UDwSimPolicy& Policy, FDwSimRunReport& Report) const;

	UPROPERTY() TObjectPtr<UWorld> World;
	UPROPERTY() TObjectPtr<UDwRun> Run;
	UPROPERTY() TObjectPtr<UDwSimPlayerSideController> PlayerSide;

	/** The policy's own stream (route and perk picks), seeded from the run seed. */
	FRandomStream PolicyRandom;
};
