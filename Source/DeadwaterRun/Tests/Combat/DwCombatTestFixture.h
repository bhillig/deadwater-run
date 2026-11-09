// Copyright Brandon Hillig 2026

#pragma once

// A fight in a transient world for DeadwaterRun.Combat.* specs: inline ASCII decks, transient tuning tables and
// test abilities. Private to the combat tests until FDwTestWorld (DW-108, tools-engineer) absorbs it.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Abilities/DwAbilitySystemComponent.h"
#include "Abilities/DwAttributeSets.h"
#include "Board/DwBoardBuilder.h"
#include "Combat/DwCombatSubsystem.h"
#include "Combat/DwCombatUnit.h"
#include "Combat/DwShip.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Abilities/DwGA_FireCannon.h"
#include "Abilities/DwGA_Move.h"
#include "Abilities/DwGA_UseItem.h"
#include "Abilities/DwGameplayEffects.h"
#include "Presentation/DwPresentationSubsystem.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Tests/DwTestAbilities.h"

struct FDwCombatFixture
{
	UWorld* World = nullptr;
	UDwCombatSubsystem* Combat = nullptr;
	UDwTestCombatListener* Listener = nullptr;
	UDataTable* UnitTable = nullptr;
	UDataTable* AbilityTable = nullptr;

	/** Ability.Attack.Melee: 30 flat damage, range 20 (reaches across both decks), 1 action. */
	UDwAbilityDefinition* Strike = nullptr;
	/** Ability.Attack.Ranged: 150 flat damage, range 20 - kills in one hit. */
	UDwAbilityDefinition* Execute = nullptr;
	/** Ability.Cannon: 300 to the enemy ship - sinks it in one hit. */
	UDwAbilityDefinition* Broadside = nullptr;
	/** Ability.Captain.RallyCry slot: stays active until cancelled. */
	UDwAbilityDefinition* Latent = nullptr;
	UDwUnitDefinition* Grunt = nullptr;

	// Real DW-104 actions (UseRealActions)
	UDwAbilityDefinition* Melee = nullptr;     // Ability.Attack.Melee payload: 25 damage, range 1
	UDwAbilityDefinition* Volley = nullptr;    // Ability.Attack.Ranged: 3-5 random hits of 10, range 20
	UDwAbilityDefinition* Terror = nullptr;    // Ability.Captain.SmokeAndFire: 15 to the afraid, then +1 Fear to all
	UDwAbilityDefinition* Move = nullptr;
	UDwAbilityDefinition* Cannon = nullptr;
	UDwAbilityDefinition* Item = nullptr;
	UDwItemDefinition* Potion = nullptr;       // heals 30, self or adjacent ally
	UDwItemDefinition* Tonic = nullptr;        // restores 10 MP, self or adjacent ally
	UDwUnitDefinition* Sailor = nullptr;
	UDataTable* ItemTable = nullptr;

	FDwEncounterSetup Setup;
	TArray<UObject*> Rooted;

	/**
	 * Default board, 10 x 3: player deck columns 0-4, enemy deck 5-9 (mirrored). Spawns in column 0 and 9,
	 * cannons at (2,1) and (7,1), boarding edges in columns 4 and 5.
	 */
	static TArray<FString> DefaultDeck()
	{
		return { TEXT("S...E"), TEXT("S.C.E"), TEXT("S...E") };
	}

	void Create()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL()); // or ASCs never register their attribute sets
		Combat = World->GetSubsystem<UDwCombatSubsystem>();
		World->GetSubsystem<UDwPresentationSubsystem>()->SetInstant(true); // tests resolve in one call stack unless a test opts out

		Listener = Root(NewObject<UDwTestCombatListener>());
		Listener->Bind(*Combat);

		AbilityTable = Root(NewObject<UDataTable>(GetTransientPackage()));
		AbilityTable->RowStruct = FDwAbilityTuningRow::StaticStruct();
		UnitTable = Root(NewObject<UDataTable>(GetTransientPackage()));
		UnitTable->RowStruct = FDwUnitTuningRow::StaticStruct();

		FDwAbilityTuningRow Row;
		Row.Range = 20;
		Row.Damage = 30;  AbilityTable->AddRow(TEXT("Strike"), Row);
		Row.Damage = 150; AbilityTable->AddRow(TEXT("Execute"), Row);
		Row.Damage = 300; AbilityTable->AddRow(TEXT("Broadside"), Row);
		Row.Damage = 0;   AbilityTable->AddRow(TEXT("Latent"), Row);

		Strike = MakeAbility(TEXT("Strike"), UDwTestAbility_Strike::StaticClass(), DwTags::Ability_Attack_Melee);
		Execute = MakeAbility(TEXT("Execute"), UDwTestAbility_Strike::StaticClass(), DwTags::Ability_Attack_Ranged);
		Broadside = MakeAbility(TEXT("Broadside"), UDwTestAbility_Broadside::StaticClass(), DwTags::Ability_Cannon);
		Latent = MakeAbility(TEXT("Latent"), UDwTestAbility_Latent::StaticClass(), DwTags::Ability_Captain_RallyCry);

		FDwUnitTuningRow Unit;
		Unit.MaxHealth = 100; Unit.MaxMana = 20; Unit.AttackDamage = 10; Unit.MoveRange = 5; Unit.MaxActionPoints = 1;
		Unit.HealthPerLevel = 10;
		UnitTable->AddRow(TEXT("Grunt"), Unit);
		Grunt = Root(NewObject<UDwUnitDefinition>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UDwUnitDefinition::StaticClass(), TEXT("DA_Unit_TestGrunt"))));
		Grunt->BasicAttack = Strike;
		Grunt->DefaultAbilities = { Execute, Broadside, Latent };
		Grunt->Tuning.DataTable = UnitTable;
		Grunt->Tuning.RowName = TEXT("Grunt");

		Setup = FDwEncounterSetup();
		SetDecks(DefaultDeck(), DefaultDeck());
		for (int32 Index = 0; Index < 3; ++Index)
		{
			FDwCombatantSetup Crew;
			Crew.Definition = Grunt;
			Crew.RosterId = FGuid(1, 0, 0, Index + 1);
			Crew.bIsCaptain = Index == 0;
			Setup.Crew.Add(Crew);

			FDwCombatantSetup Enemy;
			Enemy.Definition = Grunt;
			Setup.Enemies.Add(Enemy);
		}
		Setup.Seed = 7;
	}

	/** Crew and enemies become Sailors: melee + volley + Move, FireCannon, UseItem; the crew carries 2 potions and 1 tonic. */
	void UseRealActions()
	{
		FDwAbilityTuningRow Row;
		Row.Damage = 25; Row.Range = 1; AbilityTable->AddRow(TEXT("Melee"), Row);
		Row = FDwAbilityTuningRow(); Row.Damage = 10; Row.Range = 20; Row.MinHits = 3; Row.MaxHits = 5; AbilityTable->AddRow(TEXT("Volley"), Row);
		Row = FDwAbilityTuningRow(); Row.Damage = 15; Row.Fear = 1; Row.Range = 20; AbilityTable->AddRow(TEXT("Terror"), Row);
		Row = FDwAbilityTuningRow(); AbilityTable->AddRow(TEXT("Move"), Row);
		AbilityTable->AddRow(TEXT("Cannon"), Row);
		AbilityTable->AddRow(TEXT("Item"), Row);

		Melee = MakeAbility(TEXT("Melee"), UDwTestPayload_Melee::StaticClass(), DwTags::Ability_Attack_Melee);
		Volley = MakeAbility(TEXT("Volley"), UDwTestPayload_Volley::StaticClass(), DwTags::Ability_Attack_Ranged);
		Terror = MakeAbility(TEXT("Terror"), UDwTestPayload_Terror::StaticClass(), DwTags::Ability_Captain_SmokeAndFire);
		Move = MakeAbility(TEXT("Move"), UDwGA_Move::StaticClass(), DwTags::Ability_Move);
		Cannon = MakeAbility(TEXT("Cannon"), UDwGA_FireCannon::StaticClass(), DwTags::Ability_Cannon);
		Item = MakeAbility(TEXT("Item"), UDwGA_UseItem::StaticClass(), DwTags::Ability_Item);

		ItemTable = Root(NewObject<UDataTable>(GetTransientPackage()));
		ItemTable->RowStruct = FDwItemTuningRow::StaticStruct();
		FDwItemTuningRow ItemRow;
		ItemRow.Range = 1;
		ItemRow.Amount = 30; ItemTable->AddRow(TEXT("Potion"), ItemRow);
		ItemRow.Amount = 10; ItemTable->AddRow(TEXT("Tonic"), ItemRow);
		Potion = MakeItem(TEXT("Potion"), UDwGE_Heal::StaticClass());
		Tonic = MakeItem(TEXT("Tonic"), UDwGE_RestoreMana::StaticClass());

		Sailor = Root(NewObject<UDwUnitDefinition>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UDwUnitDefinition::StaticClass(), TEXT("DA_Unit_TestSailor"))));
		Sailor->BasicAttack = Melee;
		Sailor->DefaultAbilities = { Volley };
		Sailor->Tuning = Grunt->Tuning;
		for (FDwCombatantSetup& Crew : Setup.Crew) { Crew.Definition = Sailor; }
		for (FDwCombatantSetup& Enemy : Setup.Enemies) { Enemy.Definition = Sailor; }
		Setup.CommonAbilities = { Move, Cannon, Item };
		Setup.Items.Reset();
		FDwItemStack& Potions = Setup.Items.AddDefaulted_GetRef(); Potions.Item = Potion; Potions.Count = 2;
		FDwItemStack& Tonics = Setup.Items.AddDefaulted_GetRef(); Tonics.Item = Tonic; Tonics.Count = 1;
	}

	EDwCommandResult MoveTo(FDwUnitHandle Unit, FIntPoint Tile)
	{
		return Combat->SubmitCommand(FDwCombatCommand::MakeMove(Unit, Tile));
	}

	EDwCommandResult UseItem(FDwUnitHandle Source, UDwItemDefinition* ItemDef, FDwUnitHandle Target)
	{
		return Combat->SubmitCommand(FDwCombatCommand::MakeAbility(Source, DwTags::Ability_Item,
			FDwAbilityTarget::MakeUnit(Target, Unit(Target)->GetTile()), ItemDef->GetFName()));
	}

	FDwTargetingContext Context(const FDwCombatSnapshot& Snapshot, FDwUnitHandle Source, const UDwAbilityDefinition* Ability, FName ItemId = NAME_None) const
	{
		FDwTargetingContext C;
		C.Snapshot = &Snapshot;
		C.Ability = Ability;
		C.Source = Source;
		C.ItemId = ItemId;
		return C;
	}

	void Destroy()
	{
		Combat->EndEncounter();
		for (UObject* Object : Rooted)
		{
			Object->RemoveFromRoot();
		}
		Rooted.Reset();
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		World = nullptr;
		Combat = nullptr;
	}

	void SetDecks(const TArray<FString>& PlayerRows, const TArray<FString>& EnemyRows)
	{
		Setup.Grid = FDwBoardBuilder::BuildShipBattle(FDwDeckAscii::Parse(PlayerRows), FDwDeckAscii::Parse(EnemyRows));
	}

	bool Start() { return Combat->StartEncounter(Setup); }

	/** Handles: crew 1..3 (1 = captain), enemies 4..6. */
	static FDwUnitHandle CrewHandle(int32 Index) { return FDwUnitHandle(Index + 1); }
	FDwUnitHandle EnemyHandle(int32 Index) const { return FDwUnitHandle(Setup.Crew.Num() + Index + 1); }

	ADwCombatUnit* Unit(FDwUnitHandle Handle) const { return Combat->FindUnit(Handle); }
	UDwAbilitySystemComponent* Asc(FDwUnitHandle Handle) const { return Unit(Handle)->GetDwAbilitySystem(); }
	float Attr(FDwUnitHandle Handle, const FGameplayAttribute& Attribute) const { return Asc(Handle)->GetNumericAttribute(Attribute); }
	float Health(FDwUnitHandle Handle) const { return Attr(Handle, UDwUnitAttributeSet::GetHealthAttribute()); }
	float ShipHealth(EDwSide Side) const { return Combat->GetShip(Side)->GetHealth(); }

	EDwCommandResult Use(FDwUnitHandle Source, UDwAbilityDefinition* Ability, FDwUnitHandle Target)
	{
		const ADwCombatUnit* TargetUnit = Unit(Target);
		return Combat->SubmitCommand(FDwCombatCommand::MakeAbility(Source, Ability->AbilityTag,
			FDwAbilityTarget::MakeUnit(Target, TargetUnit ? TargetUnit->GetTile() : FIntPoint::ZeroValue)));
	}

	EDwCommandResult FireAtShip(FDwUnitHandle Source, EDwSide ShipSide)
	{
		return Combat->SubmitCommand(FDwCombatCommand::MakeAbility(Source, Broadside->AbilityTag, FDwAbilityTarget::MakeShip(ShipSide)));
	}

	EDwCommandResult EndTurn(EDwSide Side = EDwSide::Player)
	{
		return Combat->SubmitCommand(FDwCombatCommand::MakeEndTurn(Side));
	}

	/** Applies a shared effect from Source (or no instigator) to Target, outside any command. */
	FActiveGameplayEffectHandle ApplyEffect(TSubclassOf<UGameplayEffect> Effect, FDwUnitHandle Target, FDwUnitHandle Source = FDwUnitHandle())
	{
		UDwAbilitySystemComponent* SourceAsc = Source.IsValid() ? Asc(Source) : Asc(Target);
		FGameplayEffectContextHandle Context = SourceAsc->MakeEffectContext();
		FGameplayEffectSpecHandle Spec = SourceAsc->MakeOutgoingSpec(Effect, 1, Context);
		return SourceAsc->ApplyGameplayEffectSpecToTarget(*Spec.Data, Asc(Target));
	}

	template <typename T>
	T* Root(T* Object)
	{
		Object->AddToRoot();
		Rooted.Add(Object);
		return Object;
	}

	UDwItemDefinition* MakeItem(const TCHAR* Name, UClass* Effect)
	{
		UDwItemDefinition* ItemDef = Root(NewObject<UDwItemDefinition>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UDwItemDefinition::StaticClass(), *FString::Printf(TEXT("DA_Item_Test%s"), Name))));
		ItemDef->Effect = Effect;
		ItemDef->Targeting.Pattern = EDwTargetPattern::SingleUnit;
		ItemDef->Targeting.Side = EDwTargetSide::Ally;
		ItemDef->Tuning.DataTable = ItemTable;
		ItemDef->Tuning.RowName = Name;
		return ItemDef;
	}

private:
	UDwAbilityDefinition* MakeAbility(const TCHAR* Name, UClass* Class, const FGameplayTag& Tag)
	{
		UDwAbilityDefinition* Ability = Root(NewObject<UDwAbilityDefinition>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UDwAbilityDefinition::StaticClass(), *FString::Printf(TEXT("DA_Ability_Test%s"), Name))));
		Ability->DisplayName = FText::FromString(Name);
		Ability->AbilityClass = Class;
		Ability->AbilityTag = Tag;
		Ability->Tuning.DataTable = AbilityTable;
		Ability->Tuning.RowName = Name;
		return Ability;
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
