// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DwTargetingRules.generated.h"

/** Docs/Architecture/02-GAS-Foundation.md "FDwTargetingRules". */
UENUM(BlueprintType)
enum class EDwTargetPattern : uint8
{
	Self,
	SingleUnit,
	SingleUnitOrShip,
	EnemyShip,
	Tile,
	AllEnemies,
	AllAllies,
	RandomEnemies,
	Line
};

UENUM(BlueprintType)
enum class EDwTargetSide : uint8
{
	Enemy,
	Ally,
	Any
};

/** What an ability (or item) may target. Range and hit counts come from the tuning row, not from here. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwTargetingRules
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
	EDwTargetPattern Pattern = EDwTargetPattern::SingleUnit;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
	EDwTargetSide Side = EDwTargetSide::Enemy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
	bool bRequiresLineOfSight = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
	bool bIgnoreRange = false;

	/** false for melee: a unit in the nest can't be a melee target (C5). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
	bool bCanTargetElevated = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (Categories = "Tile"))
	FGameplayTagContainer RequiredSourceTileTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
	FGameplayTagContainer RequiredTargetTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
	FGameplayTagContainer BlockedTargetTags;
};
