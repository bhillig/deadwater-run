// Copyright Brandon Hillig 2026

#pragma once

// World-free bench for DeadwaterRun.AI.*: hand-made snapshots (ASCII decks, units with real ability CDOs), the real
// default AI tuning read from Tuning/AiTuning.json, and a pure "apply this command" so whole turns can be planned
// without GAS. Private to the AI tests.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "AI/DwAiProfile.h"
#include "AI/DwCrewAiBrain.h"
#include "Abilities/DwGA_FireCannon.h"
#include "Abilities/DwGA_Move.h"
#include "Abilities/DwGA_UseItem.h"
#include "Board/DwBoardBuilder.h"
#include "Combat/DwCombatRules.h"
#include "Combat/DwCombatSnapshot.h"
#include "Core/DwGameplayTags.h"
#include "Abilities/DwGameplayEffects.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Misc/ScopeExit.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tests/AI/DwAiTestAbilities.h"
#include "Tests/DwTestAbilities.h"

struct FDwAiTestBench
{
	UDataTable* AbilityTable = nullptr;
	UDataTable* AiTable = nullptr;

	/** Ability.Attack.Melee: 25, range 1, can't reach the nest (content's cutlass). */
	UDwAbilityDefinition* Sword = nullptr;
	/** Ability.Attack.Ranged: 20, range 6, line of sight (content's musket). */
	UDwAbilityDefinition* Rifle = nullptr;
	/** Ability.Captain.RallyCry slot: heals one ally for 30, range 3. */
	UDwAbilityDefinition* Heal = nullptr;
	UDwAbilityDefinition* Move = nullptr;
	UDwAbilityDefinition* Cannon = nullptr;
	UDwAbilityDefinition* Item = nullptr;

	UDwAiProfile* Profile = nullptr;
	UDwCrewAiBrain* Brain = nullptr;
	FDwCombatSnapshot Snapshot;
	TArray<UObject*> Rooted;

	void Create()
	{
		AbilityTable = Root(NewObject<UDataTable>(GetTransientPackage()));
		AbilityTable->RowStruct = FDwAbilityTuningRow::StaticStruct();
		FDwAbilityTuningRow Row;
		Row.Damage = 25; Row.Range = 1; AbilityTable->AddRow(TEXT("Sword"), Row);
		Row = FDwAbilityTuningRow(); Row.Damage = 20; Row.Range = 6; AbilityTable->AddRow(TEXT("Rifle"), Row);
		Row = FDwAbilityTuningRow(); Row.Heal = 30; Row.Range = 3; AbilityTable->AddRow(TEXT("Heal"), Row);
		Row = FDwAbilityTuningRow();
		AbilityTable->AddRow(TEXT("Move"), Row);
		AbilityTable->AddRow(TEXT("Cannon"), Row);
		AbilityTable->AddRow(TEXT("Item"), Row);

		Sword = MakeAbility(TEXT("Sword"), UDwTestPayload_Melee::StaticClass(), DwTags::Ability_Attack_Melee);
		Rifle = MakeAbility(TEXT("Rifle"), UDwAiTestPayload_Rifle::StaticClass(), DwTags::Ability_Attack_Ranged);
		Heal = MakeAbility(TEXT("Heal"), UDwAiTestPayload_Heal::StaticClass(), DwTags::Ability_Captain_RallyCry);
		Move = MakeAbility(TEXT("Move"), UDwGA_Move::StaticClass(), DwTags::Ability_Move);
		Cannon = MakeAbility(TEXT("Cannon"), UDwGA_FireCannon::StaticClass(), DwTags::Ability_Cannon);
		Item = MakeAbility(TEXT("Item"), UDwGA_UseItem::StaticClass(), DwTags::Ability_Item);

		// The shipping default profile's numbers, from the JSON the Balance Designer edits.
		AiTable = Root(NewObject<UDataTable>(GetTransientPackage()));
		AiTable->RowStruct = FDwAiTuningRow::StaticStruct();
		FString Json;
		FFileHelper::LoadFileToString(Json, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Tuning"), TEXT("AiTuning.json")));
		AiTable->CreateTableFromJSONString(Json);
		Profile = Root(NewObject<UDwAiProfile>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UDwAiProfile::StaticClass(), TEXT("DA_AiProfile_Test"))));
		Profile->Tuning.DataTable = AiTable;
		Profile->Tuning.RowName = TEXT("Default");
		Brain = Root(NewObject<UDwCrewAiBrain>(GetTransientPackage()));
		Brain->Profile = Profile;

		Snapshot = FDwCombatSnapshot();
		Snapshot.ActiveSide = EDwSide::Enemy;
		Snapshot.Round = 1;
		Snapshot.Units.Reserve(32); // tests hold FDwUnitSnapshot& across AddUnit calls
	}

	void Destroy()
	{
		for (UObject* Object : Rooted)
		{
			Object->RemoveFromRoot();
		}
		Rooted.Reset();
	}

	/**
	 * Default test deck, 6 x 5, facing right. On the 12 x 5 board: player deck x 0-5 (cannon (2,2), edge x 5),
	 * enemy deck x 6-11 (edge x 6, cannon (9,2)).
	 */
	static TArray<FString> Deck()
	{
		return { TEXT("S....E"), TEXT("S....E"), TEXT("S.C..E"), TEXT("S....E"), TEXT("S....E") };
	}

	/** Preview of Ability aimed at Target from Tile (the unit's own tile if unset). */
	FDwAbilityPreview Preview(FDwUnitHandle Source, const UDwAbilityDefinition* Ability, const FDwAbilityTarget& Target, TOptional<FIntPoint> Tile = {}, FName ItemId = NAME_None) const
	{
		FDwTargetingContext Targeting;
		Targeting.Snapshot = &Snapshot;
		Targeting.Ability = Ability;
		Targeting.Source = Source;
		Targeting.SourceTileOverride = Tile;
		Targeting.ItemId = ItemId;
		return FDwCombatQueries::GetAbilityCDO(Ability)->PreviewOutcome(Targeting, Target);
	}

	FDwAbilityTarget At(FDwUnitHandle Handle) { return FDwAbilityTarget::MakeUnit(Handle, Unit(Handle).Tile); }

	bool HasDefaultTuning() const { return AiTable && AiTable->FindRow<FDwAiTuningRow>(TEXT("Default"), TEXT("AI test"), false) != nullptr; }
	const FDwAiTuningRow& DefaultTuning() const { return *AiTable->FindRow<FDwAiTuningRow>(TEXT("Default"), TEXT("AI test")); }

	/** Player deck on the left, enemy deck mirrored on the right (FDwBoardBuilder). Both ships: 250 hull, 70 per cannon. */
	void SetBoard(const TArray<FString>& PlayerRows, const TArray<FString>& EnemyRows)
	{
		Snapshot.Grid = FDwBoardBuilder::BuildShipBattle(FDwDeckAscii::Parse(PlayerRows), FDwDeckAscii::Parse(EnemyRows));
		Snapshot.Grid.Occupants.Init(FDwOccupant(), Snapshot.Grid.Tiles.Num());
		Snapshot.Ships.Reset();
		for (const EDwSide Side : { EDwSide::Player, EDwSide::Enemy })
		{
			FDwShipSnapshot& Ship = Snapshot.Ships.AddDefaulted_GetRef();
			Ship.Side = Side;
			Ship.Health = Ship.MaxHealth = 250.f;
			Ship.CannonDamage = 70.f;
			for (int32 Index = 0; Index < Snapshot.Grid.Tiles.Num(); ++Index)
			{
				const FDwTile& Tile = Snapshot.Grid.Tiles[Index];
				if (Tile.Owner == Side && Tile.Tags.HasTagExact(DwTags::Tile_Cannon))
				{
					Ship.CannonTiles.Add(Snapshot.Grid.ToTile(Index));
				}
			}
		}
	}

	FDwShipSnapshot& Ship(EDwSide Side) { return *Snapshot.Ships.FindByPredicate([Side](const FDwShipSnapshot& S) { return S.Side == Side; }); }

	/** A 100 HP pirate with 5 move, 1 action, 20 MP, its attack plus Move, Cannon and Item. Handles count from 1. */
	FDwUnitSnapshot& AddUnit(EDwSide Side, FIntPoint Tile, UDwAbilityDefinition* Attack, float Health = 100.f)
	{
		FDwUnitSnapshot& U = Snapshot.Units.AddDefaulted_GetRef();
		U.Handle = FDwUnitHandle(Snapshot.Units.Num());
		U.Side = Side;
		U.Tile = Tile;
		U.Health = Health;
		U.MaxHealth = 100.f;
		U.Mana = U.MaxMana = 20.f;
		U.MaxFear = 3.f;
		U.ActionPoints = 1.f;
		U.MovePoints = U.MoveRange = 5.f;
		U.Abilities.Reset();
		if (Attack)
		{
			U.Abilities.Add(Attack);
		}
		U.Abilities.Append({ Move, Cannon, Item });
		for (const UDwAbilityDefinition* Ability : U.Abilities)
		{
			U.GrantedAbilityTags.AddTag(Ability->AbilityTag);
		}
		Snapshot.Grid.SetOccupant(Tile, U.Handle, Side);
		return U;
	}

	FDwUnitSnapshot& Unit(FDwUnitHandle Handle) { return *Snapshot.Units.FindByPredicate([Handle](const FDwUnitSnapshot& U) { return U.Handle == Handle; }); }

	/** It is Handle's turn (enemy side: the fixed sequence's acting unit). */
	void SetActing(FDwUnitHandle Handle)
	{
		Snapshot.ActiveSide = Unit(Handle).Side;
		Snapshot.ActingUnit = Snapshot.ActiveSide == EDwSide::Enemy ? Handle : FDwUnitHandle();
	}

	FDwCombatCommand Decide(FDwUnitHandle Handle, int32 Seed = 1)
	{
		SetActing(Handle);
		FDwAiTurnContext Context;
		FRandomStream Rng(Seed);
		return Brain->ChooseNextCommand(Snapshot, Handle, Context, Rng);
	}

	/**
	 * Plays Handle's whole turn on the snapshot: every command the brain chooses, applied with Apply, until EndUnit.
	 * Returns the commands in order (EndUnit last). Every command must pass ValidateCommand or the list ends early.
	 */
	TArray<FDwCombatCommand> PlayTurn(FDwUnitHandle Handle, FRandomStream& Rng, FString* OutRejected = nullptr)
	{
		SetActing(Handle);
		FDwAiTurnContext Context;
		TArray<FDwCombatCommand> Commands;
		for (int32 Step = 0; Step < 8; ++Step)
		{
			const FDwCombatCommand Command = Brain->ChooseNextCommand(Snapshot, Handle, Context, Rng);
			Commands.Add(Command);
			if (Command.Type == EDwCommandType::EndUnit)
			{
				break;
			}
			FText Reason;
			if (FDwCombatQueries::ValidateCommand(Snapshot, Command, &Reason) != EDwCommandResult::Accepted)
			{
				if (OutRejected) { *OutRejected = Command.ToString() + TEXT(": ") + Reason.ToString(); }
				break;
			}
			Apply(Command);
		}
		return Commands;
	}

	/** What the command does, approximated without GAS: moves, budgets, MP, the preview's damage and heals, cannons used. */
	void Apply(const FDwCombatCommand& Command)
	{
		FDwUnitSnapshot& Self = Unit(Command.Unit);
		if (Command.Type == EDwCommandType::Move)
		{
			Snapshot.Grid.ClearOccupant(Self.Tile);
			Self.Tile = Command.Target.Tile;
			Snapshot.Grid.SetOccupant(Self.Tile, Self.Handle, Self.Side);
			Self.MovePoints = 0.f;
			ApplyBudgetRules(Self);
			return;
		}
		if (Command.Type != EDwCommandType::Ability)
		{
			return;
		}
		ON_SCOPE_EXIT { ApplyBudgetRules(Unit(Command.Unit)); };
		const UDwAbilityDefinition* Ability = FDwCombatQueries::FindAbility(Self, Command.AbilityTag);
		const UDwGameplayAbility* CDO = FDwCombatQueries::GetAbilityCDO(Ability);
		FDwTargetingContext Targeting;
		Targeting.Snapshot = &Snapshot;
		Targeting.Ability = Ability;
		Targeting.Source = Self.Handle;
		Targeting.ItemId = Command.ItemId;
		const FDwAbilityPreview Preview = CDO->PreviewOutcome(Targeting, Command.Target);
		if (Command.AbilityTag == DwTags::Ability_Cannon)
		{
			if (const TOptional<FIntPoint> Used = FDwCannonRules::FindUsableCannon(Snapshot.Grid, Self.Tile, Self.Side, Snapshot.CannonsUsed))
			{
				Snapshot.CannonsUsed.Add(Used.GetValue());
			}
		}
		for (const FDwTargetPreview& Hit : Preview.Targets)
		{
			if (Hit.Target.bShip)
			{
				Ship(Hit.Target.ShipSide).Health -= Hit.Damage;
			}
			else if (Hit.Target.IsUnit())
			{
				FDwUnitSnapshot& Victim = Unit(Hit.Target.Unit);
				Victim.Health = FMath::Clamp(Victim.Health - Hit.Damage + Hit.Heal, 0.f, Victim.MaxHealth);
				if (Victim.Health <= 0.f && Victim.bAlive)
				{
					Victim.bAlive = false;
					Victim.Tags.AddTag(DwTags::State_Dead);
					Snapshot.Grid.ClearOccupant(Victim.Tile);
				}
			}
		}
		if (CDO->ActionCost == EDwActionCost::Action)
		{
			Self.ActionPoints -= 1.f;
		}
	}

	/** Rule.Turn.SingleChoice: paying either budget spends both (UDwGameplayAbility::ApplyCost). */
	void ApplyBudgetRules(FDwUnitSnapshot& Self)
	{
		if (Self.Tags.HasTag(DwTags::Rule_Turn_SingleChoice) && (Self.ActionPoints < 1.f || Self.MovePoints <= 0.f))
		{
			Self.ActionPoints = 0.f;
			Self.MovePoints = 0.f;
		}
	}

	/** A run-inventory item for player-side tests: heals 30, self or an adjacent ally. */
	UDwItemDefinition* MakePotion()
	{
		UDataTable* ItemTable = Root(NewObject<UDataTable>(GetTransientPackage()));
		ItemTable->RowStruct = FDwItemTuningRow::StaticStruct();
		FDwItemTuningRow Row;
		Row.Amount = 30;
		Row.Range = 1;
		ItemTable->AddRow(TEXT("Potion"), Row);
		UDwItemDefinition* Potion = Root(NewObject<UDwItemDefinition>(GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UDwItemDefinition::StaticClass(), TEXT("DA_Item_AiTestPotion"))));
		Potion->Effect = UDwGE_Heal::StaticClass();
		Potion->Targeting.Pattern = EDwTargetPattern::SingleUnit;
		Potion->Targeting.Side = EDwTargetSide::Ally;
		Potion->Tuning.DataTable = ItemTable;
		Potion->Tuning.RowName = TEXT("Potion");
		return Potion;
	}

	template <typename T>
	T* Root(T* Object)
	{
		Object->AddToRoot();
		Rooted.Add(Object);
		return Object;
	}

private:
	UDwAbilityDefinition* MakeAbility(const TCHAR* Name, UClass* Class, const FGameplayTag& Tag)
	{
		UDwAbilityDefinition* Ability = Root(NewObject<UDwAbilityDefinition>(GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UDwAbilityDefinition::StaticClass(), *FString::Printf(TEXT("DA_Ability_AiTest%s"), Name))));
		Ability->DisplayName = FText::FromString(Name);
		Ability->AbilityClass = Class;
		Ability->AbilityTag = Tag;
		Ability->Tuning.DataTable = AbilityTable;
		Ability->Tuning.RowName = Name;
		return Ability;
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
