// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DwAiTuningRow.generated.h"

/**
 * One weight per FDwAiScoring term (Docs/Architecture/07-Enemy-AI.md). Scores are in "hit points of value": a weight
 * of 1 on Damage means one point of damage dealt is worth one point. Terms marked (cost) are subtracted.
 * The same struct carries a candidate's per-term breakdown (raw term values before weighting) for logs and tests.
 * Values live in Tuning/AiTuning.json (owner balance-designer); the C++ defaults are zero so missing data is obvious.
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwAiWeights
{
	GENERATED_BODY()

	/** Per point of damage dealt to opponent units, capped at the target's Health (no value in overkill). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Action") float Damage = 0.f;
	/** Per opponent unit the action is expected to kill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Action") float Kill = 0.f;
	/** The action ends the fight in this side's favour (e.g. kills the player captain, C10). The opposite is a cost. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Action") float Win = 0.f;
	/** Per point of damage dealt to the opponent's ship, capped at its Health. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Action") float ShipDamage = 0.f;
	/** Extra per point of ship damage, scaled by how much hull the opponent ship has already lost (finish it off). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Action") float ShipLowHull = 0.f;
	/** (cost) Per point of damage dealt to this side's own units or ship. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Action") float FriendlyFire = 0.f;
	/** Per point of Health restored to an ally, capped at what the ally is missing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Action") float Heal = 0.f;
	/** Per point of Fear put on opponents that can feel it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Action") float Fear = 0.f;
	/** Per damaged opponent, scaled by the fraction of Health it is already missing (focus the wounded). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Action") float LowHealthFocus = 0.f;
	/** Extra per point of damage dealt to the opponent captain (Open-Questions C10b: how hard crews hunt the captain). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Action") float CaptainPressure = 0.f;
	/** (cost) Per MP the ability spends (MP never regenerates in a fight, C9). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Action") float ManaCost = 0.f;

	/** (cost) Per point of damage the opponents could deal to the unit at its destination on their next turn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Position") float Danger = 0.f;
	/** (cost) Standing on the opponent's ship when this side's cannons can sink it before the unit acts again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Position") float SinkingRisk = 0.f;
	/** (cost) Per tile the destination is beyond attack range of the nearest opponent the unit could hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Position") float Approach = 0.f;
	/** Per level of elevation at the destination, for units with a ranged attack (the sniper's nest gives range). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Position") float HighGround = 0.f;
	/** Standing on the opponent's deck. Only for profiles with bBoarders. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Position") float Boarding = 0.f;
	/** Standing on or next to a loaded cannon of this side. Only for profiles with bPreferCannons. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Position") float CannonAffinity = 0.f;

	/** Sum of A[i] * B[i] over every term, with costs subtracted (A = weights, B = raw terms). */
	static float Dot(const FDwAiWeights& Weights, const FDwAiWeights& Terms);

	/** "Damage=25 Kill=1 ..." listing only non-zero terms (logs, test messages). */
	FString ToString() const;
};

/** DT_AiTuning, keyed by AI profile id (Docs/Architecture/13-Data-Layout.md). Owner of the schema: ai-engineer. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwAiTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI") FDwAiWeights Weights;

	/** 0 = always the best-scoring candidate. > 0 = softmax sampling over candidates (score / Temperature), seeded. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI") float Temperature = 0.f;

	/** Destinations kept after the cheap positional pass, before danger is evaluated (performance bound). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI") int32 MaxMoveCandidates = 0;
};
