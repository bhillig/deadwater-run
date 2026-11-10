// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "AI/DwAiTypes.h"
#include "Combat/DwCombatSnapshot.h"
#include "Data/Tuning/DwAiTuningRow.h"

class UDwAbilityDefinition;

/** One way an opponent could hit the acting unit next turn. */
struct FDwAiThreatOption
{
	int32 Range = 0;
	bool bRequiresLineOfSight = false;
	bool bCanTargetElevated = true;
	/** Expected damage to the acting unit (FDwDamageRules, random patterns spread over the unit's side). */
	float Damage = 0.f;
};

/** An opponent that can act next turn: where it can attack from (its tile + every tile it can walk to) and how. */
struct FDwAiThreat
{
	FDwUnitHandle Unit;
	TArray<FIntPoint> From;
	TArray<FDwAiThreatOption> Options;
};

/** One of the acting unit's own attacks, for "how far am I from being able to hit someone" (Approach). */
struct FDwAiReach
{
	int32 Range = 0;
	bool bCanTargetElevated = true;
	/** Ranged and able to shoot into or out of the nest: elevation is worth something (HighGround). */
	bool bRanged = false;
};

/**
 * What scoring needs about the acting unit's situation, computed once per decision so per-candidate scoring is cheap.
 * Reads only what the player can see on the HUD: positions, Health, MP, statuses, abilities and their tuning.
 */
struct DEADWATERRUN_API FDwAiScoringContext
{
	const FDwCombatSnapshot* Snapshot = nullptr;
	FDwUnitHandle Unit;
	EDwSide Side = EDwSide::Enemy;
	bool bPreferCannons = false;
	bool bBoarders = false;

	TArray<FDwAiThreat> Threats;
	TArray<FDwAiReach> Reach;
	/** The opponent ship can be sunk by this side's cannons before the unit acts again (SinkingRisk). */
	bool bOpponentShipCanSink = false;

	static FDwAiScoringContext Make(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Unit, bool bPreferCannons, bool bBoarders);

	const FDwUnitSnapshot* GetUnit() const { return Snapshot ? Snapshot->FindUnit(Unit) : nullptr; }
	EDwSide GetOpponentSide() const { return Side == EDwSide::Player ? EDwSide::Enemy : EDwSide::Player; }

	/** Scratch copy of units and ships for "does this end the fight" checks; built on first use. */
	mutable TOptional<FDwCombatSnapshot> OutcomeScratch;
};

/**
 * The crew AI's scoring terms (Docs/Architecture/07-Enemy-AI.md "FDwAiScoring"). Pure and deterministic: snapshot and
 * candidate in, raw term values out; FDwAiWeights::Dot turns them into a score. Every term is something a player can
 * reason about from the damage preview and the board.
 */
struct DEADWATERRUN_API FDwAiScoring
{
	/** Positional terms of ending the move on Tile, except Danger (see DangerAt): SinkingRisk, Approach, HighGround, Boarding, CannonAffinity. */
	static void ScorePosition(const FDwAiScoringContext& Context, FIntPoint Tile, FDwAiWeights& OutTerms);

	/** Damage the opponents could deal to the unit standing on Tile during their next turn (sum of each one's best option). */
	static float DangerAt(const FDwAiScoringContext& Context, FIntPoint Tile);

	/**
	 * Action terms of using Ability from Tile with this preview: Damage, Kill, Win, ShipDamage, ShipLowHull, FriendlyFire,
	 * Heal, Fear, LowHealthFocus, CaptainPressure, ManaCost.
	 */
	static void ScoreAction(const FDwAiScoringContext& Context, FIntPoint Tile, const UDwAbilityDefinition& Ability,
		const FDwAbilityPreview& Preview, FDwAiWeights& OutTerms, TArray<FDwUnitHandle>* OutKilled = nullptr);

	/**
	 * Every term of Candidate (fills Candidate.Terms and Candidate.Score). The doc 07 entry point. CachedDanger is
	 * DangerAt(MoveTile) if the caller already has it; it is recomputed anyway when the action kills a threat.
	 */
	static float ScoreCandidate(const FDwAiScoringContext& Context, FDwAiCandidate& Candidate, const UDwAbilityDefinition* Ability,
		const FDwAiWeights& Weights, const float* CachedDanger = nullptr);

	/** True for damage-dealing unit attacks (what threat and approach consider). */
	static bool IsUnitAttack(const UDwAbilityDefinition& Ability, const FDwTargetingContext& Context);
};
