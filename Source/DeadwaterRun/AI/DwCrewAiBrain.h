// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "AI/DwAiBrain.h"
#include "Data/Tuning/DwAiTuningRow.h"
#include "DwCrewAiBrain.generated.h"

class UDwAiProfile;
struct FDwUnitSnapshot;

/**
 * One-ply utility planner for pirate crews (Docs/Architecture/07-Enemy-AI.md "UDwCrewAiBrain"):
 *  1. Destinations: the unit's tile plus every tile UDwGA_Move would accept, ranked by a cheap score (position + best
 *     action there) and cut to MaxMoveCandidates; danger is only evaluated for the survivors.
 *  2. For each destination, each ability the unit can afford, each target GatherValidTargets offers from that tile:
 *     the ability's own PreviewOutcome (the numbers the player sees).
 *  3. FDwAiScoring with the profile's weights; the best wins, ties (and Temperature > 0) drawn from the stream.
 *  4. Commands: Move first if the tile differs, the planned Ability on the next call, then EndUnit. A unit that acted
 *     without moving gets one more decision: where to stand (one move per turn, C3, so act-then-move is legal).
 * Items are the player's run inventory, so only the player side (simulator policy) considers Ability.Item.
 */
UCLASS()
class DEADWATERRUN_API UDwCrewAiBrain : public UDwAiBrain
{
	GENERATED_BODY()

public:
	/** Switches and the DT_AiTuning row. Null = no AI data: every unit ends its turn (logged once by the controller). */
	UPROPERTY(EditAnywhere, Category = "AI")
	TObjectPtr<const UDwAiProfile> Profile;

	/** Uses this tuning instead of the profile's row (tests and the simulator's what-if runs). */
	void SetTuningOverride(const FDwAiTuningRow& Tuning) { TuningOverride = Tuning; }

	virtual FDwCombatCommand ChooseNextCommand(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Unit, FDwAiTurnContext& Context,
		FRandomStream& Rng) const override;

	/** Every scored option for Unit right now, best first (debugging, tests, the simulator's decision log). */
	TArray<FDwAiCandidate> EvaluateCandidates(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Unit) const;

private:
	FDwAiTuningRow GetTuning() const;
	/** The candidate to act on: the best, or (Temperature > 0) a softmax draw. Ties prefer staying on UnitTile, then are drawn. */
	static const FDwAiCandidate* Pick(const TArray<FDwAiCandidate>& Candidates, float Temperature, FIntPoint UnitTile, FRandomStream& Rng);
	static FDwCombatCommand ToCommand(const FDwAiCandidate& Candidate, const FDwUnitSnapshot& Unit, FDwAiTurnContext& Context);

	TOptional<FDwAiTuningRow> TuningOverride;
};
