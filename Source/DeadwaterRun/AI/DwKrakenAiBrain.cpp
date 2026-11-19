// Copyright Brandon Hillig 2026

#include "AI/DwKrakenAiBrain.h"

#include "Abilities/DwGameplayAbility.h"
#include "AI/DwAiProfile.h"
#include "AI/DwAiScoring.h"
#include "Combat/DwCombatRules.h"
#include "Combat/DwCombatSnapshot.h"
#include "Core/DwGameplayTags.h"
#include "Core/DwLog.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwKrakenDefinition.h"

namespace DwKrakenAiBrain
{
	static constexpr float TieTolerance = 1e-3f;

	/** Used when the brain has no profile (tests): the same spirit as DA_AiProfile_Kraken's row. */
	static FDwAiTuningRow MakeDefaultTuning()
	{
		FDwAiTuningRow Row;
		Row.Weights.Damage = 1.f;
		Row.Weights.Kill = 40.f;
		Row.Weights.Win = 1000.f;
		Row.Weights.ShipDamage = 0.25f;
		Row.Weights.ShipLowHull = 1.f;
		Row.Weights.LowHealthFocus = 20.f;
		Row.Weights.CaptainPressure = 0.5f;
		return Row;
	}

	static bool IsGroupPattern(EDwTargetPattern Pattern)
	{
		return Pattern == EDwTargetPattern::AllEnemies || Pattern == EDwTargetPattern::AllAllies || Pattern == EDwTargetPattern::RandomEnemies;
	}

	/** The scratch snapshot after Preview lands: Health down, the dead off the board (later moves in the plan see it). */
	static void ApplyPredictedHits(FDwCombatSnapshot& Scratch, const FDwAbilityPreview& Preview, EDwTargetPattern Pattern)
	{
		if (Pattern == EDwTargetPattern::RandomEnemies)
		{
			return; // who gets hit is drawn when it lands
		}
		for (const FDwTargetPreview& Hit : Preview.Targets)
		{
			if (Hit.Target.IsUnit())
			{
				FDwUnitSnapshot* Victim = Scratch.Units.FindByPredicate([&Hit](const FDwUnitSnapshot& U) { return U.Handle == Hit.Target.Unit; });
				if (Victim && Victim->bAlive && Hit.Damage > 0)
				{
					Victim->Health = FMath::Max(0.f, Victim->Health - Hit.Damage);
					if (Victim->Health <= 0.f)
					{
						Victim->bAlive = false;
						Scratch.Grid.ClearOccupant(Victim->Tile);
					}
				}
			}
			else if (Hit.Target.bShip && Hit.Damage > 0)
			{
				FDwShipSnapshot* Ship = Scratch.Ships.FindByPredicate([&Hit](const FDwShipSnapshot& S) { return S.Side == Hit.Target.ShipSide; });
				if (Ship)
				{
					Ship->Health = FMath::Max(0.f, Ship->Health - Hit.Damage);
				}
			}
		}
	}
}

void UDwKrakenAiBrain::Configure(const UDwAiProfile* InProfile, const FDwEncounterSetup& Setup)
{
	Profile = InProfile;
	Kraken = Setup.Kraken;
}

FDwAiTuningRow UDwKrakenAiBrain::GetTuning() const
{
	if (TuningOverride.IsSet())
	{
		return TuningOverride.GetValue();
	}
	return Profile ? Profile->GetTuningValues() : DwKrakenAiBrain::MakeDefaultTuning();
}

int32 UDwKrakenAiBrain::GetActionsNextTurn(const FDwUnitSnapshot& Self)
{
	return FMath::Max(1, FMath::RoundToInt(Self.MaxActionPoints));
}

TArray<TPair<const UDwAbilityDefinition*, float>> UDwKrakenAiBrain::GetMoveOptions(const FDwCombatSnapshot& Snapshot, const FDwUnitSnapshot& Self) const
{
	TArray<TPair<const UDwAbilityDefinition*, float>> Options;
	auto Allowed = [&Snapshot](const UDwAbilityDefinition* Ability)
	{
		// The phase gate (DW-301): outside a Kraken fight (tests) every granted move is allowed.
		return Ability && (!Snapshot.Kraken.IsSet() || Snapshot.Kraken.PhaseMoves.HasTagExact(Ability->AbilityTag));
	};
	const FDwKrakenPhase* Phase = Kraken && Kraken->Phases.IsValidIndex(Snapshot.Kraken.PhaseIndex) ? &Kraken->Phases[Snapshot.Kraken.PhaseIndex] : nullptr;
	if (Phase)
	{
		for (const FDwWeightedAbility& Move : Phase->Moves)
		{
			const UDwAbilityDefinition* Granted = Move.Ability ? FDwCombatQueries::FindAbility(Self, Move.Ability->AbilityTag) : nullptr;
			if (Granted && Move.Weight > 0.f && Allowed(Granted))
			{
				Options.Emplace(Granted, Move.Weight);
			}
		}
		return Options;
	}
	// No definition: the gate alone decides (and outside a Kraken fight, anything but walking - the body never moves).
	for (const UDwAbilityDefinition* Ability : Self.Abilities)
	{
		if (Allowed(Ability) && !Ability->AbilityTag.MatchesTagExact(DwTags::Ability_Move))
		{
			Options.Emplace(Ability, 1.f);
		}
	}
	return Options;
}

bool UDwKrakenAiBrain::ChooseTarget(const FDwCombatSnapshot& Snapshot, const FDwUnitSnapshot& Self, const UDwAbilityDefinition& Move,
	FRandomStream& Rng, FDwAbilityTarget& OutTarget) const
{
	using namespace DwKrakenAiBrain;
	const UDwGameplayAbility* CDO = FDwCombatQueries::GetAbilityCDO(&Move);
	if (!CDO)
	{
		return false;
	}
	FDwTargetingContext Targeting;
	Targeting.Snapshot = &Snapshot;
	Targeting.Ability = &Move;
	Targeting.Source = Self.Handle;
	const FDwTargetingRules& Rules = CDO->GetTargetingRules(Targeting);

	TArray<FDwAbilityTarget> Valid;
	CDO->GatherValidTargets(Targeting, Valid);
	if (Valid.IsEmpty())
	{
		return false;
	}
	if (Rules.Pattern == EDwTargetPattern::Self)
	{
		OutTarget = FDwAbilityTarget::MakeUnit(Self.Handle, Self.Tile);
		return true;
	}
	if (IsGroupPattern(Rules.Pattern))
	{
		OutTarget = FDwAbilityTarget();
		return true;
	}

	// Tile-aimable moves aim at the tile under a victim (dodgeable); others at the unit. Empty tiles are never chosen.
	TArray<FDwAbilityTarget> Options;
	for (const FDwAbilityTarget& Target : Valid)
	{
		if (Target.bShip)
		{
			Options.Add(Target);
		}
		else if (Rules.bAimAtTile ? !Target.IsUnit() && Snapshot.Grid.IsOccupied(Target.Tile) : Target.IsUnit())
		{
			Options.Add(Target);
		}
	}

	const FDwAiTuningRow Tuning = GetTuning();
	const FDwAiScoringContext Scoring = FDwAiScoringContext::Make(Snapshot, Self.Handle, false, false);
	TArray<TPair<float, int32>, TInlineAllocator<16>> Scored;
	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		const FDwAbilityPreview Preview = CDO->PreviewOutcome(Targeting, Options[Index]);
		if (!Preview.bValid || Preview.Targets.IsEmpty())
		{
			continue;
		}
		FDwAiWeights Terms;
		FDwAiScoring::ScoreAction(Scoring, Self.Tile, Move, Preview, Terms);
		float Score = FDwAiWeights::Dot(Tuning.Weights, Terms);

		const bool bDealsDamage = Preview.Targets.ContainsByPredicate([](const FDwTargetPreview& Hit) { return Hit.Damage > 0; });
		if (!bDealsDamage)
		{
			// Control (Grab): pin the hardest hitter; rooting someone already rooted is wasted.
			const FDwUnitSnapshot* Victim = Snapshot.FindUnit(Preview.Targets[0].Target.Unit);
			if (!Victim || Victim->Tags.HasTagExact(DwTags::Status_Rooted))
			{
				continue;
			}
			Score += Victim->AttackDamage;
		}
		Scored.Emplace(Score, Index);
	}
	if (Scored.IsEmpty())
	{
		return false;
	}
	Scored.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key > B.Key || (A.Key == B.Key && A.Value < B.Value); });
	int32 Tied = 1;
	while (Tied < Scored.Num() && Scored[Tied].Key >= Scored[0].Key - TieTolerance)
	{
		++Tied;
	}
	OutTarget = Options[Scored[Tied == 1 ? 0 : Rng.RandRange(0, Tied - 1)].Value];
	return true;
}

TArray<FDwAiIntent> UDwKrakenAiBrain::PlanMoves(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Unit, int32 Count, FRandomStream& Rng) const
{
	using namespace DwKrakenAiBrain;
	TArray<FDwAiIntent> Plan;
	const FDwUnitSnapshot* Self = Snapshot.FindUnit(Unit);
	if (!Self || !Self->bAlive)
	{
		return Plan;
	}

	FDwCombatSnapshot Scratch = Snapshot;
	// Eyes of the Sea's extra action already pending: planning it again would chain double turns forever.
	bool bExtraActionPending = Snapshot.Kraken.IsSet() && Self->MaxActionPoints > Snapshot.Kraken.BaseActions + 0.5f;
	for (int32 Step = 0; Step < Count; ++Step)
	{
		const FDwUnitSnapshot* ScratchSelf = Scratch.FindUnit(Unit);
		TArray<TPair<const UDwAbilityDefinition*, float>> Options = GetMoveOptions(Scratch, *ScratchSelf);
		Options.RemoveAll([bExtraActionPending](const TPair<const UDwAbilityDefinition*, float>& Option)
		{
			return bExtraActionPending && Option.Key->AbilityTag.MatchesTagExact(DwTags::Ability_Kraken_EyesOfTheSea);
		});

		// Weighted draw; a move with nobody to hit is dropped and the draw repeats.
		while (!Options.IsEmpty())
		{
			float Total = 0.f;
			for (const TPair<const UDwAbilityDefinition*, float>& Option : Options)
			{
				Total += Option.Value;
			}
			float Roll = Rng.FRand() * Total;
			int32 Picked = Options.Num() - 1;
			for (int32 Index = 0; Index < Options.Num(); ++Index)
			{
				Roll -= Options[Index].Value;
				if (Roll <= 0.f)
				{
					Picked = Index;
					break;
				}
			}
			const UDwAbilityDefinition* Move = Options[Picked].Key;
			FDwAbilityTarget Target;
			if (!ChooseTarget(Scratch, *ScratchSelf, *Move, Rng, Target))
			{
				Options.RemoveAt(Picked);
				continue;
			}

			FDwAiIntent& Intent = Plan.AddDefaulted_GetRef();
			Intent.AbilityTag = Move->AbilityTag;
			Intent.Target = Target;
			Intent.bTelegraph = true;

			if (Move->AbilityTag.MatchesTagExact(DwTags::Ability_Kraken_EyesOfTheSea))
			{
				bExtraActionPending = true;
			}
			if (const UDwGameplayAbility* CDO = FDwCombatQueries::GetAbilityCDO(Move))
			{
				FDwTargetingContext Targeting;
				Targeting.Snapshot = &Scratch;
				Targeting.Ability = Move;
				Targeting.Source = Unit;
				const FDwAbilityPreview Preview = CDO->PreviewOutcome(Targeting, Target);
				ApplyPredictedHits(Scratch, Preview, CDO->GetTargetingRules(Targeting).Pattern);
			}
			break;
		}
	}
	return Plan;
}

TArray<FDwAiIntent> UDwKrakenAiBrain::DeclareIntents(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Unit, FRandomStream& Rng) const
{
	const FDwUnitSnapshot* Self = Snapshot.FindUnit(Unit);
	if (!Self || !Self->bAlive)
	{
		return {};
	}
	TArray<FDwAiIntent> Plan = PlanMoves(Snapshot, Unit, GetActionsNextTurn(*Self), Rng);
	for (const FDwAiIntent& Intent : Plan)
	{
		UE_LOG(LogDwAI, Log, TEXT("Kraken %d intends %s at %s(%d,%d)"), Unit.Id, *Intent.AbilityTag.ToString(),
			Intent.Target.bShip ? TEXT("ship ") : Intent.Target.IsUnit() ? TEXT("unit ") : TEXT("tile "), Intent.Target.Tile.X, Intent.Target.Tile.Y);
	}
	return Plan;
}

bool UDwKrakenAiBrain::ShouldRedeclare(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Unit) const
{
	const FDwUnitSnapshot* Self = Snapshot.FindUnit(Unit);
	if (!Self || !Self->bAlive)
	{
		return false;
	}
	const FDwUnitIntents* Declared = Snapshot.FindIntents(Unit);
	if (!Declared || Declared->Intents.Num() != FMath::Max(1, FMath::RoundToInt(Self->ActionPoints)))
	{
		return true; // nothing shown yet, or the action count changed since
	}
	for (const FDwAiIntent& Intent : Declared->Intents)
	{
		const UDwAbilityDefinition* Ability = FDwCombatQueries::FindAbility(*Self, Intent.AbilityTag);
		if (!Ability || (Snapshot.Kraken.IsSet() && !Snapshot.Kraken.PhaseMoves.HasTagExact(Intent.AbilityTag)))
		{
			return true; // B7: a phase change at turn start retired the declared moves
		}
	}
	return false;
}

FDwCombatCommand UDwKrakenAiBrain::ChooseNextCommand(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Unit, FDwAiTurnContext& Context,
	FRandomStream& Rng) const
{
	++Context.Commands;
	const FDwUnitSnapshot* Self = Snapshot.FindUnit(Unit);
	if (!Self || !Self->bAlive || Self->ActionPoints < 1.f)
	{
		return FDwCombatCommand::MakeEndUnit(Unit);
	}

	// What the player was shown, in order. A tile aim at a tile the victim left is still legal: it lands and misses.
	const FDwUnitIntents* Declared = Snapshot.FindIntents(Unit);
	if (Declared && Declared->Intents.IsValidIndex(Context.Actions))
	{
		const FDwAiIntent& Intent = Declared->Intents[Context.Actions];
		const FDwCombatCommand Command = FDwCombatCommand::MakeAbility(Unit, Intent.AbilityTag, Intent.Target);
		if (FDwCombatQueries::ValidateCommand(Snapshot, Command) == EDwCommandResult::Accepted)
		{
			++Context.Actions;
			return Command;
		}
		UE_LOG(LogDwAI, Log, TEXT("Kraken %d: declared %s is no longer possible; choosing again"), Unit.Id, *Intent.AbilityTag.ToString());
	}

	// Undeclared action (or a stale intent): choose now.
	const TArray<FDwAiIntent> Now = PlanMoves(Snapshot, Unit, 1, Rng);
	if (!Now.IsEmpty())
	{
		const FDwCombatCommand Command = FDwCombatCommand::MakeAbility(Unit, Now[0].AbilityTag, Now[0].Target);
		if (FDwCombatQueries::ValidateCommand(Snapshot, Command) == EDwCommandResult::Accepted)
		{
			++Context.Actions;
			return Command;
		}
	}
	return FDwCombatCommand::MakeEndUnit(Unit);
}
