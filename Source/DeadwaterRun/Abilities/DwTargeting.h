// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Core/DwTypes.h"
#include "DwTargeting.generated.h"

class UDwAbilityDefinition;
struct FDwCombatSnapshot;

/** One thing an ability can be aimed at: a unit, a ship, or a tile. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwAbilityTarget
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Target") FDwUnitHandle Unit;
	UPROPERTY(BlueprintReadOnly, Category = "Target") FIntPoint Tile = FIntPoint::ZeroValue;
	UPROPERTY(BlueprintReadOnly, Category = "Target") bool bShip = false;
	UPROPERTY(BlueprintReadOnly, Category = "Target") EDwSide ShipSide = EDwSide::Neutral;

	static FDwAbilityTarget MakeUnit(FDwUnitHandle InUnit, FIntPoint InTile) { FDwAbilityTarget T; T.Unit = InUnit; T.Tile = InTile; return T; }
	static FDwAbilityTarget MakeShip(EDwSide Side) { FDwAbilityTarget T; T.bShip = true; T.ShipSide = Side; return T; }
	static FDwAbilityTarget MakeTile(FIntPoint InTile) { FDwAbilityTarget T; T.Tile = InTile; return T; }

	bool IsUnit() const { return Unit.IsValid(); }

	friend bool operator==(const FDwAbilityTarget& A, const FDwAbilityTarget& B)
	{
		return A.Unit == B.Unit && A.bShip == B.bShip && A.ShipSide == B.ShipSide && (A.Unit.IsValid() || A.bShip || A.Tile == B.Tile);
	}
};

/** What the UI shows before a click, and what the AI scores. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwTargetPreview
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Preview") FDwAbilityTarget Target;
	/** Per hit for RandomEnemies patterns. */
	UPROPERTY(BlueprintReadOnly, Category = "Preview") int32 Damage = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Preview") int32 Heal = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Preview") int32 Fear = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Preview") bool bLethal = false;
};

USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwAbilityPreview
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Preview") bool bValid = false;
	/** Shown on a greyed-out target ("Out of range", "No line of sight"). */
	UPROPERTY(BlueprintReadOnly, Category = "Preview") FText Reason;
	/** After redirects (defend): the units that would actually be hit. */
	UPROPERTY(BlueprintReadOnly, Category = "Preview") TArray<FDwTargetPreview> Targets;
	/** Hit count range for random patterns (Six Pistols, Arms of Fury). */
	UPROPERTY(BlueprintReadOnly, Category = "Preview") int32 MinHits = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Preview") int32 MaxHits = 1;
};

/**
 * Everything an ability's pure queries need. Runs on the ability CDO, so the definition (tuning) comes in here.
 * SourceTileOverride lets the UI preview "move here, then act".
 */
struct DEADWATERRUN_API FDwTargetingContext
{
	const FDwCombatSnapshot* Snapshot = nullptr;
	const UDwAbilityDefinition* Ability = nullptr;
	FDwUnitHandle Source;
	TOptional<FIntPoint> SourceTileOverride;
	/** Situational damage multiplier (Team Leader synergy, DW-207). 1 = none. */
	float DamageMultiplier = 1.f;
	/** Combat random stream for random patterns. Required by ResolveTargets for RandomEnemies. */
	FRandomStream* Rng = nullptr;
	/** Ability.Item: the item being aimed (its definition decides the targeting). */
	FName ItemId;

	/** The source's tile, or the override. Unset if the source is not on the board. */
	TOptional<FIntPoint> GetSourceTile() const;
};

/**
 * Activation payload: the combat subsystem triggers abilities with Event.Ability.Activate and this target data
 * (Docs/Architecture/02 "Activation and target data"). Abilities never read input.
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwGameplayAbilityTargetData_Grid : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	UPROPERTY() FIntPoint Tile = FIntPoint::ZeroValue;
	UPROPERTY() FDwUnitHandle Unit;
	UPROPERTY() FName ItemId;
	UPROPERTY() bool bShip = false;
	UPROPERTY() EDwSide ShipSide = EDwSide::Neutral;

	FDwAbilityTarget ToTarget() const;
	static FGameplayAbilityTargetDataHandle MakeHandle(const FDwAbilityTarget& Target, FName ItemId = NAME_None);

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual FString ToString() const override;
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template <>
struct TStructOpsTypeTraits<FDwGameplayAbilityTargetData_Grid> : public TStructOpsTypeTraitsBase2<FDwGameplayAbilityTargetData_Grid>
{
	enum { WithNetSerializer = true };
};
