// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Abilities/DwTargeting.h"
#include "Data/Tuning/DwAiTuningRow.h"
#include "DwAiTypes.generated.h"

class UDwAbilityDefinition;

/**
 * One option the crew brain considers (Docs/Architecture/07-Enemy-AI.md "FDwAiCandidate"): where to stand, then what
 * to do there. No ability = only reposition. Plain data: it is what the brain turns into Move and Ability commands.
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwAiCandidate
{
	GENERATED_BODY()

	/** Destination; the unit's own tile means "stay". */
	UPROPERTY(BlueprintReadOnly, Category = "AI") FIntPoint MoveTile = FIntPoint::ZeroValue;
	/** Empty = no action from MoveTile. */
	UPROPERTY(BlueprintReadOnly, Category = "AI") FGameplayTag AbilityTag;
	UPROPERTY(BlueprintReadOnly, Category = "AI") FDwAbilityTarget Target;
	/** Ability.Item only. */
	UPROPERTY(BlueprintReadOnly, Category = "AI") FName ItemId;
	/** What the action would do, as the player's damage preview shows it. */
	UPROPERTY(BlueprintReadOnly, Category = "AI") FDwAbilityPreview Preview;

	UPROPERTY(BlueprintReadOnly, Category = "AI") float Score = 0.f;
	/** Raw value of every term before weighting (why the brain liked it). */
	UPROPERTY(BlueprintReadOnly, Category = "AI") FDwAiWeights Terms;

	bool HasAction() const { return AbilityTag.IsValid(); }
	FString ToString() const;
};

/** A telegraphed move (Docs/Architecture/07 "FDwAiIntent"). The Kraken always declares one; crews may (DW-302). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwAiIntent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AI") FGameplayTag AbilityTag;
	UPROPERTY(BlueprintReadOnly, Category = "AI") FDwAbilityTarget Target;
	UPROPERTY(BlueprintReadOnly, Category = "AI") bool bTelegraph = false;

	bool IsSet() const { return AbilityTag.IsValid(); }
};

/**
 * What a brain decided earlier in the same unit turn. Owned by the side controller and handed to every
 * ChooseNextCommand call, so the brain itself stays stateless: "move here, then do this" survives the move.
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwAiTurnContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AI") bool bHasPlan = false;
	UPROPERTY(BlueprintReadOnly, Category = "AI") FDwAiCandidate Plan;
	/** Commands chosen for this unit this turn (a safety bound for the controller). */
	UPROPERTY(BlueprintReadOnly, Category = "AI") int32 Commands = 0;

	void Reset() { *this = FDwAiTurnContext(); }
};
