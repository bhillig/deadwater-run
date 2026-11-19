// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AI/DwAiTypes.h"
#include "Combat/DwCombatSideController.h"
#include "DwAiSideController.generated.h"

class UDwAiBrain;
class UDwAiProfile;
struct FDwEncounterSetup;

/**
 * Drives the enemy side (Docs/Architecture/07-Enemy-AI.md, 03 "side controller contract"). For each unit the turn
 * manager hands it, it asks that unit's brain for the next command and submits it through UDwCombatSubsystem::SubmitCommand
 * - the same validated path the player's clicks take - until the brain ends the unit ("loop until latent": when an ability
 * plays out over time it returns, and continues in OnCommandResolved).
 *
 * Randomness: brains draw from this controller's own stream, seeded from the encounter seed. It never touches the
 * combat stream, so a recorded fight still replays from seed + commands alone (Docs/Architecture/03 "Combat random").
 */
UCLASS()
class DEADWATERRUN_API UDwAiSideController : public UObject, public IDwCombatSideController
{
	GENERATED_BODY()

public:
	/**
	 * A controller for every enemy whose FDwCombatantSetup carries an AI profile: one brain per profile, of the profile's BrainClass
	 * (UDwCrewAiBrain when empty), configured with the setup.
	 * EnemyHandles are the enemies' handles in Setup.Enemies order. Null if no enemy has a profile.
	 */
	static UDwAiSideController* CreateForEncounter(UObject* Outer, const FDwEncounterSetup& Setup, TConstArrayView<FDwUnitHandle> EnemyHandles);

	/** Assigns (or, with nullptr, removes) the brain that decides for Unit. */
	void SetBrain(FDwUnitHandle Unit, UDwAiBrain* Brain);
	UDwAiBrain* GetBrain(FDwUnitHandle Unit) const;

	/** Reseeds the AI stream (CreateForEncounter derives it from Setup.Seed). */
	void SetSeed(int32 EncounterSeed);

	// IDwCombatSideController
	virtual void OnCombatStarted(UDwCombatSubsystem& Combat) override;
	virtual void OnSideTurnEnded(UDwCombatSubsystem& Combat, EDwSide Side) override;
	virtual void BeginSideTurn(UDwCombatSubsystem& Combat, EDwSide Side) override {}
	virtual void BeginUnitTurn(UDwCombatSubsystem& Combat, FDwUnitHandle Unit) override;
	virtual void OnCommandResolved(UDwCombatSubsystem& Combat) override;

	/** Upper bound on commands one unit may submit in a turn; a brain that keeps going is ended (and logged). */
	static constexpr int32 MaxCommandsPerUnit = 8;

private:
	/** Submits the acting unit's commands until it ends, the fight ends, or an ability resolves latently. */
	void ActForUnit(UDwCombatSubsystem& Combat);
	void FinishUnit(UDwCombatSubsystem& Combat, FDwUnitHandle Unit);
	/** Asks Unit's brain for its next-turn intents and stores them (only touches storage if there is something to store or clear). */
	void DeclareIntentsFor(UDwCombatSubsystem& Combat, FDwUnitHandle Unit);
	/** DeclareIntentsFor every unit with a brain, in handle order (the AI stream is drawn in a fixed order). */
	void DeclareAllIntents(UDwCombatSubsystem& Combat);

	UPROPERTY() TMap<FDwUnitHandle, TObjectPtr<UDwAiBrain>> BrainsByUnit;

	FRandomStream Random;
	FDwUnitHandle ActingUnit;
	FDwAiTurnContext TurnContext;
};
