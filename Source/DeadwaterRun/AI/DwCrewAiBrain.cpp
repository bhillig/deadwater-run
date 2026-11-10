// Copyright Brandon Hillig 2026

#include "AI/DwCrewAiBrain.h"

#include "AI/DwAiProfile.h"
#include "AI/DwAiScoring.h"
#include "Abilities/DwGameplayAbility.h"
#include "Combat/DwCombatRules.h"
#include "Combat/DwCombatSnapshot.h"
#include "Core/DwGameplayTags.h"
#include "Core/DwLog.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/Tuning/DwTuningRows.h"

namespace DwCrewAiBrain
{
	/** Scores closer than this are a tie. */
	static constexpr float TieTolerance = 1e-3f;

	/** One ability (and, for Ability.Item, one item) the unit can pay for this turn. */
	struct FOption
	{
		const UDwAbilityDefinition* Ability = nullptr;
		const UDwGameplayAbility* CDO = nullptr;
		FName ItemId;
	};

	/** A preview that doesn't depend on where the unit stands (single-target patterns), computed once per decision. */
	struct FCachedPreview
	{
		int32 Option = INDEX_NONE;
		FDwAbilityTarget Target;
		FDwAbilityPreview Preview;
	};

	struct FTileCandidates
	{
		FIntPoint Tile = FIntPoint::ZeroValue;
		int32 Index = 0;
		float Rank = 0.f;
		TArray<FDwAiCandidate> Candidates;
	};

	static bool IsGroupPattern(EDwTargetPattern Pattern)
	{
		return Pattern == EDwTargetPattern::AllEnemies || Pattern == EDwTargetPattern::AllAllies || Pattern == EDwTargetPattern::RandomEnemies;
	}

	static bool IsTileIndependent(EDwTargetPattern Pattern)
	{
		// Who gets hit (and for how much) doesn't change with the source tile; only whether the target is valid does.
		return Pattern == EDwTargetPattern::SingleUnit || Pattern == EDwTargetPattern::SingleUnitOrShip
			|| Pattern == EDwTargetPattern::EnemyShip;
	}
}

FString FDwAiCandidate::ToString() const
{
	return FString::Printf(TEXT("(%d,%d) %s%s score %.2f [%s]"), MoveTile.X, MoveTile.Y,
		HasAction() ? *AbilityTag.ToString() : TEXT("no action"),
		HasAction() ? *FString::Printf(TEXT(" -> %s"), Target.bShip ? TEXT("ship") : *FString::Printf(TEXT("unit %d (%d,%d)"), Target.Unit.Id, Target.Tile.X, Target.Tile.Y)) : TEXT(""),
		Score, *Terms.ToString());
}

FDwAiTuningRow UDwCrewAiBrain::GetTuning() const
{
	if (TuningOverride.IsSet())
	{
		return TuningOverride.GetValue();
	}
	return Profile ? Profile->GetTuningValues() : FDwAiTuningRow();
}

TArray<FDwAiCandidate> UDwCrewAiBrain::EvaluateCandidates(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Unit) const
{
	using namespace DwCrewAiBrain;
	TArray<FDwAiCandidate> Result;
	const FDwUnitSnapshot* Self = Snapshot.FindUnit(Unit);
	if (!Self || !Self->bAlive)
	{
		return Result;
	}
	const FDwAiTuningRow Tuning = GetTuning();
	const FDwAiWeights& Weights = Tuning.Weights;
	const FDwAiScoringContext Context = FDwAiScoringContext::Make(Snapshot, Unit, Profile && Profile->bPreferCannons, Profile && Profile->bBoarders);

	// 1. Destinations: stay, or any tile the move ability itself would accept (4-way, pass allies, one move, C1-C3).
	TArray<FIntPoint> Tiles = { Self->Tile };
	const UDwAbilityDefinition* Move = FDwCombatQueries::FindAbility(*Self, DwTags::Ability_Move);
	if (Move && FDwCombatQueries::CanActivate(Snapshot, *Self, Move) == EDwCommandResult::Accepted)
	{
		FDwTargetingContext Targeting;
		Targeting.Snapshot = &Snapshot;
		Targeting.Ability = Move;
		Targeting.Source = Unit;
		TArray<FDwAbilityTarget> Reachable;
		FDwCombatQueries::GetAbilityCDO(Move)->GatherValidTargets(Targeting, Reachable);
		for (const FDwAbilityTarget& Target : Reachable)
		{
			Tiles.Add(Target.Tile);
		}
	}

	// 2. What the unit can pay for this turn. Free abilities without an MP cost are skipped: nothing would stop a loop.
	TArray<FOption> Options;
	for (const UDwAbilityDefinition* Ability : Self->Abilities)
	{
		const UDwGameplayAbility* CDO = FDwCombatQueries::GetAbilityCDO(Ability);
		const FDwAbilityTuningRow* AbilityTuning = UDwGameplayAbility::GetTuning(Ability);
		if (!CDO || Ability->AbilityTag == DwTags::Ability_Move
			|| (CDO->ActionCost == EDwActionCost::Free && (!AbilityTuning || AbilityTuning->ManaCost <= 0)))
		{
			continue;
		}
		TArray<FName> ItemIds;
		if (Ability->AbilityTag == DwTags::Ability_Item)
		{
			if (Self->Side != EDwSide::Player)
			{
				continue; // the inventory is the player's
			}
			for (const TPair<FName, int32>& Item : Snapshot.Items)
			{
				if (Item.Value > 0) { ItemIds.Add(Item.Key); }
			}
			ItemIds.Sort(FNameLexicalLess());
		}
		else
		{
			ItemIds.Add(NAME_None);
		}
		for (const FName ItemId : ItemIds)
		{
			if (FDwCombatQueries::CanActivate(Snapshot, *Self, Ability, ItemId) == EDwCommandResult::Accepted)
			{
				Options.Add({ Ability, CDO, ItemId });
			}
		}
	}
	// Conqueror's crew (Rule.Turn.SingleChoice): moving spends the action too.
	const bool bMoveSpendsAction = Self->Tags.HasTag(DwTags::Rule_Turn_SingleChoice);

	// 3. Every (tile, ability, target) with its preview, ranked without danger.
	TArray<FCachedPreview> Cache;
	TArray<FTileCandidates> PerTile;
	PerTile.Reserve(Tiles.Num());
	for (int32 TileIndex = 0; TileIndex < Tiles.Num(); ++TileIndex)
	{
		const FIntPoint Tile = Tiles[TileIndex];
		FTileCandidates& Entry = PerTile.AddDefaulted_GetRef();
		Entry.Tile = Tile;
		Entry.Index = TileIndex;

		FDwAiCandidate& Stand = Entry.Candidates.AddDefaulted_GetRef();
		Stand.MoveTile = Tile;
		FDwAiScoring::ScorePosition(Context, Tile, Stand.Terms);
		const float PositionScore = FDwAiWeights::Dot(Weights, Stand.Terms);
		float BestAction = 0.f;

		if (Tile != Self->Tile && bMoveSpendsAction)
		{
			Entry.Rank = PositionScore;
			continue;
		}
		for (int32 OptionIndex = 0; OptionIndex < Options.Num(); ++OptionIndex)
		{
			const FOption& Option = Options[OptionIndex];
			FDwTargetingContext Targeting;
			Targeting.Snapshot = &Snapshot;
			Targeting.Ability = Option.Ability;
			Targeting.Source = Unit;
			Targeting.ItemId = Option.ItemId;
			Targeting.SourceTileOverride = Tile;
			const EDwTargetPattern Pattern = Option.CDO->GetTargetingRules(Targeting).Pattern;

			TArray<FDwAbilityTarget> Targets;
			Option.CDO->GatherValidTargets(Targeting, Targets);
			if (IsGroupPattern(Pattern) && Targets.Num() > 1)
			{
				Targets.SetNum(1); // one command hits the whole group; any valid target aims it
			}
			for (const FDwAbilityTarget& Target : Targets)
			{
				FDwAiCandidate Candidate;
				Candidate.MoveTile = Tile;
				Candidate.AbilityTag = Option.Ability->AbilityTag;
				Candidate.Target = Target;
				Candidate.ItemId = Option.ItemId;
				if (IsTileIndependent(Pattern))
				{
					const FCachedPreview* Cached = Cache.FindByPredicate([&](const FCachedPreview& C) { return C.Option == OptionIndex && C.Target == Target; });
					if (!Cached)
					{
						Cached = &Cache.Add_GetRef({ OptionIndex, Target, Option.CDO->PreviewOutcome(Targeting, Target) });
					}
					Candidate.Preview = Cached->Preview;
				}
				else
				{
					Candidate.Preview = Option.CDO->PreviewOutcome(Targeting, Target);
				}
				if (!Candidate.Preview.bValid)
				{
					continue;
				}
				FDwAiWeights ActionTerms;
				FDwAiScoring::ScoreAction(Context, Tile, *Option.Ability, Candidate.Preview, ActionTerms);
				BestAction = FMath::Max(BestAction, FDwAiWeights::Dot(Weights, ActionTerms));
				Entry.Candidates.Add(MoveTemp(Candidate));
			}
		}
		Entry.Rank = PositionScore + BestAction;
	}

	// 4. Keep the unit's own tile and the best-ranked destinations, then score them fully (danger included).
	if (Tuning.MaxMoveCandidates > 0 && PerTile.Num() > Tuning.MaxMoveCandidates + 1)
	{
		TArray<FTileCandidates> Moves(MoveTemp(PerTile));
		PerTile.Reset();
		PerTile.Add(MoveTemp(Moves[0]));
		Moves.RemoveAt(0);
		Moves.StableSort([](const FTileCandidates& A, const FTileCandidates& B) { return A.Rank > B.Rank; });
		Moves.SetNum(Tuning.MaxMoveCandidates);
		Moves.StableSort([](const FTileCandidates& A, const FTileCandidates& B) { return A.Index < B.Index; }); // board order again
		PerTile.Append(MoveTemp(Moves));
	}
	for (FTileCandidates& Entry : PerTile)
	{
		const float Danger = FDwAiScoring::DangerAt(Context, Entry.Tile);
		for (FDwAiCandidate& Candidate : Entry.Candidates)
		{
			FDwAiScoring::ScoreCandidate(Context, Candidate, Candidate.HasAction() ? FDwCombatQueries::FindAbility(*Self, Candidate.AbilityTag) : nullptr,
				Weights, &Danger);
			Result.Add(MoveTemp(Candidate));
		}
	}
	Result.StableSort([](const FDwAiCandidate& A, const FDwAiCandidate& B) { return A.Score > B.Score; });
	return Result;
}

const FDwAiCandidate* UDwCrewAiBrain::Pick(const TArray<FDwAiCandidate>& Candidates, float Temperature, FIntPoint UnitTile, FRandomStream& Rng)
{
	using namespace DwCrewAiBrain;
	if (Candidates.IsEmpty())
	{
		return nullptr;
	}
	const float Best = Candidates[0].Score; // sorted best first
	if (Temperature > 0.f)
	{
		float Total = 0.f;
		TArray<float, TInlineAllocator<64>> Weights;
		for (const FDwAiCandidate& Candidate : Candidates)
		{
			Total += Weights.Add_GetRef(FMath::Exp((Candidate.Score - Best) / Temperature));
		}
		float Roll = Rng.FRand() * Total;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			Roll -= Weights[Index];
			if (Roll <= 0.f)
			{
				return &Candidates[Index];
			}
		}
		return &Candidates.Last();
	}

	// Ties: standing still reads better than a pointless step, so the tie goes to candidates that don't move; any
	// remaining tie is drawn from the stream.
	TArray<int32, TInlineAllocator<16>> Tied;
	for (int32 Index = 0; Index < Candidates.Num() && Candidates[Index].Score >= Best - TieTolerance; ++Index)
	{
		Tied.Add(Index);
	}
	if (Tied.ContainsByPredicate([&](int32 Index) { return Candidates[Index].MoveTile == UnitTile; }))
	{
		Tied.RemoveAll([&](int32 Index) { return Candidates[Index].MoveTile != UnitTile; });
	}
	return Tied.Num() == 1 ? &Candidates[Tied[0]] : &Candidates[Tied[Rng.RandRange(0, Tied.Num() - 1)]];
}

FDwCombatCommand UDwCrewAiBrain::ToCommand(const FDwAiCandidate& Candidate, const FDwUnitSnapshot& Unit, FDwAiTurnContext& Context)
{
	if (Candidate.MoveTile != Unit.Tile)
	{
		Context.bHasPlan = Candidate.HasAction();
		Context.Plan = Candidate;
		return FDwCombatCommand::MakeMove(Unit.Handle, Candidate.MoveTile);
	}
	if (Candidate.HasAction())
	{
		return FDwCombatCommand::MakeAbility(Unit.Handle, Candidate.AbilityTag, Candidate.Target, Candidate.ItemId);
	}
	return FDwCombatCommand::MakeEndUnit(Unit.Handle);
}

FDwCombatCommand UDwCrewAiBrain::ChooseNextCommand(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Unit, FDwAiTurnContext& Context,
	FRandomStream& Rng) const
{
	++Context.Commands;
	const FDwUnitSnapshot* Self = Snapshot.FindUnit(Unit);
	if (!Self || !Self->bAlive)
	{
		return FDwCombatCommand::MakeEndUnit(Unit);
	}

	// Moved last call with an action in mind: do it if it is still legal from here.
	if (Context.bHasPlan)
	{
		const FDwAiCandidate Plan = Context.Plan;
		Context.bHasPlan = false;
		if (Plan.HasAction() && Plan.MoveTile == Self->Tile)
		{
			const FDwCombatCommand Planned = FDwCombatCommand::MakeAbility(Unit, Plan.AbilityTag, Plan.Target, Plan.ItemId);
			if (FDwCombatQueries::ValidateCommand(Snapshot, Planned) == EDwCommandResult::Accepted)
			{
				return Planned;
			}
		}
	}

	const TArray<FDwAiCandidate> Candidates = EvaluateCandidates(Snapshot, Unit);
	const FDwAiCandidate* Choice = Pick(Candidates, GetTuning().Temperature, Self->Tile, Rng);
	if (!Choice)
	{
		return FDwCombatCommand::MakeEndUnit(Unit);
	}
	UE_LOG(LogDwAI, Verbose, TEXT("Unit %d chose %s (of %d)"), Unit.Id, *Choice->ToString(), Candidates.Num());
	return ToCommand(*Choice, *Self, Context);
}
