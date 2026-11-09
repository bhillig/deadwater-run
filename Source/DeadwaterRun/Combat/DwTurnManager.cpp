// Copyright Brandon Hillig 2026

#include "Combat/DwTurnManager.h"

#include "Combat/DwCombatSideController.h"
#include "Combat/DwCombatSubsystem.h"
#include "Core/DwLog.h"

void UDwTurnManager::Init(UDwCombatSubsystem& InCombat, TArray<FDwUnitHandle> InEnemyOrder)
{
	Combat = &InCombat;
	EnemyOrder = MoveTemp(InEnemyOrder);
}

void UDwTurnManager::BeginCombat()
{
	Round = 1;
	bStopped = false;
	Queue(EStep::BeginSide, EDwSide::Player); // the player always starts
	Drain();
}

void UDwTurnManager::EndSideTurn(EDwSide Side)
{
	if (Side != ActiveSide || !bSideTurnOpen)
	{
		return;
	}
	bSideTurnOpen = false;
	// An early enemy EndTurn drops any queued "next enemy" step for this turn.
	Pending.RemoveAll([](const FStep& Step) { return Step.Type == EStep::NextEnemyUnit; });
	Queue(EStep::EndSide, Side);
	Drain();
}

void UDwTurnManager::NotifyUnitDone(FDwUnitHandle Unit)
{
	if (ActiveSide == EDwSide::Enemy && bSideTurnOpen && Unit == ActingUnit)
	{
		ActingUnit = FDwUnitHandle();
		Queue(EStep::NextEnemyUnit, EDwSide::Enemy);
		Drain();
	}
}

void UDwTurnManager::Stop()
{
	bStopped = true;
	Pending.Reset();
	ActingUnit = FDwUnitHandle();
}

void UDwTurnManager::Queue(EStep Type, EDwSide Side)
{
	if (!bStopped)
	{
		Pending.Add({ Type, Side });
	}
}

void UDwTurnManager::Drain()
{
	if (bDraining)
	{
		return; // the outer loop picks the new step up
	}
	TGuardValue<bool> Guard(bDraining, true);
	while (!Pending.IsEmpty() && !bStopped)
	{
		const FStep Step = Pending[0];
		Pending.RemoveAt(0);
		switch (Step.Type)
		{
		case EStep::BeginSide:     RunBeginSide(Step.Side); break;
		case EStep::NextEnemyUnit: RunNextEnemyUnit(); break;
		case EStep::EndSide:       RunEndSide(Step.Side); break;
		}
	}
}

void UDwTurnManager::RunBeginSide(EDwSide Side)
{
	ActiveSide = Side;
	ActingUnit = FDwUnitHandle();
	NextEnemyIndex = 0;
	bSideTurnOpen = true;
	UE_LOG(LogDwCombat, Verbose, TEXT("Round %d: %s turn"), Round, *StaticEnum<EDwSide>()->GetNameStringByValue(static_cast<int64>(Side)));

	Combat->HandleSideTurnBegin(Side); // budgets, durations, TurnStarted event
	if (bStopped)
	{
		return;
	}
	if (Side == EDwSide::Enemy)
	{
		Queue(EStep::NextEnemyUnit, Side); // runs after the controller's side-start hook
	}
	if (IDwCombatSideController* Controller = Combat->GetSideController(Side))
	{
		Controller->BeginSideTurn(*Combat, Side);
	}
}

void UDwTurnManager::RunNextEnemyUnit()
{
	if (ActiveSide != EDwSide::Enemy || !bSideTurnOpen)
	{
		return;
	}
	while (NextEnemyIndex < EnemyOrder.Num())
	{
		const FDwUnitHandle Unit = EnemyOrder[NextEnemyIndex++];
		if (!Combat->BeginEnemyUnitTurn(Unit)) // dead, cowering or incapacitated: skipped
		{
			if (bStopped)
			{
				return;
			}
			continue;
		}
		ActingUnit = Unit;
		if (IDwCombatSideController* Controller = Combat->GetSideController(EDwSide::Enemy))
		{
			Controller->BeginUnitTurn(*Combat, Unit);
		}
		return; // the controller's EndUnit queues the next enemy
	}
	// Everyone has acted.
	bSideTurnOpen = false;
	Queue(EStep::EndSide, EDwSide::Enemy);
}

void UDwTurnManager::RunEndSide(EDwSide Side)
{
	ActingUnit = FDwUnitHandle();
	Combat->HandleSideTurnEnd(Side);
	if (bStopped)
	{
		return;
	}
	if (Side == EDwSide::Enemy)
	{
		++Round; // a round is both sides
	}
	Queue(EStep::BeginSide, Side == EDwSide::Player ? EDwSide::Enemy : EDwSide::Player);
}
