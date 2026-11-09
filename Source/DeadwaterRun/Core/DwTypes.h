// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "DwTypes.generated.h"

/** Which side something belongs to. Tiles use Neutral for water and Kraken arena tiles. */
UENUM(BlueprintType)
enum class EDwSide : uint8
{
	Player,
	Enemy,
	Neutral
};

/** When a turn-duration effect counts down (Docs/Architecture/02-GAS-Foundation.md "Turn-based durations"). */
UENUM(BlueprintType)
enum class EDwTurnTickPoint : uint8
{
	OwnerSideTurnStart,
	OwnerSideTurnEnd
};

/**
 * Stable id of a combat unit within one fight. Commands, snapshots, events and the grid refer to units by handle,
 * never by actor pointer, so pure rules, the AI and the simulator work without a world.
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwUnitHandle
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadwater")
	int32 Id = INDEX_NONE;

	FDwUnitHandle() = default;
	explicit FDwUnitHandle(int32 InId) : Id(InId) {}

	bool IsValid() const { return Id != INDEX_NONE; }

	friend bool operator==(const FDwUnitHandle& A, const FDwUnitHandle& B) { return A.Id == B.Id; }
	friend bool operator!=(const FDwUnitHandle& A, const FDwUnitHandle& B) { return A.Id != B.Id; }
	friend uint32 GetTypeHash(const FDwUnitHandle& Handle) { return ::GetTypeHash(Handle.Id); }
};
