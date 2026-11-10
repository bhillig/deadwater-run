// Copyright Brandon Hillig 2026

#include "Run/DwRunTypes.h"

#include "Data/DwItemDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"

float FDwCrewMemberState::GetMaxHealth() const
{
	const FDwUnitTuningRow* Row = Unit ? Unit->GetTuningRow<FDwUnitTuningRow>() : nullptr;
	return Row ? static_cast<float>(Row->GetMaxHealthAtLevel(Level)) : 0.f;
}

FText FDwCrewMemberState::GetDisplayName() const
{
	return Unit ? Unit->DisplayName : FText::GetEmpty();
}

TArray<int32> FDwMapGraph::GetChoices(int32 Current) const
{
	TArray<int32> Choices;
	if (Current == INDEX_NONE)
	{
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			if (Nodes[Index].Layer == 0 && Nodes[Index].Type != EDwNodeType::Boss)
			{
				Choices.Add(Index);
			}
		}
	}
	else if (Nodes.IsValidIndex(Current))
	{
		Choices = Nodes[Current].Next;
	}
	return Choices;
}

const FDwCrewMemberState* FDwRunState::FindCrew(const FGuid& Id) const
{
	return Crew.FindByPredicate([&Id](const FDwCrewMemberState& Member) { return Member.Id == Id; });
}

FDwCrewMemberState* FDwRunState::FindCrew(const FGuid& Id)
{
	return Crew.FindByPredicate([&Id](const FDwCrewMemberState& Member) { return Member.Id == Id; });
}

const FDwCrewMemberState* FDwRunState::GetCaptainMember() const
{
	return Crew.FindByPredicate([](const FDwCrewMemberState& Member) { return Member.bIsCaptain; });
}

int32 FDwRunState::CountLivingCrew() const
{
	int32 Count = 0;
	for (const FDwCrewMemberState& Member : Crew)
	{
		Count += Member.bAlive ? 1 : 0;
	}
	return Count;
}

int32 FDwRunState::CountItem(FName ItemId) const
{
	int32 Count = 0;
	for (const FDwItemStack& Stack : Inventory)
	{
		if (Stack.Item && Stack.Item->GetFName() == ItemId)
		{
			Count += Stack.Count;
		}
	}
	return Count;
}

namespace DwRun
{
	FString PhaseName(EDwRunPhase Phase)
	{
		return StaticEnum<EDwRunPhase>()->GetNameStringByValue(static_cast<int64>(Phase));
	}

	FString EndReasonName(EDwRunEndReason Reason)
	{
		return StaticEnum<EDwRunEndReason>()->GetNameStringByValue(static_cast<int64>(Reason));
	}

	EDwRunEndReason EndReasonFor(EDwCombatOutcome Outcome)
	{
		switch (Outcome)
		{
		case EDwCombatOutcome::DefeatShipSunk:    return EDwRunEndReason::ShipSunk;
		case EDwCombatOutcome::DefeatCrewDead:    return EDwRunEndReason::CrewDead;
		case EDwCombatOutcome::DefeatCaptainDead: return EDwRunEndReason::CaptainDead;
		default:                                  return EDwRunEndReason::None;
		}
	}
}
