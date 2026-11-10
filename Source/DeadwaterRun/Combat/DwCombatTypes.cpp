// Copyright Brandon Hillig 2026

#include "Combat/DwCombatTypes.h"

#include "Core/DwGameplayTags.h"

FDwCombatCommand FDwCombatCommand::MakeMove(FDwUnitHandle InUnit, FIntPoint Tile)
{
	FDwCombatCommand Command;
	Command.Type = EDwCommandType::Move;
	Command.Unit = InUnit;
	Command.AbilityTag = DwTags::Ability_Move;
	Command.Target = FDwAbilityTarget::MakeTile(Tile);
	return Command;
}

FDwCombatCommand FDwCombatCommand::MakeAbility(FDwUnitHandle InUnit, const FGameplayTag& InAbilityTag, const FDwAbilityTarget& InTarget, FName InItemId)
{
	FDwCombatCommand Command;
	Command.Type = EDwCommandType::Ability;
	Command.Unit = InUnit;
	Command.AbilityTag = InAbilityTag;
	Command.Target = InTarget;
	Command.ItemId = InItemId;
	return Command;
}

FDwCombatCommand FDwCombatCommand::MakeEndUnit(FDwUnitHandle InUnit)
{
	FDwCombatCommand Command;
	Command.Type = EDwCommandType::EndUnit;
	Command.Unit = InUnit;
	return Command;
}

FDwCombatCommand FDwCombatCommand::MakeEndTurn(EDwSide InSide)
{
	FDwCombatCommand Command;
	Command.Type = EDwCommandType::EndTurn;
	Command.Side = InSide;
	return Command;
}

FGameplayTag FDwCombatCommand::GetAbilityTag() const
{
	switch (Type)
	{
	case EDwCommandType::Move:    return DwTags::Ability_Move;
	case EDwCommandType::Ability: return AbilityTag;
	default:                      return FGameplayTag();
	}
}

FString FDwCombatCommand::ToString() const
{
	const FString TypeName = StaticEnum<EDwCommandType>()->GetNameStringByValue(static_cast<int64>(Type));
	if (Type == EDwCommandType::EndTurn)
	{
		return FString::Printf(TEXT("%s(%s)"), *TypeName, *StaticEnum<EDwSide>()->GetNameStringByValue(static_cast<int64>(Side)));
	}
	return FString::Printf(TEXT("%s(Unit=%d %s Tile=%d,%d TargetUnit=%d Ship=%d Item=%s)"), *TypeName, Unit.Id, *GetAbilityTag().ToString(),
		Target.Tile.X, Target.Tile.Y, Target.Unit.Id, Target.bShip ? 1 : 0, *ItemId.ToString());
}

bool FDwCombatResult::operator==(const FDwCombatResult& Other) const
{
	if (Outcome != Other.Outcome || Rounds != Other.Rounds || PlayerShipHealth != Other.PlayerShipHealth
		|| Crew.Num() != Other.Crew.Num() || Kills.Num() != Other.Kills.Num() || !ItemsUsed.OrderIndependentCompareEqual(Other.ItemsUsed))
	{
		return false;
	}
	for (int32 Index = 0; Index < Crew.Num(); ++Index)
	{
		const FDwCrewResult& A = Crew[Index];
		const FDwCrewResult& B = Other.Crew[Index];
		if (A.RosterId != B.RosterId || A.bAlive != B.bAlive || A.Health != B.Health)
		{
			return false;
		}
	}
	for (int32 Index = 0; Index < Kills.Num(); ++Index)
	{
		const FDwKillRecord& A = Kills[Index];
		const FDwKillRecord& B = Other.Kills[Index];
		if (A.KillerRosterId != B.KillerRosterId || A.Killer != B.Killer || A.Victim != B.Victim || A.VictimDefinition != B.VictimDefinition
			|| A.Xp != B.Xp)
		{
			return false;
		}
	}
	return true;
}

FString FDwCombatResult::ToString() const
{
	FString Out = FString::Printf(TEXT("%s after %d rounds, ship %.0f HP, %d kills;"),
		*StaticEnum<EDwCombatOutcome>()->GetNameStringByValue(static_cast<int64>(Outcome)), Rounds, PlayerShipHealth, Kills.Num());
	for (const FDwCrewResult& Member : Crew)
	{
		Out += FString::Printf(TEXT(" [%s %s %.0f]"), *Member.RosterId.ToString(EGuidFormats::Short), Member.bAlive ? TEXT("alive") : TEXT("dead"), Member.Health);
	}
	return Out;
}
