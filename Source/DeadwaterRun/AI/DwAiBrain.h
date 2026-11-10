// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AI/DwAiTypes.h"
#include "Combat/DwCombatTypes.h"
#include "DwAiBrain.generated.h"

struct FDwCombatSnapshot;

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

	/** What the unit will do next turn, shown to the player (telegraphs). Crews declare nothing by default. */
	virtual FDwAiIntent DeclareIntent(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Unit, FRandomStream& Rng) const { return FDwAiIntent(); }
};
