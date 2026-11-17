// Copyright Brandon Hillig 2026

#include "Run/Events/DwEventResolver.h"

#include "Data/DwEventDefinition.h"
#include "Run/Events/DwEventOutcome.h"

namespace DwEventResolver
{
	/** Weighted pick among the candidates; null when none has weight. */
	const UDwEventDefinition* PickWeighted(const TArray<const UDwEventDefinition*>& Candidates, FRandomStream& Random)
	{
		float Total = 0.f;
		for (const UDwEventDefinition* Event : Candidates)
		{
			Total += Event->Weight;
		}
		if (Candidates.IsEmpty() || Total <= 0.f)
		{
			return nullptr;
		}
		float Roll = Random.FRand() * Total;
		for (const UDwEventDefinition* Event : Candidates)
		{
			if (Roll < Event->Weight)
			{
				return Event;
			}
			Roll -= Event->Weight;
		}
		return Candidates.Last();
	}

	/** Events of Valence on Layer, unseen ones only if there are any. */
	TArray<const UDwEventDefinition*> Candidates(const FDwRunState& State, TConstArrayView<const UDwEventDefinition*> Pool, int32 Layer,
		EDwEventValence Valence)
	{
		TArray<const UDwEventDefinition*> All;
		TArray<const UDwEventDefinition*> Unseen;
		for (const UDwEventDefinition* Event : Pool)
		{
			if (Event && Event->Valence == Valence && FDwEventResolver::IsAvailable(*Event, Layer))
			{
				All.Add(Event);
				if (!State.SeenEvents.Contains(Event))
				{
					Unseen.Add(Event);
				}
			}
		}
		return Unseen.IsEmpty() ? All : Unseen;
	}
}

const UDwEventDefinition* FDwEventResolver::Roll(const FDwRunState& State, TConstArrayView<const UDwEventDefinition*> Pool, int32 Layer,
	float GoodChance, FRandomStream& Random)
{
	// The valence is always the stream's first draw, so the pool's contents never shift the 60/40 split.
	const EDwEventValence Valence = Random.FRand() < GoodChance ? EDwEventValence::Good : EDwEventValence::Bad;
	const EDwEventValence Other = Valence == EDwEventValence::Good ? EDwEventValence::Bad : EDwEventValence::Good;
	TArray<const UDwEventDefinition*> Candidates = DwEventResolver::Candidates(State, Pool, Layer, Valence);
	if (Candidates.IsEmpty())
	{
		Candidates = DwEventResolver::Candidates(State, Pool, Layer, Other);
	}
	return DwEventResolver::PickWeighted(Candidates, Random);
}

bool FDwEventResolver::IsAvailable(const UDwEventDefinition& Event, int32 Layer)
{
	return Event.Weight > 0.f && Layer >= Event.MinLayer && Layer <= Event.MaxLayer && !Event.Choices.IsEmpty();
}

FText FDwEventResolver::GetRequirementBlocker(const FDwRunState& State, const FDwEventChoice& Choice)
{
	for (const UDwEventRequirement* Requirement : Choice.Requirements)
	{
		if (Requirement && !Requirement->IsMet(State))
		{
			return Requirement->Describe();
		}
	}
	return FText::GetEmpty();
}

bool FDwEventResolver::NeedsChosenCrew(const FDwEventChoice& Choice)
{
	return Choice.Outcomes.ContainsByPredicate([](const UDwEventOutcome* Outcome) { return Outcome && Outcome->NeedsChosenCrew(); });
}

TArray<FGuid> FDwEventResolver::GetChoosableCrew(const FDwRunState& State, const FDwEventChoice& Choice)
{
	TArray<FGuid> Out;
	if (!NeedsChosenCrew(Choice))
	{
		return Out;
	}
	for (const FDwCrewMemberState& Member : State.Crew)
	{
		const bool bAccepted = Member.bAlive && !Choice.Outcomes.ContainsByPredicate([&Member](const UDwEventOutcome* Outcome)
			{ return Outcome && !Outcome->AcceptsChosen(Member); });
		if (bAccepted)
		{
			Out.Add(Member.Id);
		}
	}
	return Out;
}

int32 FDwEventResolver::PickDefaultChoice(const FDwRunState& State, FGuid& OutCrew)
{
	OutCrew.Invalidate();
	const UDwEventDefinition* Event = State.Event.Event;
	if (!Event || State.Event.IsResolved())
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < Event->Choices.Num(); ++Index)
	{
		const FDwEventChoice& Choice = Event->Choices[Index];
		if (!GetRequirementBlocker(State, Choice).IsEmpty())
		{
			continue;
		}
		if (NeedsChosenCrew(Choice))
		{
			const TArray<FGuid> Crew = GetChoosableCrew(State, Choice);
			if (Crew.IsEmpty())
			{
				continue;
			}
			OutCrew = Crew[0];
		}
		return Index;
	}
	return INDEX_NONE;
}

void FDwEventResolver::Apply(const FDwEventChoice& Choice, FDwEventContext& Context)
{
	for (const UDwEventOutcome* Outcome : Choice.Outcomes)
	{
		if (Outcome)
		{
			Outcome->Apply(Context);
		}
	}
}
