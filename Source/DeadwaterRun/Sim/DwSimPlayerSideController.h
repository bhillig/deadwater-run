// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AI/DwAiTypes.h"
#include "Combat/DwCombatSideController.h"
#include "DwSimPlayerSideController.generated.h"

class UDwAiProfile;
class UDwCrewAiBrain;
struct FDwEncounterSetup;

/**
 * Plays the player's side in the headless simulator (Docs/Architecture/17): at each player turn it walks the living crew
 * in handle order, asks UDwCrewAiBrain (with the policy's profile) for each pirate's commands and submits them through
 * UDwCombatSubsystem::SubmitCommand, the same validated path as clicks, then ends the turn. Its random stream comes
 * from the encounter seed, so a fight replays from seed + commands. Owner: tools-engineer.
 */
UCLASS()
class DEADWATERRUN_API UDwSimPlayerSideController : public UObject, public IDwCombatSideController
{
	GENERATED_BODY()

public:
	/** Call before each fight: a brain for Profile configured with Setup, the stream from Setup.Seed, counters reset. */
	void Configure(const UDwAiProfile& Profile, const FDwEncounterSetup& Setup, int32 InMaxRounds);

	/** Accepted Move and Ability commands of the crew this fight (the estimate counts them). */
	int32 GetActions() const { return Actions; }
	/** Commands the subsystem refused (a brain/rules mismatch: always a bug). */
	int32 GetRejected() const { return Rejected; }
	/** The fight reached MaxRounds and the controller stopped playing (the driver reports the run as stuck). */
	bool HitRoundLimit() const { return bHitRoundLimit; }

	// IDwCombatSideController
	virtual void BeginSideTurn(UDwCombatSubsystem& Combat, EDwSide Side) override;
	virtual void OnCommandResolved(UDwCombatSubsystem& Combat) override;

private:
	/** Plays the queued crew until the queue is empty (then ends the turn) or an ability resolves latently. */
	void Act(UDwCombatSubsystem& Combat);

	UPROPERTY() TObjectPtr<UDwCrewAiBrain> Brain;

	FRandomStream Random;
	TArray<FDwUnitHandle> Queue;
	FDwAiTurnContext TurnContext;
	int32 MaxRounds = 40;
	int32 Actions = 0;
	int32 Rejected = 0;
	bool bHitRoundLimit = false;
	bool bTurnOpen = false;
};
