// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Run/DwRunTypes.h"

class UDwEventDefinition;
struct FDwEventChoice;
struct FDwEventContext;

/**
 * Event selection and resolution (Docs/Architecture/11-Events.md "Flow"). Pure: UDwRun is the only caller, the simulator
 * and tests call it directly.
 */
struct DEADWATERRUN_API FDwEventResolver
{
	/**
	 * The event at a node on Layer: valence Good with GoodChance (DT_RunTuning.GoodEventChance, 0.6), then a weighted pick
	 * among that valence's events valid on Layer, preferring ones not in State.SeenEvents; all seen -> repeats allowed;
	 * none of that valence -> the other valence. Null only when no event in Pool fits Layer. Same Random state -> same event.
	 */
	static const UDwEventDefinition* Roll(const FDwRunState& State, TConstArrayView<const UDwEventDefinition*> Pool, int32 Layer, float GoodChance,
		FRandomStream& Random);

	/** Whether Event may appear on Layer (MinLayer..MaxLayer, weight above 0, at least one choice). */
	static bool IsAvailable(const UDwEventDefinition& Event, int32 Layer);

	/** The first failing requirement's text ("Requires 50 gold"); empty when every requirement passes. */
	static FText GetRequirementBlocker(const FDwRunState& State, const FDwEventChoice& Choice);

	/** True when an outcome of Choice lands on a crew member the player picks (EDwCrewSelector::Chosen). */
	static bool NeedsChosenCrew(const FDwEventChoice& Choice);

	/** Living crew who may be picked for Choice (LoseCrew never offers the captain). Empty when Choice needs no pick. */
	static TArray<FGuid> GetChoosableCrew(const FDwRunState& State, const FDwEventChoice& Choice);

	/**
	 * A stand-in player for dev demos, tests and the simulator: the first choice of State.Event whose requirements pass,
	 * with OutCrew = the first crew member it may land on when it needs one. INDEX_NONE when no event is open, it's
	 * resolved already, or nothing can be chosen.
	 */
	static int32 PickDefaultChoice(const FDwRunState& State, FGuid& OutCrew);

	/** Applies Choice's outcomes in order to Context. Does not check requirements (UDwRun does). */
	static void Apply(const FDwEventChoice& Choice, FDwEventContext& Context);
};
