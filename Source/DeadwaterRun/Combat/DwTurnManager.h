// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/DwTypes.h"
#include "DwTurnManager.generated.h"

class UDwCombatSubsystem;

/**
 * Round and side-turn sequencing (Docs/Architecture/03-Combat-Turns.md "UDwTurnManager"). Owned by UDwCombatSubsystem.
 * - Player side first; the player acts with any unit in any order until EndTurn.
 * - Enemy side: units act one at a time in encounter order (C14); dead, cowering and incapacitated units are skipped.
 * - No tick and no timers: every transition is a function call from a command, an ability end or a side controller.
 *
 * Transitions run as queued steps drained in a loop, so a controller that submits commands synchronously (the AI and
 * the simulator in instant mode) never recurses turn into turn: a 100-round headless fight uses constant stack depth.
 */
UCLASS()
class DEADWATERRUN_API UDwTurnManager : public UObject
{
	GENERATED_BODY()

public:
	void Init(UDwCombatSubsystem& InCombat, TArray<FDwUnitHandle> InEnemyOrder);

	/** Round 1, player side turn. */
	void BeginCombat();

	/** The side ends its turn (EndTurn command). The enemy side may end early; remaining enemies don't act. */
	void EndSideTurn(EDwSide Side);

	/** A unit finished (EndUnit command). On the enemy side this moves the sequence to the next enemy. */
	void NotifyUnitDone(FDwUnitHandle Unit);

	/** Stops all further transitions (an outcome was reached or the encounter is torn down). */
	void Stop();

	int32 GetRound() const { return Round; }
	EDwSide GetActiveSide() const { return ActiveSide; }
	/** The enemy whose turn it is, or invalid on the player side. */
	FDwUnitHandle GetActingUnit() const { return ActingUnit; }
	bool IsStopped() const { return bStopped; }

private:
	enum class EStep : uint8 { BeginSide, NextEnemyUnit, EndSide };
	struct FStep
	{
		EStep Type;
		EDwSide Side;
	};

	void Queue(EStep Type, EDwSide Side);
	void Drain();
	void RunBeginSide(EDwSide Side);
	void RunNextEnemyUnit();
	void RunEndSide(EDwSide Side);

	UPROPERTY()
	TObjectPtr<UDwCombatSubsystem> Combat;

	TArray<FDwUnitHandle> EnemyOrder;
	TArray<FStep> Pending;
	int32 Round = 0;
	EDwSide ActiveSide = EDwSide::Player;
	FDwUnitHandle ActingUnit;
	int32 NextEnemyIndex = 0;
	bool bDraining = false;
	bool bStopped = false;
	/** True between BeginSide and EndSide; guards against a second EndTurn queued for the same turn. */
	bool bSideTurnOpen = false;
};
