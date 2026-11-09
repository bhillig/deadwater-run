// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "DwDataTypes.generated.h"

class UDwAbilityDefinition;

/** Docs/Architecture/13-Data-Layout.md "UDwEncounterDefinition". */
UENUM(BlueprintType)
enum class EDwEncounterKind : uint8
{
	ShipBattle,
	Kraken
};

/** Map node types (Docs/Architecture/09-Run-Map.md). */
UENUM(BlueprintType)
enum class EDwNodeType : uint8
{
	Fight,
	Shop,
	Event,
	Boss
};

/** Used by the 60/40 event roll (Docs/Architecture/11-Events.md). */
UENUM(BlueprintType)
enum class EDwEventValence : uint8
{
	Good,
	Bad
};

/** What destroying a tentacle does (Open-Questions B2). */
UENUM(BlueprintType)
enum class EDwTentacleEffect : uint8
{
	ReduceActions,
	ReduceDamage,
	Both
};

/** Offer kinds a shop can show (Docs/Architecture/10-Economy-Shop-Loot.md "GenerateInventory"). */
UENUM(BlueprintType)
enum class EDwShopOfferType : uint8
{
	Repair,
	Item,
	Ability,
	Recruit,
	ShipUpgrade,
	Disband
};

/** What happens to Fear after an enemy cowers (Open-Questions K8; placeholder ResetToZero). */
UENUM(BlueprintType)
enum class EDwFearCowerPolicy : uint8
{
	ResetToZero,
	StayAtMax,
	DropToOne
};

/** An ability with a selection weight (ability pools, Kraken phase moves). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwWeightedAbility
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	TObjectPtr<UDwAbilityDefinition> Ability;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "0"))
	float Weight = 1.f;
};
