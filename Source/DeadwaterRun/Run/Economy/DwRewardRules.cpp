// Copyright Brandon Hillig 2026

#include "Run/Economy/DwRewardRules.h"

#include "Data/DwAbilityDefinition.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwPools.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"

float FDwRewardRules::GetLootMultiplier(EDwCombatOutcome Outcome, const FDwEconomyTuningRow& Economy)
{
	switch (Outcome)
	{
	case EDwCombatOutcome::VictorySunk:         return Economy.SinkLootMultiplier;
	case EDwCombatOutcome::VictoryCrewDefeated: return Economy.PlunderLootMultiplier;
	case EDwCombatOutcome::VictorySurrender:    return Economy.SurrenderLootMultiplier;
	default:                                    return 0.f; // the boss ends the run; defeats pay nothing
	}
}

bool FDwRewardRules::IsPlunder(EDwCombatOutcome Outcome)
{
	return Outcome == EDwCombatOutcome::VictoryCrewDefeated || Outcome == EDwCombatOutcome::VictorySurrender;
}

FDwRewardBundle FDwRewardRules::ComputeRewards(const FDwRewardContext& Context, FRandomStream& Random)
{
	FDwRewardBundle Bundle;
	Bundle.Outcome = Context.Outcome;
	if (!Context.Economy || !DwCombat::IsVictory(Context.Outcome))
	{
		return Bundle;
	}
	const FDwEconomyTuningRow& Economy = *Context.Economy;

	// Gold.
	const int32 BaseGold = Context.Encounter ? Context.Encounter->BaseGold : 0;
	Bundle.Gold = FMath::Max(0, FMath::RoundToInt(BaseGold * GetLootMultiplier(Context.Outcome, Economy)));

	// Plunder item (P6). The roll is always drawn on plunder, so the chance never shifts the ability draw below.
	if (IsPlunder(Context.Outcome) && !Context.PlunderItems.IsEmpty())
	{
		const float Roll = Random.GetFraction();
		const int32 Pick = Random.RandHelper(Context.PlunderItems.Num());
		if (Roll < Economy.PlunderItemChance && Context.PlunderItems[Pick])
		{
			FDwItemStack& Stack = Bundle.Items.AddDefaulted_GetRef();
			Stack.Item = Context.PlunderItems[Pick];
			Stack.Count = 1;
		}
	}

	// A new move (P5).
	const bool bGrantsAbility = !Context.Encounter || Context.Encounter->bGrantsAbilityChoice;
	if (bGrantsAbility && Context.State && Context.Outcome != EDwCombatOutcome::VictoryBoss)
	{
		Bundle.AbilityChoices = RollAbilityChoices(*Context.State, Economy.RewardAbilityChoices, Random);
	}
	return Bundle;
}

bool FDwRewardRules::CanLearn(const FDwRunState& State, const FDwCrewMemberState& Member, const UDwAbilityDefinition& Ability)
{
	if (!Member.bAlive || !Member.Unit)
	{
		return false;
	}
	const UDwUnitDefinition& Unit = *Member.Unit;
	auto Is = [&Ability](const UDwAbilityDefinition* Known) { return Known == &Ability; };
	if (Member.Abilities.ContainsByPredicate(Is) || Unit.BasicAttack.Get() == &Ability || Unit.DefaultAbilities.ContainsByPredicate(Is))
	{
		return false; // already known
	}
	if (Member.bIsCaptain && State.Captain)
	{
		const FGameplayTagContainer& Blocked = State.Captain->BlockedAbilityTags;
		if (!Blocked.IsEmpty() && (Ability.AbilityTags.HasAny(Blocked) || Ability.AbilityTag.MatchesAny(Blocked)))
		{
			return false; // Conqueror: no firearms (K2)
		}
	}
	return true;
}

TArray<FDwAbilityOffer> FDwRewardRules::RollAbilityChoices(const FDwRunState& State, int32 Count, FRandomStream& Random)
{
	struct FCandidate
	{
		FDwAbilityOffer Offer;
		float Weight = 0.f;
	};

	// Roster order, then pool order: the candidate list (and so the draw) is stable for a given state.
	TArray<FCandidate> Candidates;
	for (const FDwCrewMemberState& Member : State.Crew)
	{
		const UDwAbilityPool* Pool = Member.bAlive && Member.Unit ? Member.Unit->LearnablePool.Get() : nullptr;
		if (!Pool)
		{
			continue;
		}
		for (const FDwWeightedAbility& Entry : Pool->Abilities)
		{
			if (!Entry.Ability || Entry.Weight <= 0.f || !CanLearn(State, Member, *Entry.Ability))
			{
				continue;
			}
			FCandidate* Existing = Candidates.FindByPredicate([&Entry](const FCandidate& C) { return C.Offer.Ability == Entry.Ability; });
			if (!Existing)
			{
				Existing = &Candidates.AddDefaulted_GetRef();
				Existing->Offer.Ability = Entry.Ability;
			}
			Existing->Offer.EligibleCrew.AddUnique(Member.Id);
			Existing->Weight = FMath::Max(Existing->Weight, Entry.Weight); // shared pools don't make an ability likelier
		}
	}

	TArray<FDwAbilityOffer> Choices;
	while (Choices.Num() < Count && !Candidates.IsEmpty())
	{
		float Total = 0.f;
		for (const FCandidate& Candidate : Candidates)
		{
			Total += Candidate.Weight;
		}
		float Roll = Random.GetFraction() * Total;
		int32 Pick = Candidates.Num() - 1;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			Roll -= Candidates[Index].Weight;
			if (Roll < 0.f)
			{
				Pick = Index;
				break;
			}
		}
		Choices.Add(MoveTemp(Candidates[Pick].Offer));
		Candidates.RemoveAt(Pick); // keeps order: the next draw stays stable
	}
	return Choices;
}
