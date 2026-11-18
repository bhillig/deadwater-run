// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Board/DwGridModel.h"
#include "Combat/DwCombatTypes.h"
#include "Core/DwTypes.h"
#include "AI/DwAiTypes.h"
#include "Data/DwItemDefinition.h"
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
	/** MovePoints at the start of the unit's next turn (the AI reads it to judge next-turn threat, DW-106). */
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float MoveRange = 0.f;
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

/**
 * The Kraken fight's rule state (Docs/Architecture/08-Kraken-Boss.md). Unset (Body invalid) in every other fight.
 * Phase and tentacle numbers are what the combat rules apply; the brain (DW-302) and the boss HUD (DW-303) read them.
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwKrakenSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Kraken") FDwUnitHandle Body;
	/** Index into UDwKrakenDefinition::Phases of the phase in force (changes at the start of the Kraken's turn, B7). */
	UPROPERTY(BlueprintReadOnly, Category = "Kraken") int32 PhaseIndex = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Kraken") FName PhaseId;
	/** AbilityTag of every move the current phase allows. The body may use nothing else. */
	UPROPERTY(BlueprintReadOnly, Category = "Kraken") FGameplayTagContainer PhaseMoves;
	UPROPERTY(BlueprintReadOnly, Category = "Kraken") int32 TentaclesTotal = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Kraken") int32 TentaclesDead = 0;
	/** Multiplier on every hit the Kraken deals (B2: missing tentacles weaken it). 1 = full strength. */
	UPROPERTY(BlueprintReadOnly, Category = "Kraken") float DamageMultiplier = 1.f;

	bool IsSet() const { return Body.IsValid(); }
};

/** One unit's declared moves for its next turn (Docs/Architecture/03 "intent storage"; the Kraken always declares, B6). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwUnitIntents
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Intent") FDwUnitHandle Unit;
	/** In execution order. */
	UPROPERTY(BlueprintReadOnly, Category = "Intent") TArray<FDwAiIntent> Intents;
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
	/** The player captain's rules (synergy, defend, kill bonus), from the setup. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat") FDwCaptainCombatRules CaptainRules;
	/** Cannon tiles already fired this side turn (each cannon fires once per side turn, C7). */
	UPROPERTY(BlueprintReadOnly, Category = "Combat") TArray<FIntPoint> CannonsUsed;
	/** Run inventory view: item id -> count left. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat") TMap<FName, int32> Items;
	/** Definition of every item id the fight knows (pure item targeting and previews). */
	UPROPERTY(BlueprintReadOnly, Category = "Combat") TMap<FName, TObjectPtr<const UDwItemDefinition>> ItemDefinitions;
	UPROPERTY(BlueprintReadOnly, Category = "Combat") EDwCombatOutcome Outcome = EDwCombatOutcome::None;
	/** Kraken fight only. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat") FDwKrakenSnapshot Kraken;
	/** Declared intents, one entry per unit that has any. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat") TArray<FDwUnitIntents> Intents;

	const FDwUnitIntents* FindIntents(FDwUnitHandle Handle) const
	{
		return Intents.FindByPredicate([Handle](const FDwUnitIntents& I) { return I.Unit == Handle; });
	}

	const FDwUnitSnapshot* FindUnit(FDwUnitHandle Handle) const
	{
		return Units.FindByPredicate([Handle](const FDwUnitSnapshot& U) { return U.Handle == Handle; });
	}

	const UDwItemDefinition* FindItem(FName ItemId) const
	{
		const TObjectPtr<const UDwItemDefinition>* Item = ItemDefinitions.Find(ItemId);
		return Item ? Item->Get() : nullptr;
	}

	const FDwShipSnapshot* FindShip(EDwSide Side) const
	{
		return Ships.FindByPredicate([Side](const FDwShipSnapshot& S) { return S.Side == Side; });
	}
};
