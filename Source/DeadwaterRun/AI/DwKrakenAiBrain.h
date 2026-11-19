// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "AI/DwAiBrain.h"
#include "Data/Tuning/DwAiTuningRow.h"
#include "DwKrakenAiBrain.generated.h"

class UDwAbilityDefinition;
class UDwAiProfile;
class UDwKrakenDefinition;
struct FDwUnitSnapshot;

/**
 * The Kraken's decisions (Docs/Architecture/08-Kraken-Boss.md "UDwKrakenAiBrain", DW-302). The combat rules (phase,
 * action count, tentacle damage scaling, the phase move gate) are DW-301's; this only chooses and telegraphs.
 *
 *  - Intents: the side controller asks DeclareIntents at combat start and after each enemy turn; the brain plans one
 *    move per action it will have next turn (MaxActionPoints: 2 after Eyes of the Sea) and the subsystem stores them.
 *  - Moves: a weighted draw from the current phase's moves (weights on DA_Kraken) using the AI stream. Eyes of the Sea is
 *    never planned while its extra action is pending or twice in one plan, so it can't chain into permanent double turns.
 *  - Targets: single-target moves are scored with FDwAiScoring and the profile's weights (lowest Health, kills, the
 *    captain, the ship when its hull is low); a move with no damage (Grab) goes for the hardest hitter not already rooted.
 *    Tile-aimable moves (FDwTargetingRules::bAimAtTile: Slam, Grab) are aimed at the victim's *tile*, so moving off it
 *    dodges. Group moves (Swipe, Arms of Fury) carry no target.
 *  - Later moves in one plan see the earlier ones' predicted damage, so two Slams don't both go to a pirate the first kills.
 *  - On its turn it carries out the stored intents in order; an intent the rules no longer accept (or an action beyond
 *    the declared ones) is chosen afresh. ShouldRedeclare asks for a new plan at turn start after a phase change.
 */
UCLASS()
class DEADWATERRUN_API UDwKrakenAiBrain : public UDwAiBrain
{
	GENERATED_BODY()

public:
	/** Phase move weights. Null (tests without a definition) = every phase move weighs 1. */
	UPROPERTY(EditAnywhere, Category = "AI")
	TObjectPtr<const UDwKrakenDefinition> Kraken;

	/** Target-scoring weights (DT_AiTuning row). Null = the built-in DwKrakenAiBrain::DefaultWeights. */
	UPROPERTY(EditAnywhere, Category = "AI")
	TObjectPtr<const UDwAiProfile> Profile;

	void SetTuningOverride(const FDwAiTuningRow& Tuning) { TuningOverride = Tuning; }

	virtual void Configure(const UDwAiProfile* InProfile, const FDwEncounterSetup& Setup) override;
	virtual FDwCombatCommand ChooseNextCommand(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Unit, FDwAiTurnContext& Context,
		FRandomStream& Rng) const override;
	virtual TArray<FDwAiIntent> DeclareIntents(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Unit, FRandomStream& Rng) const override;
	virtual bool ShouldRedeclare(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Unit) const override;

	/** Count moves from the current phase, in order (what DeclareIntents stores). Pure apart from Rng. */
	TArray<FDwAiIntent> PlanMoves(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Unit, int32 Count, FRandomStream& Rng) const;

	/** The phase's moves the unit can use right now, with their weights (positive weights only). */
	TArray<TPair<const UDwAbilityDefinition*, float>> GetMoveOptions(const FDwCombatSnapshot& Snapshot, const FDwUnitSnapshot& Self) const;

	/**
	 * The target for Move, or false if it has none. Single-target: the best-scoring victim (ties drawn from Rng), aimed at
	 * its tile when the move allows tile aims. Self: the unit. Group patterns: an empty target.
	 */
	bool ChooseTarget(const FDwCombatSnapshot& Snapshot, const FDwUnitSnapshot& Self, const UDwAbilityDefinition& Move, FRandomStream& Rng,
		FDwAbilityTarget& OutTarget) const;

	/** Actions the unit will have at its next turn start: MaxActionPoints (effects included), at least 1. */
	static int32 GetActionsNextTurn(const FDwUnitSnapshot& Self);

private:
	FDwAiTuningRow GetTuning() const;

	TOptional<FDwAiTuningRow> TuningOverride;
};
