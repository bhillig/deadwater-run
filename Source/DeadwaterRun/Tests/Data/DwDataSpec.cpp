// Copyright Brandon Hillig 2026

// DW-004: Asset Manager setup, tuning tables carry the design numbers, definition validation rules.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwEventDefinition.h"
#include "Data/DwKrakenDefinition.h"
#include "Data/DwMapRules.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace DwDataSpec
{
	template <typename TRow>
	const TRow* Row(const UDataTable* Table, const TCHAR* RowName)
	{
		return Table ? Table->FindRow<TRow>(FName(RowName), TEXT("DwDataSpec"), /*bWarnIfRowMissing*/ false) : nullptr;
	}
}

BEGIN_DEFINE_SPEC(FDwDataSpec, "DeadwaterRun.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	const UDwGameData* GameData = nullptr;
END_DEFINE_SPEC(FDwDataSpec)

void FDwDataSpec::Define()
{
	BeforeEach([this]()
	{
		GameData = UDwAssetManager::Get().GetGameData();
	});

	Describe("AssetManager", [this]()
	{
		It("is the Deadwater asset manager", [this]()
		{
			TestNotNull(TEXT("UDwAssetManager configured"), Cast<UDwAssetManager>(&UAssetManager::Get()));
		});

		It("registers every definition primary asset type", [this]()
		{
			for (const FPrimaryAssetType& Type : UDwDefinitionBase::FTypes::All())
			{
				FPrimaryAssetTypeInfo Info;
				TestTrue(FString::Printf(TEXT("%s registered"), *Type.ToString()),
					UAssetManager::Get().GetPrimaryAssetTypeInfo(Type, Info));
			}
		});

		It("preloads definitions through the asset manager at startup", [this]()
		{
			// GetPrimaryAssetObject only returns objects the asset manager itself loaded (LoadAllDefinitions),
			// not ones loaded by path, so this fails if the startup load silently did nothing.
			const FPrimaryAssetId Id(UDwDefinitionBase::FTypes::GameData, UDwGameData::AssetName);
			TestNotNull(TEXT("DA_GameData loaded by UDwAssetManager"), UAssetManager::Get().GetPrimaryAssetObject(Id));
		});

		It("finds DA_GameData with every tuning table bound to the right row struct", [this]()
		{
			if (!TestNotNull(TEXT("DA_GameData"), GameData))
			{
				return;
			}
			const FDwTuningTables& T = GameData->TuningTables;
			const TPair<const UDataTable*, const UScriptStruct*> Tables[] = {
				{ T.Unit, FDwUnitTuningRow::StaticStruct() },
				{ T.Ability, FDwAbilityTuningRow::StaticStruct() },
				{ T.Item, FDwItemTuningRow::StaticStruct() },
				{ T.Perk, FDwPerkTuningRow::StaticStruct() },
				{ T.Captain, FDwCaptainTuningRow::StaticStruct() },
				{ T.Ship, FDwShipTuningRow::StaticStruct() },
				{ T.Encounter, FDwEncounterTuningRow::StaticStruct() },
				{ T.Economy, FDwEconomyTuningRow::StaticStruct() },
				{ T.Progression, FDwProgressionTuningRow::StaticStruct() },
				{ T.Run, FDwRunTuningRow::StaticStruct() },
				{ T.Combat, FDwCombatTuningRow::StaticStruct() },
				{ T.Kraken, FDwKrakenTuningRow::StaticStruct() },
			};
			for (const TPair<const UDataTable*, const UScriptStruct*>& Pair : Tables)
			{
				if (TestNotNull(FString::Printf(TEXT("Table for %s"), *Pair.Value->GetName()), Pair.Key))
				{
					TestEqual(TEXT("Row struct"), Pair.Key->GetRowStruct(), Pair.Value);
				}
			}
			TestNotNull(TEXT("Player ship tuning row"), GameData->PlayerShipTuning.GetRow<FDwShipTuningRow>(TEXT("DwDataSpec")));
		});
	});

	Describe("Tuning", [this]()
	{
		// Every number in DeadwaterRun.md, read back through the imported DataTables.
		It("has the crew and ship numbers from the design doc", [this]()
		{
			if (!TestNotNull(TEXT("DA_GameData"), GameData)) { return; }
			for (const TCHAR* Unit : { TEXT("Captain_TeamLeader"), TEXT("Captain_Fearmongerer"), TEXT("Captain_Conqueror"), TEXT("Swordsman"), TEXT("Sniper") })
			{
				const FDwUnitTuningRow* R = DwDataSpec::Row<FDwUnitTuningRow>(GameData->TuningTables.Unit, Unit);
				if (TestNotNull(Unit, R))
				{
					TestEqual(FString::Printf(TEXT("%s HP"), Unit), R->MaxHealth, 100);
					TestEqual(FString::Printf(TEXT("%s MP"), Unit), R->MaxMana, 20);
					TestEqual(FString::Printf(TEXT("%s move"), Unit), R->MoveRange, 5);
					TestEqual(FString::Printf(TEXT("%s actions"), Unit), R->MaxActionPoints, 1);
				}
			}
			const FDwShipTuningRow* Ship = DwDataSpec::Row<FDwShipTuningRow>(GameData->TuningTables.Ship, TEXT("PlayerShip"));
			if (TestNotNull(TEXT("PlayerShip"), Ship))
			{
				TestEqual(TEXT("Ship HP"), Ship->MaxHealth, 250);
				TestEqual(TEXT("Cannon damage"), Ship->CannonDamage, 70);
			}
		});

		It("has the captain ability numbers from the design doc", [this]()
		{
			if (!TestNotNull(TEXT("DA_GameData"), GameData)) { return; }
			const UDataTable* A = GameData->TuningTables.Ability;
			const FDwAbilityTuningRow* Rally = DwDataSpec::Row<FDwAbilityTuningRow>(A, TEXT("Captain_RallyCry"));
			const FDwAbilityTuningRow* Grapple = DwDataSpec::Row<FDwAbilityTuningRow>(A, TEXT("Captain_GrappleHook"));
			const FDwAbilityTuningRow* Smoke = DwDataSpec::Row<FDwAbilityTuningRow>(A, TEXT("Captain_SmokeAndFire"));
			const FDwAbilityTuningRow* Pistols = DwDataSpec::Row<FDwAbilityTuningRow>(A, TEXT("Captain_SixPistols"));
			const FDwAbilityTuningRow* Charge = DwDataSpec::Row<FDwAbilityTuningRow>(A, TEXT("Captain_Charge"));
			const FDwAbilityTuningRow* Eye = DwDataSpec::Row<FDwAbilityTuningRow>(A, TEXT("Captain_EyeOfTheTiger"));
			if (!TestTrue(TEXT("All captain ability rows exist"), Rally && Grapple && Smoke && Pistols && Charge && Eye)) { return; }
			TestEqual(TEXT("Rally Cry heals 15"), Rally->Heal, 15);
			TestEqual(TEXT("Grapple range 10"), Grapple->Range, 10);
			TestEqual(TEXT("Smoke and Fire adds 1 Fear"), Smoke->Fear, 1);
			TestEqual(TEXT("Six Pistols 15 damage"), Pistols->Damage, 15);
			TestEqual(TEXT("Six Pistols 6 shots"), Pistols->MinHits, 6);
			TestEqual(TEXT("Six Pistols 6 shots (max)"), Pistols->MaxHits, 6);
			TestEqual(TEXT("Charge up to 6 tiles"), Charge->Range, 6);
			TestEqual(TEXT("Eye of the Tiger freezes 1 turn"), Eye->DurationTurns, 1);
		});

		It("has the Kraken numbers from the design doc", [this]()
		{
			if (!TestNotNull(TEXT("DA_GameData"), GameData)) { return; }
			const UDataTable* A = GameData->TuningTables.Ability;
			const FDwUnitTuningRow* Body = DwDataSpec::Row<FDwUnitTuningRow>(GameData->TuningTables.Unit, TEXT("KrakenBody"));
			const FDwAbilityTuningRow* Slam = DwDataSpec::Row<FDwAbilityTuningRow>(A, TEXT("Kraken_TentacleSlam"));
			const FDwAbilityTuningRow* Swipe = DwDataSpec::Row<FDwAbilityTuningRow>(A, TEXT("Kraken_KrankySwipe"));
			const FDwAbilityTuningRow* Grab = DwDataSpec::Row<FDwAbilityTuningRow>(A, TEXT("Kraken_TentacleGrab"));
			const FDwAbilityTuningRow* Fury = DwDataSpec::Row<FDwAbilityTuningRow>(A, TEXT("Kraken_ArmsOfFury"));
			if (!TestTrue(TEXT("All Kraken rows exist"), Body && Slam && Swipe && Grab && Fury)) { return; }
			TestEqual(TEXT("Kraken 500 HP"), Body->MaxHealth, 500);
			TestEqual(TEXT("Tentacle Slam 50"), Slam->Damage, 50);
			TestEqual(TEXT("Kranky Swipe 20"), Swipe->Damage, 20);
			TestEqual(TEXT("Tentacle Grab 1 turn"), Grab->DurationTurns, 1);
			TestEqual(TEXT("Arms of Fury 40 per hit"), Fury->Damage, 40);
			TestEqual(TEXT("Arms of Fury min 3 hits"), Fury->MinHits, 3);
			TestEqual(TEXT("Arms of Fury max 5 hits"), Fury->MaxHits, 5);
		});

		It("has the captain run-rule, economy, run and combat numbers from the design doc", [this]()
		{
			if (!TestNotNull(TEXT("DA_GameData"), GameData)) { return; }
			const FDwTuningTables& T = GameData->TuningTables;
			const FDwCaptainTuningRow* Leader = DwDataSpec::Row<FDwCaptainTuningRow>(T.Captain, TEXT("TeamLeader"));
			const FDwCaptainTuningRow* Fear = DwDataSpec::Row<FDwCaptainTuningRow>(T.Captain, TEXT("Fearmongerer"));
			const FDwCaptainTuningRow* Conq = DwDataSpec::Row<FDwCaptainTuningRow>(T.Captain, TEXT("Conqueror"));
			const FDwEconomyTuningRow* Eco = DwDataSpec::Row<FDwEconomyTuningRow>(T.Economy, TEXT("Default"));
			const FDwRunTuningRow* Run = DwDataSpec::Row<FDwRunTuningRow>(T.Run, TEXT("Default"));
			const FDwCombatTuningRow* Combat = DwDataSpec::Row<FDwCombatTuningRow>(T.Combat, TEXT("Default"));
			const FDwEncounterTuningRow* Fight = DwDataSpec::Row<FDwEncounterTuningRow>(T.Encounter, TEXT("Placeholder"));
			if (!TestTrue(TEXT("All rows exist"), Leader && Fear && Conq && Eco && Run && Combat && Fight)) { return; }

			TestEqual(TEXT("Veteran Crew +50% XP"), Leader->XpMultiplier, 1.5f);
			TestEqual(TEXT("Veteran Crew levels twice as fast"), Leader->LevelThresholdMultiplier, 0.5f);
			TestEqual(TEXT("Dread Reputation +1 Fear per surrender"), Fear->FearPerSurrender, 1);
			TestEqual(TEXT("Conqueror +1% per kill"), Conq->KillBonusPerKill, 0.01f);
			TestEqual(TEXT("Conqueror max 20%"), Conq->KillBonusCap, 0.2f);

			TestEqual(TEXT("Sink 1x loot"), Eco->SinkLootMultiplier, 1.f);
			TestEqual(TEXT("Plunder 1.5x loot"), Eco->PlunderLootMultiplier, 1.5f);
			TestEqual(TEXT("Surrender 1.5x loot"), Eco->SurrenderLootMultiplier, 1.5f);
			TestEqual(TEXT("Repair 100 gold"), Eco->RepairCost, 100);
			TestEqual(TEXT("Abilities 50 gold"), Eco->AbilityUnlockCost, 50);
			TestEqual(TEXT("Items 30 gold"), Eco->ItemCost, 30);
			TestEqual(TEXT("Max crew 4"), Eco->MaxCrew, 4);
			TestEqual(TEXT("Example fight 150 gold"), Fight->BaseGold, 150);

			TestEqual(TEXT("Events 60% good"), Run->GoodEventChance, 0.6f);
			TestEqual(TEXT("Cower at 3 Fear"), Combat->MaxFear, 3);
			TestEqual(TEXT("Climb cost 2 (C5)"), Combat->ClimbMoveCost, 2);
			TestEqual(TEXT("Boarding cost 1"), Combat->BoardingMoveCost, 1);
			TestEqual(TEXT("Cower policy parsed from JSON enum name"), Combat->FearCowerPolicy, EDwFearCowerPolicy::ResetToZero);
		});
	});

#if WITH_EDITOR // IsDataValid is editor-only
	Describe("Validation", [this]()
	{
		It("rejects a captain without exactly 2 abilities", [this]()
		{
			if (!TestNotNull(TEXT("DA_GameData"), GameData)) { return; }
			UDwCaptainDefinition* Captain = NewObject<UDwCaptainDefinition>(GetTransientPackage(), TEXT("DA_Captain_Test"));
			Captain->DisplayName = FText::FromString(TEXT("Test"));
			Captain->Tuning.DataTable = GameData->TuningTables.Captain;
			Captain->Tuning.RowName = TEXT("TeamLeader");
			Captain->CaptainUnit = NewObject<UDwUnitDefinition>(GetTransientPackage());
			Captain->CaptainAbilities.Add(NewObject<UDwAbilityDefinition>(GetTransientPackage()));

			FDataValidationContext OneAbility;
			TestEqual(TEXT("1 ability is invalid"), Captain->IsDataValid(OneAbility), EDataValidationResult::Invalid);

			Captain->CaptainAbilities.Add(NewObject<UDwAbilityDefinition>(GetTransientPackage()));
			FDataValidationContext TwoAbilities;
			TestEqual(TEXT("2 abilities is valid"), Captain->IsDataValid(TwoAbilities), EDataValidationResult::Valid);
		});

		It("rejects a tuning row from the wrong table or a missing row", [this]()
		{
			if (!TestNotNull(TEXT("DA_GameData"), GameData)) { return; }
			UDwCaptainDefinition* Captain = NewObject<UDwCaptainDefinition>(GetTransientPackage(), TEXT("DA_Captain_TuningTest"));
			Captain->DisplayName = FText::FromString(TEXT("Test"));
			Captain->CaptainUnit = NewObject<UDwUnitDefinition>(GetTransientPackage());
			Captain->CaptainAbilities = { NewObject<UDwAbilityDefinition>(GetTransientPackage()), NewObject<UDwAbilityDefinition>(GetTransientPackage()) };

			Captain->Tuning.DataTable = GameData->TuningTables.Unit;
			Captain->Tuning.RowName = TEXT("Swordsman");
			FDataValidationContext WrongTable;
			TestEqual(TEXT("Unit table on a captain is invalid"), Captain->IsDataValid(WrongTable), EDataValidationResult::Invalid);

			Captain->Tuning.DataTable = GameData->TuningTables.Captain;
			Captain->Tuning.RowName = TEXT("NoSuchCaptain");
			FDataValidationContext MissingRow;
			TestEqual(TEXT("Missing row is invalid"), Captain->IsDataValid(MissingRow), EDataValidationResult::Invalid);

			Captain->Tuning.DataTable = GameData->TuningTables.Unit;
			Captain->Tuning.RowName = TEXT("Swordsman");
			TestNull(TEXT("Typed lookup refuses the wrong table"), Captain->GetTuningRow<FDwCaptainTuningRow>());
		});

		It("enforces event choice counts, Kraken phase order and map node limits", [this]()
		{
			UDwEventDefinition* Event = NewObject<UDwEventDefinition>(GetTransientPackage(), TEXT("DA_Event_Test"));
			Event->DisplayName = FText::FromString(TEXT("Test"));
			FDataValidationContext NoChoices;
			TestEqual(TEXT("0 choices invalid"), Event->IsDataValid(NoChoices), EDataValidationResult::Invalid);
			FDwEventChoice Choice;
			Choice.Label = FText::FromString(TEXT("Sail on"));
			Event->Choices.Add(Choice);
			FDataValidationContext OneChoice;
			TestEqual(TEXT("1 choice valid"), Event->IsDataValid(OneChoice), EDataValidationResult::Valid);
			Event->Choices.Init(Choice, 4);
			FDataValidationContext FourChoices;
			TestEqual(TEXT("4 choices invalid"), Event->IsDataValid(FourChoices), EDataValidationResult::Invalid);

			UDwKrakenDefinition* Kraken = NewObject<UDwKrakenDefinition>(GetTransientPackage(), TEXT("DA_Kraken_Test"));
			Kraken->Phases.SetNum(2);
			Kraken->Phases[0].EnterBelowHealthPct = 1.f;
			Kraken->Phases[1].EnterBelowHealthPct = 0.3f;
			FDataValidationContext KrakenCtx;
			Kraken->IsDataValid(KrakenCtx);
			const bool bHasPhaseError = KrakenCtx.GetIssues().ContainsByPredicate([](const FDataValidationContext::FIssue& Issue)
				{ return Issue.Message.ToString().Contains(TEXT("phase")); });
			TestFalse(TEXT("1.0 then 0.3 is a valid phase order"), bHasPhaseError);
			Kraken->Phases[1].EnterBelowHealthPct = 1.f;
			FDataValidationContext BadOrder;
			Kraken->IsDataValid(BadOrder);
			TestTrue(TEXT("Non-descending phases rejected"), BadOrder.GetIssues().ContainsByPredicate([](const FDataValidationContext::FIssue& Issue)
				{ return Issue.Message.ToString().Contains(TEXT("previous phase")); }));

			UDwMapRules* Rules = NewObject<UDwMapRules>(GetTransientPackage(), TEXT("DA_MapRules_Test"));
			Rules->DisplayName = FText::FromString(TEXT("Test"));
			FDataValidationContext Defaults;
			TestEqual(TEXT("Design defaults (8-9 nodes, 5 fights) valid"), Rules->IsDataValid(Defaults), EDataValidationResult::Valid);
			Rules->MinFightsPerPath = 9;
			FDataValidationContext TooManyFights;
			TestEqual(TEXT("More fights than nodes invalid"), Rules->IsDataValid(TooManyFights), EDataValidationResult::Invalid);
		});
	});
#endif // WITH_EDITOR
}

#endif // WITH_DEV_AUTOMATION_TESTS
