// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Data/DwDataTypes.h"
#include "DwTuningRows.generated.h"

/*
 * Tuning row structs. Every number a balance pass would change lives here (Docs/Architecture/13-Data-Layout.md).
 * Source of truth: Tuning/<Table>.json at the repo root; field names here = JSON keys. Values are owned by
 * balance-designer; schema by gameplay-engineer (AI rows: ai-engineer, DW-106).
 */

/** DT_UnitTuning, keyed by unit id. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwUnitTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") int32 MaxHealth = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") int32 MaxMana = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") int32 AttackDamage = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") int32 MoveRange = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") int32 MaxActionPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level") int32 HealthPerLevel = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level") int32 ManaPerLevel = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level") int32 AttackDamagePerLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run") int32 XpOnKill = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run") int32 RecruitCost = 0;

	/** Gains apply per level above 1. Combat spawn and run state both use these, so they always agree. */
	int32 GetMaxHealthAtLevel(int32 Level) const { return MaxHealth + HealthPerLevel * FMath::Max(0, Level - 1); }
	int32 GetMaxManaAtLevel(int32 Level) const { return MaxMana + ManaPerLevel * FMath::Max(0, Level - 1); }
	int32 GetAttackDamageAtLevel(int32 Level) const { return AttackDamage + AttackDamagePerLevel * FMath::Max(0, Level - 1); }
};

/** DT_AbilityTuning, keyed by ability id. Fields an ability doesn't use stay 0. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwAbilityTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") int32 ManaCost = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") int32 Damage = 0;
	/** Multiplier of the source's AttackDamage, added to Damage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") float DamageScale = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") int32 Heal = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") int32 Fear = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") int32 Range = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") int32 MinHits = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") int32 MaxHits = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") int32 DurationTurns = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") int32 ShopPrice = 0;
};

/** DT_ItemTuning, keyed by item id. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwItemTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") int32 Price = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") int32 Amount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") int32 Range = 0;
};

/** DT_PerkTuning, keyed by perk id. Description tokens {Value} / {Value2}. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwPerkTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perk") float Value = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perk") float Value2 = 0.f;
};

/** DT_CaptainTuning, keyed by captain id. Multipliers default to 1 (no effect). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwCaptainTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Captain") float XpMultiplier = 1.f;
	/** Multiplier on XP needed per level (0.5 = levels twice as fast). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Captain") float LevelThresholdMultiplier = 1.f;
	/** Dread Reputation: starting Fear added per surrender win. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Captain") int32 FearPerSurrender = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Captain") int32 StartingFearCap = 0;
	/** Conqueror: DamageBonus per own kill (0.01 = +1%). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Captain") float KillBonusPerKill = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Captain") float KillBonusCap = 0.f;
	/** Team Leader passive: damage multiplier when attacking next to an ally (K4). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Captain") float SynergyDamageMultiplier = 1.f;
};

/** DT_ShipTuning, keyed by ship id. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwShipTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship") int32 MaxHealth = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship") int32 CannonDamage = 0;
};

/** DT_EncounterTuning, keyed by encounter id. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwEncounterTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter") int32 BaseGold = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter") int32 ClearXp = 0;
};

/** DT_EconomyTuning, single row "Default". Field list from Docs/Architecture/10-Economy-Shop-Loot.md. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwEconomyTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gold") int32 StartingGold = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot") float SinkLootMultiplier = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot") float PlunderLootMultiplier = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot") float SurrenderLootMultiplier = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot") float PlunderItemChance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop") int32 RepairCost = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop") int32 RepairAmount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop") int32 AbilityUnlockCost = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop") int32 ItemCost = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop") int32 RecruitCostDefault = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop") int32 DisbandRefund = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crew") int32 MaxCrew = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop") int32 ShopAbilityOffers = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop") int32 ShopRecruitOffers = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop") int32 ShopUpgradeOffers = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards") int32 RewardAbilityChoices = 0;
};

/** DT_ProgressionTuning, keyed by level ("1", "2", ...). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwProgressionTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression") int32 XpToNext = 0;
};

/** DT_RunTuning, single row "Default". */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwRunTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run") float GoodEventChance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run") int32 PerkChoices = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run") int32 MaxLevel = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run") int32 MaxAbilitySlots = 0;
};

/** DT_CombatTuning, single row "Default". Grid numbers (doc 04) and Fear. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwCombatTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid") int32 BoardingMoveCost = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid") int32 ClimbMoveCost = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid") int32 RangeBonusPerElevation = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fear") int32 MaxFear = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fear") EDwFearCowerPolicy FearCowerPolicy = EDwFearCowerPolicy::ResetToZero;
};

/** DT_KrakenTuning, keyed by Kraken id. Body HP lives in DT_UnitTuning (KrakenBody). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwKrakenTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tentacles") int32 TentacleCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tentacles") int32 TentacleHealth = 0;
	/** Actions per turn, one entry per phase in UDwKrakenDefinition::Phases order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions") TArray<int32> BaseActionsPerTurn;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tentacles") float ActionsLostPerTentacle = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tentacles") float DamageMultiplierPerMissingTentacle = 0.f;
	/** 0 = tentacles never regrow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tentacles") int32 TentacleRegrowTurns = 0;
};
