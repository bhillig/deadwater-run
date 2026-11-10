// Copyright Brandon Hillig 2026

#include "AI/DwAiScoring.h"

#include "Abilities/DwDamageRules.h"
#include "Abilities/DwGameplayAbility.h"
#include "Board/DwGridQueries.h"
#include "Combat/DwCombatRules.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/Tuning/DwTuningRows.h"

namespace DwAiScoring
{
	/** Stands in for "unlimited" (bIgnoreRange) without overflowing range arithmetic. */
	static constexpr int32 UnlimitedRange = 1000;

	static FDwTargetingContext MakeTargeting(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Source, const UDwAbilityDefinition* Ability)
	{
		FDwTargetingContext Context;
		Context.Snapshot = &Snapshot;
		Context.Ability = Ability;
		Context.Source = Source;
		return Context;
	}

	static int32 GetRange(const FDwTargetingRules& Rules, const FDwAbilityTuningRow& Tuning)
	{
		return Rules.bIgnoreRange ? UnlimitedRange : Tuning.Range;
	}

	static uint8 GetElevation(const FDwGridModel& Grid, FIntPoint Tile)
	{
		const FDwTile* Data = Grid.GetTile(Tile);
		return Data ? Data->Elevation : 0;
	}

	static int32 CountAlive(const FDwCombatSnapshot& Snapshot, EDwSide Side)
	{
		int32 Count = 0;
		for (const FDwUnitSnapshot& Unit : Snapshot.Units)
		{
			Count += Unit.Side == Side && Unit.bAlive ? 1 : 0;
		}
		return Count;
	}

	/** The side's turn will be skipped: frozen, already cowering, or at MaxFear (it cowers at its turn start). */
	static bool WillSkipNextTurn(const FDwUnitSnapshot& Unit)
	{
		return Unit.Tags.HasTag(DwTags::Status_Incapacitated)
			|| FDwFearRules::ShouldCower(Unit.Fear, Unit.MaxFear, Unit.Tags.HasTagExact(DwTags::Unit_Immune_Fear));
	}

	static float DangerAt(const FDwAiScoringContext& Context, FIntPoint Tile, TConstArrayView<FDwUnitHandle> Ignored)
	{
		const FDwGridModel& Grid = Context.Snapshot->Grid;
		const FDwGridRules& Rules = Context.Snapshot->GridRules;
		const bool bElevated = GetElevation(Grid, Tile) > 0;
		float Total = 0.f;
		for (const FDwAiThreat& Threat : Context.Threats)
		{
			if (Ignored.Contains(Threat.Unit))
			{
				continue;
			}
			float Best = 0.f;
			for (const FDwAiThreatOption& Option : Threat.Options)
			{
				if (Option.Damage <= Best || (bElevated && !Option.bCanTargetElevated))
				{
					continue;
				}
				for (const FIntPoint& From : Threat.From)
				{
					if (From == Tile || (!Option.bCanTargetElevated && GetElevation(Grid, From) > 0))
					{
						continue;
					}
					if (FDwTargetingQuery::IsInRange(Grid, Rules, From, Tile, Option.Range)
						&& (!Option.bRequiresLineOfSight || FDwLineOfSight::HasLineOfSight(Grid, From, Tile)))
					{
						Best = Option.Damage;
						break;
					}
				}
			}
			Total += Best;
		}
		return Total;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Context

bool FDwAiScoring::IsUnitAttack(const UDwAbilityDefinition& Ability, const FDwTargetingContext& Context)
{
	const UDwGameplayAbility* CDO = FDwCombatQueries::GetAbilityCDO(&Ability);
	const FDwAbilityTuningRow* Tuning = UDwGameplayAbility::GetTuning(&Ability);
	if (!CDO || !Tuning || (Tuning->Damage <= 0 && Tuning->DamageScale <= 0.f)
		|| Ability.AbilityTag == DwTags::Ability_Move || Ability.AbilityTag == DwTags::Ability_Item)
	{
		return false;
	}
	const FDwTargetingRules& Rules = CDO->GetTargetingRules(Context);
	const bool bUnitPattern = Rules.Pattern == EDwTargetPattern::SingleUnit || Rules.Pattern == EDwTargetPattern::SingleUnitOrShip
		|| Rules.Pattern == EDwTargetPattern::AllEnemies || Rules.Pattern == EDwTargetPattern::RandomEnemies;
	return bUnitPattern && Rules.Side != EDwTargetSide::Ally && Rules.RequiredSourceTileTags.IsEmpty();
}

FDwAiScoringContext FDwAiScoringContext::Make(const FDwCombatSnapshot& Snapshot, FDwUnitHandle InUnit, bool bInPreferCannons, bool bInBoarders)
{
	using namespace DwAiScoring;
	FDwAiScoringContext Context;
	Context.Snapshot = &Snapshot;
	Context.Unit = InUnit;
	Context.bPreferCannons = bInPreferCannons;
	Context.bBoarders = bInBoarders;
	const FDwUnitSnapshot* Self = Snapshot.FindUnit(InUnit);
	if (!Self)
	{
		return Context;
	}
	Context.Side = Self->Side;
	const EDwSide Opponents = Context.GetOpponentSide();

	// Own attacks: how far each reaches (Approach) and whether height helps (HighGround).
	for (const UDwAbilityDefinition* Ability : Self->Abilities)
	{
		const FDwTargetingContext Targeting = MakeTargeting(Snapshot, InUnit, Ability);
		if (Ability && FDwAiScoring::IsUnitAttack(*Ability, Targeting))
		{
			const FDwTargetingRules& Rules = FDwCombatQueries::GetAbilityCDO(Ability)->GetTargetingRules(Targeting);
			FDwAiReach& Reach = Context.Reach.AddDefaulted_GetRef();
			Reach.Range = GetRange(Rules, *UDwGameplayAbility::GetTuning(Ability));
			Reach.bCanTargetElevated = Rules.bCanTargetElevated;
			Reach.bRanged = Reach.Range >= 2 && Rules.bCanTargetElevated;
		}
	}

	// Opponents that will act next turn: every tile they can attack from, and their attacks against this unit.
	const int32 Allies = FMath::Max(1, CountAlive(Snapshot, Context.Side));
	for (const FDwUnitSnapshot& Opponent : Snapshot.Units)
	{
		if (Opponent.Side != Opponents || !Opponent.bAlive || WillSkipNextTurn(Opponent))
		{
			continue;
		}
		FDwAiThreat Threat;
		Threat.Unit = Opponent.Handle;
		for (const UDwAbilityDefinition* Ability : Opponent.Abilities)
		{
			const FDwTargetingContext Targeting = MakeTargeting(Snapshot, Opponent.Handle, Ability);
			if (!Ability || !FDwAiScoring::IsUnitAttack(*Ability, Targeting))
			{
				continue;
			}
			const UDwGameplayAbility* CDO = FDwCombatQueries::GetAbilityCDO(Ability);
			const FDwAbilityTuningRow& Tuning = *UDwGameplayAbility::GetTuning(Ability);
			const FDwTargetingRules& Rules = CDO->GetTargetingRules(Targeting);
			if (Tuning.ManaCost > Opponent.Mana || Opponent.Tags.HasAny(CDO->GetBlockingTags())
				|| !Self->Tags.HasAll(Rules.RequiredTargetTags) || Self->Tags.HasAny(Rules.BlockedTargetTags))
			{
				continue;
			}
			FDwDamageInput Input;
			Input.Flat = Tuning.Damage;
			Input.Scale = Tuning.DamageScale;
			Input.AttackDamage = Opponent.AttackDamage;
			Input.DamageBonus = Opponent.DamageBonus;
			Input.DamageReduction = Self->DamageReduction;
			float Damage = static_cast<float>(FDwDamageRules::Compute(Input).Damage);
			if (Rules.Pattern == EDwTargetPattern::RandomEnemies)
			{
				// Random hits spread over this unit's side: the expected share for one unit.
				const int32 MinHits = FMath::Max(1, Tuning.MinHits);
				const int32 MaxHits = FMath::Max(MinHits, Tuning.MaxHits);
				Damage *= 0.5f * (MinHits + MaxHits) / Allies;
			}
			FDwAiThreatOption& Option = Threat.Options.AddDefaulted_GetRef();
			Option.Range = GetRange(Rules, Tuning);
			Option.bRequiresLineOfSight = Rules.bRequiresLineOfSight;
			Option.bCanTargetElevated = Rules.bCanTargetElevated;
			Option.Damage = Damage;
		}
		if (Threat.Options.IsEmpty())
		{
			continue;
		}
		Threat.From.Add(Opponent.Tile);
		const UDwAbilityDefinition* Move = FDwCombatQueries::FindAbility(Opponent, DwTags::Ability_Move);
		const UDwGameplayAbility* MoveCDO = FDwCombatQueries::GetAbilityCDO(Move);
		if (MoveCDO && Opponent.MoveRange > 0.f && !Opponent.Tags.HasAny(MoveCDO->GetBlockingTags()))
		{
			const TMap<FIntPoint, int32> Reachable = FDwPathfinder::ComputeReachable(Snapshot.Grid, Snapshot.GridRules, Opponent.Tile,
				FMath::FloorToInt(Opponent.MoveRange), Opponent.Side);
			TArray<FIntPoint> Tiles;
			Reachable.GetKeys(Tiles);
			const FDwGridModel& Grid = Snapshot.Grid;
			Tiles.Sort([&Grid](const FIntPoint& A, const FIntPoint& B) { return Grid.ToIndex(A) < Grid.ToIndex(B); });
			Threat.From.Append(Tiles);
		}
		Context.Threats.Add(MoveTemp(Threat));
	}

	// Our cannons could finish the opponent's ship: standing on it is a risk (C12 for the player side).
	const FDwShipSnapshot* OwnShip = Snapshot.FindShip(Context.Side);
	const FDwShipSnapshot* OpponentShip = Snapshot.FindShip(Opponents);
	Context.bOpponentShipCanSink = OwnShip && OpponentShip && OpponentShip->Health > 0.f
		&& OpponentShip->Health <= OwnShip->CannonDamage * OwnShip->CannonTiles.Num();
	return Context;
}

// ---------------------------------------------------------------------------------------------------------------------
// Terms

void FDwAiScoring::ScorePosition(const FDwAiScoringContext& Context, FIntPoint Tile, FDwAiWeights& OutTerms)
{
	using namespace DwAiScoring;
	const FDwCombatSnapshot& Snapshot = *Context.Snapshot;
	const FDwGridModel& Grid = Snapshot.Grid;
	const FDwTile* Data = Grid.GetTile(Tile);
	const EDwSide Opponents = Context.GetOpponentSide();
	const bool bOnOpponentDeck = Data && Data->Owner == Opponents;
	const uint8 Elevation = Data ? Data->Elevation : 0;

	OutTerms.SinkingRisk = bOnOpponentDeck && Context.bOpponentShipCanSink ? 1.f : 0.f;
	OutTerms.Boarding = Context.bBoarders && bOnOpponentDeck ? 1.f : 0.f;
	OutTerms.CannonAffinity = Context.bPreferCannons && FDwCannonRules::FindUsableCannon(Grid, Tile, Context.Side, Snapshot.CannonsUsed).IsSet() ? 1.f : 0.f;
	OutTerms.HighGround = Context.Reach.ContainsByPredicate([](const FDwAiReach& R) { return R.bRanged; }) ? static_cast<float>(Elevation) : 0.f;

	// Approach: tiles short of hitting the nearest opponent this unit's attacks can reach at all (melee can't reach the nest).
	TOptional<int32> Shortfall;
	TOptional<int32> Nearest;
	for (const FDwUnitSnapshot& Opponent : Snapshot.Units)
	{
		if (Opponent.Side != Opponents || !Opponent.bAlive)
		{
			continue;
		}
		const int32 Distance = FDwLineOfSight::GetDistance(Snapshot.GridRules, Tile, Opponent.Tile);
		Nearest = FMath::Min(Nearest.Get(MAX_int32), Distance);
		const bool bOpponentElevated = GetElevation(Grid, Opponent.Tile) > 0;
		for (const FDwAiReach& Reach : Context.Reach)
		{
			if (!Reach.bCanTargetElevated && (bOpponentElevated || Elevation > 0))
			{
				continue;
			}
			const int32 Range = Reach.Range + Elevation * Snapshot.GridRules.RangeBonusPerElevation;
			Shortfall = FMath::Min(Shortfall.Get(MAX_int32), FMath::Max(0, Distance - Range));
		}
	}
	// No attack can reach anyone from here: at least close the distance.
	OutTerms.Approach = static_cast<float>(Shortfall.IsSet() ? Shortfall.GetValue() : (Nearest.IsSet() ? FMath::Max(0, Nearest.GetValue() - 1) : 0));
}

float FDwAiScoring::DangerAt(const FDwAiScoringContext& Context, FIntPoint Tile)
{
	return DwAiScoring::DangerAt(Context, Tile, {});
}

void FDwAiScoring::ScoreAction(const FDwAiScoringContext& Context, FIntPoint Tile, const UDwAbilityDefinition& Ability,
	const FDwAbilityPreview& Preview, FDwAiWeights& OutTerms, TArray<FDwUnitHandle>* OutKilled)
{
	using namespace DwAiScoring;
	const FDwCombatSnapshot& Snapshot = *Context.Snapshot;
	const UDwGameplayAbility* CDO = FDwCombatQueries::GetAbilityCDO(&Ability);
	const FDwAbilityTuningRow* Tuning = UDwGameplayAbility::GetTuning(&Ability);
	if (!CDO || !Preview.bValid)
	{
		return;
	}
	OutTerms.ManaCost = Tuning ? static_cast<float>(Tuning->ManaCost) : 0.f;

	// Random patterns preview every possible target once with per-hit numbers: weight each by its expected share of hits.
	const bool bRandom = CDO->GetTargetingRules(MakeTargeting(Snapshot, Context.Unit, &Ability)).Pattern == EDwTargetPattern::RandomEnemies;
	const float HitShare = bRandom ? 0.5f * (Preview.MinHits + Preview.MaxHits) / FMath::Max(1, Preview.Targets.Num()) : 1.f;

	bool bDecisive = false;
	for (const FDwTargetPreview& Entry : Preview.Targets)
	{
		const float Raw = Entry.Damage * HitShare;
		if (Entry.Target.bShip)
		{
			const FDwShipSnapshot* Ship = Snapshot.FindShip(Entry.Target.ShipSide);
			if (!Ship || Ship->MaxHealth <= 0.f)
			{
				continue;
			}
			const float Dealt = FMath::Min(Raw, Ship->Health);
			if (Entry.Target.ShipSide == Context.Side)
			{
				OutTerms.FriendlyFire += Dealt;
			}
			else
			{
				OutTerms.ShipDamage += Dealt;
				OutTerms.ShipLowHull += Dealt * (1.f - Ship->Health / Ship->MaxHealth);
			}
			bDecisive |= Raw > 0.f && Raw >= Ship->Health;
			continue;
		}
		const FDwUnitSnapshot* Victim = Snapshot.FindUnit(Entry.Target.Unit);
		if (!Victim || !Victim->bAlive || Victim->MaxHealth <= 0.f)
		{
			continue;
		}
		const float Dealt = FMath::Min(Raw, Victim->Health);
		if (Victim->Side == Context.Side)
		{
			OutTerms.FriendlyFire += Dealt;
			OutTerms.Heal += FMath::Min(Entry.Heal * HitShare, Victim->MaxHealth - Victim->Health);
			continue;
		}
		OutTerms.Damage += Dealt;
		if (Raw > 0.f && Raw >= Victim->Health)
		{
			OutTerms.Kill += 1.f;
			bDecisive = true;
			if (OutKilled)
			{
				OutKilled->AddUnique(Victim->Handle);
			}
		}
		if (Dealt > 0.f)
		{
			OutTerms.LowHealthFocus += 1.f - Victim->Health / Victim->MaxHealth;
			if (Victim->Tags.HasTagExact(DwTags::Unit_Role_Captain))
			{
				OutTerms.CaptainPressure += Dealt;
			}
		}
		if (Entry.Fear > 0 && !Victim->Tags.HasTagExact(DwTags::Unit_Immune_Fear))
		{
			OutTerms.Fear += Entry.Fear * HitShare;
			bDecisive |= Snapshot.OutcomeConfig.bSurrenderEnabled;
		}
	}
	if (!bDecisive)
	{
		return;
	}

	// Would this end the fight? Apply the preview to a scratch copy and ask the real outcome rules (C10, C12).
	if (!Context.OutcomeScratch.IsSet())
	{
		FDwCombatSnapshot& Scratch = Context.OutcomeScratch.Emplace();
		Scratch.Units = Snapshot.Units;
		Scratch.Ships = Snapshot.Ships;
		Scratch.OutcomeConfig = Snapshot.OutcomeConfig;
	}
	FDwCombatSnapshot& Scratch = Context.OutcomeScratch.GetValue();
	auto FindScratchUnit = [&Scratch](FDwUnitHandle Handle) { return Scratch.Units.FindByPredicate([Handle](const FDwUnitSnapshot& U) { return U.Handle == Handle; }); };
	if (FDwUnitSnapshot* Self = FindScratchUnit(Context.Unit))
	{
		Self->Tile = Tile;
	}
	for (const FDwTargetPreview& Entry : Preview.Targets)
	{
		const float Raw = Entry.Damage * HitShare;
		if (Entry.Target.bShip)
		{
			FDwShipSnapshot* Ship = Scratch.Ships.FindByPredicate([&Entry](const FDwShipSnapshot& S) { return S.Side == Entry.Target.ShipSide; });
			if (Ship && (Ship->Health -= Raw) <= 0.f)
			{
				// Everyone standing on a sunk ship goes down with it, boarders included.
				for (FDwUnitSnapshot& Unit : Scratch.Units)
				{
					const FDwTile* On = Snapshot.Grid.GetTile(Unit.Tile);
					Unit.bAlive &= !(On && On->Owner == Ship->Side);
				}
			}
		}
		else if (FDwUnitSnapshot* Victim = FindScratchUnit(Entry.Target.Unit))
		{
			Victim->Health -= Raw;
			Victim->bAlive &= Victim->Health > 0.f;
			Victim->Fear += Victim->Side != Context.Side ? Entry.Fear * HitShare : 0.f;
		}
	}
	const EDwCombatOutcome Outcome = FDwCombatOutcomeRules::Evaluate(Scratch, Scratch.OutcomeConfig);
	const bool bWeWin = Context.Side == EDwSide::Player ? DwCombat::IsVictory(Outcome) : DwCombat::IsDefeat(Outcome);
	const bool bWeLose = Context.Side == EDwSide::Player ? DwCombat::IsDefeat(Outcome) : DwCombat::IsVictory(Outcome);
	OutTerms.Win = bWeWin ? 1.f : (bWeLose ? -1.f : 0.f);

	// Restore the scratch copy for the next candidate.
	for (int32 Index = 0; Index < Scratch.Units.Num(); ++Index)
	{
		const FDwUnitSnapshot& Original = Snapshot.Units[Index];
		FDwUnitSnapshot& Unit = Scratch.Units[Index];
		Unit.Tile = Original.Tile;
		Unit.Health = Original.Health;
		Unit.Fear = Original.Fear;
		Unit.bAlive = Original.bAlive;
	}
	for (int32 Index = 0; Index < Scratch.Ships.Num(); ++Index)
	{
		Scratch.Ships[Index].Health = Snapshot.Ships[Index].Health;
	}
}

float FDwAiScoring::ScoreCandidate(const FDwAiScoringContext& Context, FDwAiCandidate& Candidate, const UDwAbilityDefinition* Ability,
	const FDwAiWeights& Weights, const float* CachedDanger)
{
	Candidate.Terms = FDwAiWeights();
	ScorePosition(Context, Candidate.MoveTile, Candidate.Terms);
	TArray<FDwUnitHandle> Killed;
	if (Candidate.HasAction() && Ability)
	{
		ScoreAction(Context, Candidate.MoveTile, *Ability, Candidate.Preview, Candidate.Terms, &Killed);
	}
	// Opponents this action kills won't threaten the destination next turn.
	Candidate.Terms.Danger = CachedDanger && Killed.IsEmpty() ? *CachedDanger : DwAiScoring::DangerAt(Context, Candidate.MoveTile, Killed);
	Candidate.Score = FDwAiWeights::Dot(Weights, Candidate.Terms);
	return Candidate.Score;
}
