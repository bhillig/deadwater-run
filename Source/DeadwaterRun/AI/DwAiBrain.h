// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AI/DwAiTypes.h"
#include "Combat/DwCombatTypes.h"
#include "DwAiBrain.generated.h"

struct FDwCombatSnapshot;
struct FDwEncounterSetup;
class UDwAiProfile;

/**
 * Decides one unit's commands (Docs/Architecture/07-Enemy-AI.md). Stateless apart from its configuration: everything it
 * remembers within a unit turn travels in FDwAiTurnContext, owned by the caller. It only reads the snapshot, so it runs
 * without a world (tests, the headless simulator) and gives the same command for the same snapshot, context and stream.
 * Subclasses: UDwCrewAiBrain (DW-106), UDwKrakenAiBrain (DW-302).
 */
UCLASS(Abstract)
class DEADWATERRUN_API UDwAiBrain : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * The next command for Unit: a Move, an Ability, or EndUnit when it is done. Called again after each command
	 * resolves until it returns EndUnit. Draws randomness only from Rng.
	 */
	virtual FDwCombatCommand ChooseNextCommand(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Unit, FDwAiTurnContext& Context,
		FRandomStream& Rng) const PURE_VIRTUAL(UDwAiBrain::ChooseNextCommand, return FDwCombatCommand::MakeEndUnit(Unit););

	/**
	 * Called once when the side controller creates the brain for an encounter: the profile that chose this brain class
	 * and the fight's setup (the Kraken brain reads its phase move weights from Setup.Kraken).
	 */
	virtual void Configure(const UDwAiProfile* InProfile, const FDwEncounterSetup& Setup) {}

	/**
	 * What the unit will do at its next turn, in execution order, shown to the player (telegraphs, Docs/Architecture/08
	 * "Intents"). The side controller asks at combat start and at the end of each of the unit's turns and stores the
	 * answer with UDwCombatSubsystem::DeclareIntents. Crews declare nothing.
	 */
	virtual TArray<FDwAiIntent> DeclareIntents(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Unit, FRandomStream& Rng) const { return {}; }

	/**
	 * At the start of the unit's turn: true if the intents it declared can no longer be carried out as shown (a phase
	 * change made them illegal, its action count changed), so the controller declares fresh ones before it acts.
	 */
	virtual bool ShouldRedeclare(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Unit) const { return false; }
};
