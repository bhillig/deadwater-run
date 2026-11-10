// Copyright Brandon Hillig 2026

#pragma once

// Transient run content for the world-free run specs (DeadwaterRun.Rules.Run / .Economy / .Progression): a captain,
// a deckhand archetype, tuning tables, encounters, learnable abilities and perks. Nothing here touches /Game content.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwPerkDefinition.h"
#include "Data/DwPools.h"
#include "Data/DwShopDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Engine/DataTable.h"
#include "Run/DwRunTypes.h"
#include "UObject/Package.h"

namespace DwRunTest
{
	struct FContent
	{
		TArray<UObject*> Rooted;
		UDwGameData* GameData = nullptr;
		UDwCaptainDefinition* Captain = nullptr;
		UDwUnitDefinition* CaptainUnit = nullptr;
		UDwUnitDefinition* Deckhand = nullptr;
		UDwEncounterDefinition* Early = nullptr;  // layers 0-99
		UDwEncounterDefinition* Late = nullptr;   // layers 4-99
		UDwEncounterDefinition* Kraken = nullptr;
		UDataTable* Economy = nullptr;
		UDataTable* Units = nullptr;
		UDataTable* Progression = nullptr;
		UDataTable* RunTable = nullptr;
		UDataTable* Encounters = nullptr;
		UDataTable* Captains = nullptr;

		/** Learnable: Parry (both pools), Musket (firearm, both pools), Rigging (deckhand only), Lantern (captain only). */
		UDwAbilityDefinition* Cutlass = nullptr;
		UDwAbilityDefinition* Parry = nullptr;
		UDwAbilityDefinition* Musket = nullptr;
		UDwAbilityDefinition* Rigging = nullptr;
		UDwAbilityDefinition* Lantern = nullptr;
		UDwAbilityPool* HandPool = nullptr;
		UDwAbilityPool* CaptainPool = nullptr;

		/** Tough (2 stacks), Swift (1 stack), Marksman (snipers only), Lamplighter (grants Lantern). */
		UDwPerkDefinition* Tough = nullptr;
		UDwPerkDefinition* Swift = nullptr;
		UDwPerkDefinition* Marksman = nullptr;
		UDwPerkDefinition* Lamplighter = nullptr;
		UDwPerkPool* Perks = nullptr;

		UDwItemDefinition* Grog = nullptr;
		UDwItemDefinition* Tonic = nullptr;

		template <typename T>
		T* Make(const TCHAR* Name)
		{
			T* Object = NewObject<T>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), T::StaticClass(), Name));
			Object->AddToRoot();
			Rooted.Add(Object);
			return Object;
		}

		template <typename TRow>
		UDataTable* Table(const TCHAR* RowName, const TRow& Row)
		{
			UDataTable* Result = Make<UDataTable>(TEXT("DT_Test"));
			Result->RowStruct = TRow::StaticStruct();
			Result->AddRow(RowName, Row);
			return Result;
		}

		UDwAbilityDefinition* Ability(const TCHAR* Name, bool bFirearm = false)
		{
			UDwAbilityDefinition* Result = Make<UDwAbilityDefinition>(Name);
			Result->DisplayName = FText::FromString(Name);
			if (bFirearm)
			{
				Result->AbilityTags.AddTag(DwTags::Ability_Weapon_Firearm);
			}
			return Result;
		}

		UDwPerkDefinition* Perk(const TCHAR* Name, int32 MaxStacks)
		{
			UDwPerkDefinition* Result = Make<UDwPerkDefinition>(Name);
			Result->DisplayName = FText::FromString(Name);
			Result->MaxStacks = MaxStacks;
			return Result;
		}

		void SetProgression(std::initializer_list<int32> XpToNext)
		{
			Progression->EmptyTable();
			int32 Level = 1;
			for (const int32 Xp : XpToNext)
			{
				FDwProgressionTuningRow Row;
				Row.XpToNext = Xp;
				Progression->AddRow(FName(*LexToString(Level++)), Row);
			}
		}

		FDwRunTuningRow& RunRow() { return *RunTable->FindRow<FDwRunTuningRow>(TEXT("Default"), TEXT("Test")); }
		FDwEconomyTuningRow& EconomyRow() { return *Economy->FindRow<FDwEconomyTuningRow>(TEXT("Default"), TEXT("Test")); }
		FDwEncounterTuningRow& EncounterRow() { return *Encounters->FindRow<FDwEncounterTuningRow>(TEXT("Raiders"), TEXT("Test")); }
		FDwCaptainTuningRow& CaptainRow() { return *Captains->FindRow<FDwCaptainTuningRow>(TEXT("Cap"), TEXT("Test")); }

		void Create()
		{
			Units = Make<UDataTable>(TEXT("DT_TestUnits"));
			Units->RowStruct = FDwUnitTuningRow::StaticStruct();
			FDwUnitTuningRow Row;
			Row.MaxHealth = 100; Row.MaxMana = 20; Row.HealthPerLevel = 10; Row.ManaPerLevel = 2;
			Units->AddRow(TEXT("Cap"), Row);
			Row.MaxHealth = 80; Row.XpOnKill = 25;
			Units->AddRow(TEXT("Hand"), Row);

			Cutlass = Ability(TEXT("Cutlass"));
			Parry = Ability(TEXT("Parry"));
			Musket = Ability(TEXT("Musket"), /*bFirearm*/ true);
			Rigging = Ability(TEXT("Rigging"));
			Lantern = Ability(TEXT("Lantern"));
			HandPool = Make<UDwAbilityPool>(TEXT("DA_AbilityPool_TestHand"));
			HandPool->Abilities = { { Parry, 1.f }, { Musket, 1.f }, { Rigging, 1.f } };
			CaptainPool = Make<UDwAbilityPool>(TEXT("DA_AbilityPool_TestCaptain"));
			CaptainPool->Abilities = { { Parry, 1.f }, { Musket, 1.f }, { Lantern, 1.f } };

			Tough = Perk(TEXT("Tough"), 2);
			Swift = Perk(TEXT("Swift"), 1);
			Marksman = Perk(TEXT("Marksman"), 1);
			Marksman->RequiredUnitTags.AddTag(DwTags::Unit_Archetype_Sniper);
			Lamplighter = Perk(TEXT("Lamplighter"), 1);
			Lamplighter->GrantedAbility = Lantern;
			Perks = Make<UDwPerkPool>(TEXT("DA_PerkPool_Test"));
			Perks->Perks = { Tough, Swift, Marksman, Lamplighter };

			CaptainUnit = Make<UDwUnitDefinition>(TEXT("DA_Unit_TestCaptain"));
			CaptainUnit->DisplayName = FText::FromString(TEXT("Test Captain"));
			CaptainUnit->Tuning.DataTable = Units;
			CaptainUnit->Tuning.RowName = TEXT("Cap");
			CaptainUnit->BasicAttack = Cutlass;
			CaptainUnit->LearnablePool = CaptainPool;
			CaptainUnit->PerkPool = Perks;
			Deckhand = Make<UDwUnitDefinition>(TEXT("DA_Unit_TestHand"));
			Deckhand->DisplayName = FText::FromString(TEXT("Deckhand"));
			Deckhand->Tuning.DataTable = Units;
			Deckhand->Tuning.RowName = TEXT("Hand");
			Deckhand->BasicAttack = Cutlass;
			Deckhand->LearnablePool = HandPool;
			Deckhand->PerkPool = Perks;

			FDwCaptainTuningRow CaptainTuning;
			CaptainTuning.XpMultiplier = 1.5f;
			CaptainTuning.LevelThresholdMultiplier = 1.f;
			Captains = Table(TEXT("Cap"), CaptainTuning);
			Captain = Make<UDwCaptainDefinition>(TEXT("DA_Captain_Test"));
			Captain->CaptainUnit = CaptainUnit;
			Captain->StartingCrew = { Deckhand, Deckhand };
			Captain->Tuning.DataTable = Captains;
			Captain->Tuning.RowName = TEXT("Cap");

			FDwEconomyTuningRow EconomyTuning;
			EconomyTuning.StartingGold = 100;
			EconomyTuning.SinkLootMultiplier = 1.f;
			EconomyTuning.PlunderLootMultiplier = 1.5f;
			EconomyTuning.SurrenderLootMultiplier = 1.5f;
			EconomyTuning.PlunderItemChance = 1.f;
			EconomyTuning.RewardAbilityChoices = 3;
			Economy = Table(TEXT("Default"), EconomyTuning);
			FDwRunTuningRow RunTuning;
			RunTuning.PerkChoices = 3;
			RunTuning.MaxLevel = 4;
			RunTuning.MaxAbilitySlots = 2;
			RunTable = Table(TEXT("Default"), RunTuning);
			Progression = Make<UDataTable>(TEXT("DT_TestProgression"));
			Progression->RowStruct = FDwProgressionTuningRow::StaticStruct();
			SetProgression({ 100, 100, 200 });
			FDwEncounterTuningRow EncounterTuning;
			EncounterTuning.BaseGold = 150;
			EncounterTuning.ClearXp = 50;
			Encounters = Table(TEXT("Raiders"), EncounterTuning);
			FDwShipTuningRow ShipRow;
			ShipRow.MaxHealth = 250; ShipRow.CannonDamage = 70;

			Grog = Make<UDwItemDefinition>(TEXT("DA_Item_TestGrog"));
			Tonic = Make<UDwItemDefinition>(TEXT("DA_Item_TestTonic"));
			UDwShopDefinition* Shop = Make<UDwShopDefinition>(TEXT("DA_Shop_Test"));
			Shop->Items = { Grog, Tonic };

			GameData = Make<UDwGameData>(TEXT("DA_GameData_Test"));
			GameData->TuningTables.Economy = Economy;
			GameData->TuningTables.Progression = Progression;
			GameData->TuningTables.Run = RunTable;
			GameData->PlayerShipTuning.DataTable = Table(TEXT("PlayerShip"), ShipRow);
			GameData->PlayerShipTuning.RowName = TEXT("PlayerShip");
			GameData->DefaultShop = Shop;

			Early = Make<UDwEncounterDefinition>(TEXT("DA_Encounter_TestEarly"));
			Late = Make<UDwEncounterDefinition>(TEXT("DA_Encounter_TestLate"));
			Late->MinLayer = 4;
			for (UDwEncounterDefinition* Encounter : { Early, Late })
			{
				Encounter->Tuning.DataTable = Encounters;
				Encounter->Tuning.RowName = TEXT("Raiders");
			}
			Kraken = Make<UDwEncounterDefinition>(TEXT("DA_Encounter_TestKraken"));
			Kraken->Kind = EDwEncounterKind::Kraken;
			GameData->KrakenEncounter = Kraken;
		}

		TArray<const UDwEncounterDefinition*> Pool() const { return { Early, Late, Kraken }; }

		void Destroy()
		{
			for (UObject* Object : Rooted)
			{
				Object->RemoveFromRoot();
			}
			Rooted.Reset();
		}
	};

	/** A living crew member of Unit at Level, for the pure rules. */
	inline FDwCrewMemberState Member(const UDwUnitDefinition* Unit, int32 Level = 1, bool bIsCaptain = false)
	{
		FDwCrewMemberState Out;
		Out.Id = FGuid::NewGuid();
		Out.Unit = Unit;
		Out.bIsCaptain = bIsCaptain;
		Out.Level = Level;
		Out.Health = Out.GetMaxHealth();
		return Out;
	}

	/** A fight result where everyone alive survives at the given Health. */
	inline FDwCombatResult Won(const FDwRunState& State, float Health, EDwCombatOutcome Outcome = EDwCombatOutcome::VictoryCrewDefeated)
	{
		FDwCombatResult Result;
		Result.Outcome = Outcome;
		Result.Rounds = 4;
		Result.PlayerShipHealth = State.Ship.Health;
		for (const FDwCrewMemberState& Crew : State.Crew)
		{
			if (Crew.bAlive)
			{
				FDwCrewResult& Out = Result.Crew.AddDefaulted_GetRef();
				Out.RosterId = Crew.Id;
				Out.Health = Health;
			}
		}
		return Result;
	}

	inline FDwKillRecord KillBy(const FGuid& Killer, int32 Xp, EDwSide VictimSide = EDwSide::Enemy)
	{
		FDwKillRecord Kill;
		Kill.KillerRosterId = Killer;
		Kill.Xp = Xp;
		Kill.VictimSide = VictimSide;
		return Kill;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
