// Copyright Brandon Hillig 2026

#include "Combat/DwCombatRules.h"

#include "Abilities/DwGameplayAbility.h"
#include "Combat/DwCombatSnapshot.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwAbilityDefinition.h"

#define LOCTEXT_NAMESPACE "DwCombatRules"

// ---------------------------------------------------------------------------------------------------------------------
// Outcome

EDwCombatOutcome FDwCombatOutcomeRules::Evaluate(const FDwCombatSnapshot& Snapshot, const FDwOutcomeConfig& Config)
{
	bool bAnyPlayerAlive = false;
	bool bCaptainDead = false;
	bool bAnyEnemyAlive = false;
	bool bBossDead = false;
	for (const FDwUnitSnapshot& Unit : Snapshot.Units)
	{
		if (Unit.Side == EDwSide::Player)
		{
			bAnyPlayerAlive |= Unit.bAlive;
			bCaptainDead |= !Unit.bAlive && Unit.Tags.HasTagExact(DwTags::Unit_Role_Captain);
		}
		else if (Unit.Side == EDwSide::Enemy)
		{
			bAnyEnemyAlive |= Unit.bAlive;
			bBossDead |= !Unit.bAlive && Unit.Tags.HasTagExact(DwTags::Unit_Kraken_Body);
		}
	}
	const FDwShipSnapshot* PlayerShip = Snapshot.FindShip(EDwSide::Player);
	const FDwShipSnapshot* EnemyShip = Snapshot.FindShip(EDwSide::Enemy);

	// 1-3: defeats first, so one action that sinks the enemy ship and kills your last boarders is a defeat (C12).
	if (PlayerShip && PlayerShip->Health <= 0.f)
	{
		return EDwCombatOutcome::DefeatShipSunk;
	}
	if (!bAnyPlayerAlive)
	{
		return EDwCombatOutcome::DefeatCrewDead;
	}
	if (bCaptainDead)
	{
		return EDwCombatOutcome::DefeatCaptainDead; // C10
	}
	// 4-7: victories.
	if (Config.bIsBossFight && bBossDead)
	{
		return EDwCombatOutcome::VictoryBoss;
	}
	if (EnemyShip && EnemyShip->Health <= 0.f)
	{
		return EDwCombatOutcome::VictorySunk;
	}
	if (!bAnyEnemyAlive)
	{
		return EDwCombatOutcome::VictoryCrewDefeated;
	}
	if (Config.bSurrenderEnabled && FDwFearRules::AllEnemiesSurrender(Snapshot))
	{
		return EDwCombatOutcome::VictorySurrender;
	}
	return EDwCombatOutcome::None;
}

// ---------------------------------------------------------------------------------------------------------------------
// Fear

bool FDwFearRules::ShouldCower(float Fear, float MaxFear, bool bImmune)
{
	return !bImmune && MaxFear > 0.f && Fear >= MaxFear;
}

float FDwFearRules::FearAfterCower(float Fear, EDwFearCowerPolicy Policy)
{
	switch (Policy)
	{
	case EDwFearCowerPolicy::ResetToZero: return 0.f;
	case EDwFearCowerPolicy::StayAtMax:   return Fear;
	case EDwFearCowerPolicy::DropToOne:   return FMath::Min(Fear, 1.f);
	}
	return 0.f;
}

bool FDwFearRules::AllEnemiesSurrender(const FDwCombatSnapshot& Snapshot)
{
	bool bAnyFearCapable = false;
	for (const FDwUnitSnapshot& Unit : Snapshot.Units)
	{
		if (Unit.Side != EDwSide::Enemy || !Unit.bAlive || Unit.Tags.HasTagExact(DwTags::Unit_Immune_Fear))
		{
			continue;
		}
		bAnyFearCapable = true;
		if (Unit.MaxFear <= 0.f || Unit.Fear < Unit.MaxFear)
		{
			return false;
		}
	}
	return bAnyFearCapable;
}

// ---------------------------------------------------------------------------------------------------------------------
// Queries

const UDwGameplayAbility* FDwCombatQueries::GetAbilityCDO(const UDwAbilityDefinition* Definition)
{
	return Definition && Definition->AbilityClass ? Definition->AbilityClass->GetDefaultObject<UDwGameplayAbility>() : nullptr;
}

const UDwAbilityDefinition* FDwCombatQueries::FindAbility(const FDwUnitSnapshot& Unit, const FGameplayTag& AbilityTag)
{
	for (const UDwAbilityDefinition* Ability : Unit.Abilities)
	{
		if (Ability && Ability->AbilityTag == AbilityTag)
		{
			return Ability;
		}
	}
	return nullptr;
}

EDwCommandResult FDwCombatQueries::CanActivate(const FDwCombatSnapshot& Snapshot, const FDwUnitSnapshot& Unit, const UDwAbilityDefinition* Ability,
	FName ItemId, FText* OutReason)
{
	auto Fail = [OutReason](EDwCommandResult Result, const FText& Reason)
	{
		if (OutReason) { *OutReason = Reason; }
		return Result;
	};

	const UDwGameplayAbility* CDO = GetAbilityCDO(Ability);
	if (!CDO)
	{
		return Fail(EDwCommandResult::UnknownAbility, LOCTEXT("NoAbility", "Unknown ability"));
	}
	if (!Unit.bAlive)
	{
		return Fail(EDwCommandResult::InvalidUnit, LOCTEXT("Dead", "Dead"));
	}
	if (Unit.Tags.HasAny(CDO->GetBlockingTags()))
	{
		return Fail(EDwCommandResult::Blocked, Unit.Tags.HasTag(DwTags::Status_Rooted) && !Unit.Tags.HasTag(DwTags::Status_Incapacitated)
			? LOCTEXT("Rooted", "Rooted") : LOCTEXT("Incapacitated", "Can't act this turn"));
	}
	if (Ability->AbilityTag == DwTags::Ability_Item && !ItemId.IsNone() && Snapshot.Items.FindRef(ItemId) <= 0)
	{
		return Fail(EDwCommandResult::NoItem, LOCTEXT("NoItem", "None left"));
	}
	switch (CDO->CheckBudget(Ability, Unit.Mana, Unit.ActionPoints, Unit.MovePoints))
	{
	case EDwCostCheck::Ok:            break;
	case EDwCostCheck::NotEnoughMana: return Fail(EDwCommandResult::CannotAfford, LOCTEXT("Mana", "Not enough MP"));
	case EDwCostCheck::NoActionLeft:  return Fail(EDwCommandResult::CannotAfford, LOCTEXT("Action", "No action left this turn"));
	case EDwCostCheck::NoMoveLeft:    return Fail(EDwCommandResult::CannotAfford, LOCTEXT("Move", "Already moved this turn"));
	}
	return EDwCommandResult::Accepted;
}

EDwCommandResult FDwCombatQueries::ValidateCommand(const FDwCombatSnapshot& Snapshot, const FDwCombatCommand& Command, FText* OutReason)
{
	auto Fail = [OutReason](EDwCommandResult Result, const FText& Reason)
	{
		if (OutReason) { *OutReason = Reason; }
		return Result;
	};

	if (Snapshot.Outcome != EDwCombatOutcome::None)
	{
		return Fail(EDwCommandResult::CombatOver, LOCTEXT("Over", "The fight is over"));
	}
	if (Command.Type == EDwCommandType::EndTurn)
	{
		return Command.Side == Snapshot.ActiveSide ? EDwCommandResult::Accepted
			: Fail(EDwCommandResult::NotYourTurn, LOCTEXT("NotYourTurn", "Not your turn"));
	}

	const FDwUnitSnapshot* Unit = Snapshot.FindUnit(Command.Unit);
	if (!Unit || !Unit->bAlive)
	{
		return Fail(EDwCommandResult::InvalidUnit, LOCTEXT("InvalidUnit", "No such unit"));
	}
	if (Unit->Side != Snapshot.ActiveSide)
	{
		return Fail(EDwCommandResult::NotYourTurn, LOCTEXT("NotYourTurn", "Not your turn"));
	}
	// Enemies act one at a time in encounter order (C14).
	if (Snapshot.ActingUnit.IsValid() && Snapshot.ActingUnit != Unit->Handle)
	{
		return Fail(EDwCommandResult::NotYourTurn, LOCTEXT("NotThisUnit", "Not this unit's turn"));
	}
	if (Command.Type == EDwCommandType::EndUnit)
	{
		return EDwCommandResult::Accepted;
	}

	const UDwAbilityDefinition* Ability = FindAbility(*Unit, Command.GetAbilityTag());
	if (!Ability)
	{
		return Fail(EDwCommandResult::UnknownAbility, LOCTEXT("NoAbility", "Unknown ability"));
	}
	if (Ability->AbilityTag == DwTags::Ability_Item && Command.ItemId.IsNone())
	{
		return Fail(EDwCommandResult::NoItem, LOCTEXT("NoItemChosen", "No item chosen"));
	}
	const EDwCommandResult Activatable = CanActivate(Snapshot, *Unit, Ability, Command.ItemId, OutReason);
	if (Activatable != EDwCommandResult::Accepted)
	{
		return Activatable;
	}

	FDwTargetingContext Context;
	Context.Snapshot = &Snapshot;
	Context.Ability = Ability;
	Context.Source = Unit->Handle;
	Context.ItemId = Command.ItemId;
	if (!GetAbilityCDO(Ability)->IsValidTarget(Context, Command.Target, OutReason))
	{
		return EDwCommandResult::InvalidTarget;
	}
	return EDwCommandResult::Accepted;
}

bool FDwCombatQueries::HasAnyUse(const FDwCombatSnapshot& Snapshot, const FDwUnitSnapshot& Unit, const UDwAbilityDefinition* Ability)
{
	const UDwGameplayAbility* CDO = GetAbilityCDO(Ability);
	if (!CDO)
	{
		return false;
	}

	TArray<FName, TInlineAllocator<4>> ItemIds;
	if (Ability->AbilityTag == DwTags::Ability_Item)
	{
		for (const TPair<FName, int32>& Item : Snapshot.Items)
		{
			if (Item.Value > 0)
			{
				ItemIds.Add(Item.Key);
			}
		}
		ItemIds.Sort(FNameLexicalLess()); // deterministic
	}
	else
	{
		ItemIds.Add(NAME_None);
	}

	for (const FName ItemId : ItemIds)
	{
		if (CanActivate(Snapshot, Unit, Ability, ItemId) != EDwCommandResult::Accepted)
		{
			continue;
		}
		FDwTargetingContext Context;
		Context.Snapshot = &Snapshot;
		Context.Ability = Ability;
		Context.Source = Unit.Handle;
		Context.ItemId = ItemId;
		TArray<FDwAbilityTarget> Targets;
		CDO->GatherValidTargets(Context, Targets);
		if (!Targets.IsEmpty())
		{
			return true;
		}
	}
	return false;
}

bool FDwCombatQueries::SideHasAvailableAction(const FDwCombatSnapshot& Snapshot, EDwSide Side)
{
	for (const FDwUnitSnapshot& Unit : Snapshot.Units)
	{
		if (Unit.Side != Side || !Unit.bAlive || Unit.Tags.HasTag(DwTags::Status_Incapacitated))
		{
			continue;
		}
		for (const UDwAbilityDefinition* Ability : Unit.Abilities)
		{
			if (HasAnyUse(Snapshot, Unit, Ability))
			{
				return true;
			}
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
