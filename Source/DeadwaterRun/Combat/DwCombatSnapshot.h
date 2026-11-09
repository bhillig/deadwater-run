// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Board/DwGridModel.h"
#include "Combat/DwCombatTypes.h"
#include "Core/DwTypes.h"
#include "DwCombatSnapshot.generated.h"

class UDwAbilityDefinition;

/**
 * A pure copy of one unit for rules, AI, UI and the simulator (Docs/Architecture/03-Combat-Turns.md "FDwCombatSnapshot").
 * DW-101 declares it because ability targeting and previews read it; DW-103's UDwCombatSubsystem::GetSnapshot fills it.
 * Beyond doc 03's list it carries AttackDamage, DamageBonus, DamageReduction (damage previews) and GuardedBy (defend redirect).
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwUnitSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Unit") FDwUnitHandle Handle;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") EDwSide Side = EDwSide::Player;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") FIntPoint Tile = FIntPoint::ZeroValue;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") FPrimaryAssetId Definition;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") bool bAlive = true;

	UPROPERTY(BlueprintReadOnly, Category = "Stats") float Health = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float MaxHealth = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float Mana = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float MaxMana = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float Fear = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float MaxFear = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float ActionPoints = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float MovePoints = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float AttackDamage = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float DamageBonus = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float DamageReduction = 0.f;

	/** Owned gameplay tags (unit, status, state). */
	UPROPERTY(BlueprintReadOnly, Category = "Tags") FGameplayTagContainer Tags;
	/** AbilityTag of every granted ability. */
	UPROPERTY(BlueprintReadOnly, Category = "Tags") FGameplayTagContainer GrantedAbilityTags;
	/** Definition of every granted ability, in grant order (pure queries run each ability's CDO against these). */
	UPROPERTY(BlueprintReadOnly, Category = "Tags") TArray<TObjectPtr<const UDwAbilityDefinition>> Abilities;

	/** The guardian of a Status.Guarded unit (GE_Guarded instigator). */
	UPROPERTY(BlueprintReadOnly, Category = "Defend") FDwUnitHandle GuardedBy;
};

USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwShipSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ship") EDwSide Side = EDwSide::Player;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") float Health = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") float MaxHealth = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") float CannonDamage = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") TArray<FIntPoint> CannonTiles;
};

/** The whole fight as plain data, built on demand by UDwCombatSubsystem::GetSnapshot (cheap: fewer than 20 units). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwCombatSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat") TArray<FDwUnitSnapshot> Units;
	UPROPERTY(BlueprintReadOnly, Category = "Combat") TArray<FDwShipSnapshot> Ships;
	UPROPERTY(BlueprintReadOnly, Category = "Combat") FDwGridModel Grid;
	UPROPERTY(BlueprintReadOnly, Category = "Combat") FDwGridRules GridRules;
	UPROPERTY(BlueprintReadOnly, Category = "Combat") int32 Round = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Combat") EDwSide ActiveSide = EDwSide::Player;
	/** Enemy side: the unit whose turn it is in the fixed sequence (C14). Invalid on the player side. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat") FDwUnitHandle ActingUnit;
	UPROPERTY(BlueprintReadOnly, Category = "Combat") FDwOutcomeConfig OutcomeConfig;
	/** Cannon tiles already fired this side turn (each cannon fires once per side turn, C7). */
	UPROPERTY(BlueprintReadOnly, Category = "Combat") TArray<FIntPoint> CannonsUsed;
	/** Run inventory view: item id -> count left. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat") TMap<FName, int32> Items;
	UPROPERTY(BlueprintReadOnly, Category = "Combat") EDwCombatOutcome Outcome = EDwCombatOutcome::None;

	const FDwUnitSnapshot* FindUnit(FDwUnitHandle Handle) const
	{
		return Units.FindByPredicate([Handle](const FDwUnitSnapshot& U) { return U.Handle == Handle; });
	}

	const FDwShipSnapshot* FindShip(EDwSide Side) const
	{
		return Ships.FindByPredicate([Side](const FDwShipSnapshot& S) { return S.Side == Side; });
	}
};
