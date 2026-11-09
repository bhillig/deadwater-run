// Copyright Brandon Hillig 2026

// DW-103: combat subsystem and turn manager in a transient world (Docs/Architecture/03 "How it's tested", 06 sinking).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/DwGameplayAbility.h"
#include "Combat/DwCombatRules.h"
#include "Board/DwDeckLayout.h"
#include "Combat/DwEncounterSetupBuilder.h"
#include "Core/DwGameData.h"
#include "Data/DwEncounterDefinition.h"
#include "Tests/Combat/DwCombatTestFixture.h"

BEGIN_DEFINE_SPEC(FDwCombatSpec, "DeadwaterRun.Combat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	FDwCombatFixture F;
	UDwTestScriptedController* Enemy = nullptr;

	/** Enemies run Script on their turn (then end the unit). */
	void ScriptEnemies(TFunction<void(UDwCombatSubsystem&, FDwUnitHandle)> Script)
	{
		Enemy = F.Root(NewObject<UDwTestScriptedController>());
		Enemy->OnUnitTurn = MoveTemp(Script);
		F.Combat->SetSideController(EDwSide::Enemy, Enemy);
	}

	int32 CountResolved(EDwCommandType Type) const
	{
		return F.Listener->Resolved.FilterByPredicate([Type](const FDwCombatCommand& C) { return C.Type == Type; }).Num();
	}

END_DEFINE_SPEC(FDwCombatSpec)

void FDwCombatSpec::Define()
{
	BeforeEach([this]() { F.Create(); });
	AfterEach([this]() { F.Destroy(); });

	Describe("Spawn", [this]()
	{
		It("puts 3 crew and 3 enemies on their spawn tiles, on two 250 HP ships", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			TestEqual(TEXT("Six units"), F.Combat->GetUnits().Num(), 6);
			for (int32 Index = 0; Index < 3; ++Index)
			{
				TestEqual(TEXT("Crew in column 0"), F.Unit(F.CrewHandle(Index))->GetTile(), FIntPoint(0, Index));
				TestEqual(TEXT("Enemy in column 9"), F.Unit(F.EnemyHandle(Index))->GetTile(), FIntPoint(9, Index));
				TestEqual(TEXT("Grid knows the enemy"), F.Combat->GetGrid().GetOccupant(FIntPoint(9, Index)).Unit, F.EnemyHandle(Index));
				TestEqual(TEXT("Enemy side"), F.Unit(F.EnemyHandle(Index))->GetSide(), EDwSide::Enemy);
			}
			TestEqual(TEXT("Player ship 250"), F.ShipHealth(EDwSide::Player), 250.f);
			TestEqual(TEXT("Enemy ship 250"), F.ShipHealth(EDwSide::Enemy), 250.f);
			TestTrue(TEXT("Player cannon tile"), F.Combat->GetShip(EDwSide::Player)->GetCannonTiles() == TArray<FIntPoint>{ FIntPoint(2, 1) });
			TestTrue(TEXT("Enemy cannon tile (mirrored)"), F.Combat->GetShip(EDwSide::Enemy)->GetCannonTiles() == TArray<FIntPoint>{ FIntPoint(7, 1) });
			TestTrue(TEXT("Crew 1 is the captain"), F.Asc(F.CrewHandle(0))->HasMatchingGameplayTag(DwTags::Unit_Role_Captain));
			TestFalse(TEXT("Crew 2 is not"), F.Asc(F.CrewHandle(1))->HasMatchingGameplayTag(DwTags::Unit_Role_Captain));
		});

		It("carries Health over, always starts Mana at max (C9) and applies level gains", [this]()
		{
			F.Setup.Crew[1].Health = 40.f;
			F.Setup.Crew[2].Level = 3;
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			TestEqual(TEXT("Health carried"), F.Health(F.CrewHandle(1)), 40.f);
			TestEqual(TEXT("Mana full"), F.Attr(F.CrewHandle(1), UDwUnitAttributeSet::GetManaAttribute()), 20.f);
			TestEqual(TEXT("Level 3: +2 x 10 MaxHealth"), F.Attr(F.CrewHandle(2), UDwUnitAttributeSet::GetMaxHealthAttribute()), 120.f);
			TestEqual(TEXT("Full health by default"), F.Health(F.CrewHandle(2)), 120.f);
			TestEqual(TEXT("MaxFear from setup"), F.Attr(F.EnemyHandle(0), UDwUnitAttributeSet::GetMaxFearAttribute()), 3.f);
		});

		It("grants one ability per tag: basic attack, defaults, then common abilities", [this]()
		{
			F.Setup.CommonAbilities = { F.Strike }; // same tag as the basic attack: not granted twice
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			const FDwCombatSnapshot Snap = F.Combat->GetSnapshot();
			const FDwUnitSnapshot* Unit = Snap.FindUnit(F.CrewHandle(0));
			TestEqual(TEXT("Four abilities"), Unit->Abilities.Num(), 4);
			TestTrue(TEXT("Basic attack first"), Unit->Abilities[0] == F.Strike);
			TestTrue(TEXT("Tag list"), Unit->GrantedAbilityTags.HasTagExact(DwTags::Ability_Cannon));
		});

		It("refuses a setup with too few spawn tiles and leaves nothing behind", [this]()
		{
			AddExpectedError(TEXT("spawn tiles"), EAutomationExpectedErrorFlags::Contains, 1);
			const FDwCombatantSetup Extra = F.Setup.Crew[1];
			F.Setup.Crew.Add(Extra);
			TestFalse(TEXT("Refused"), F.Start());
			TestFalse(TEXT("Not active"), F.Combat->IsActive());
			TestEqual(TEXT("No units"), F.Combat->GetUnits().Num(), 0);
			TestNull(TEXT("No board"), F.Combat->GetBoard());
		});
	});

	Describe("Encounter definitions", [this]()
	{
		It("builds and starts a 3 v 3 ship battle from DA_GameData and an encounter definition", [this]()
		{
			auto MakeLayout = [this]()
			{
				UDwDeckLayout* Layout = F.Root(NewObject<UDwDeckLayout>());
				const FDwDeckSource Source = FDwDeckAscii::Parse(FDwCombatFixture::DefaultDeck());
				Layout->Size = Source.Size;
				Layout->Tiles = Source.Tiles;
				return Layout;
			};
			UDataTable* Ships = F.Root(NewObject<UDataTable>());
			Ships->RowStruct = FDwShipTuningRow::StaticStruct();
			FDwShipTuningRow Ship;
			Ship.MaxHealth = 250; Ship.CannonDamage = 70;
			Ships->AddRow(TEXT("PlayerShip"), Ship);
			Ships->AddRow(TEXT("EnemyPlaceholder"), Ship);
			UDataTable* CombatTable = F.Root(NewObject<UDataTable>());
			CombatTable->RowStruct = FDwCombatTuningRow::StaticStruct();
			FDwCombatTuningRow Combat;
			Combat.BoardingMoveCost = 1; Combat.ClimbMoveCost = 2; Combat.RangeBonusPerElevation = 2; Combat.MaxFear = 3;
			CombatTable->AddRow(TEXT("Default"), Combat);

			UDwGameData* GameData = F.Root(NewObject<UDwGameData>());
			GameData->DefaultPlayerDeck = MakeLayout();
			GameData->PlayerShipTuning.DataTable = Ships;
			GameData->PlayerShipTuning.RowName = TEXT("PlayerShip");
			GameData->TuningTables.Combat = CombatTable;
			GameData->CommonAbilities = { F.Latent };

			UDwEncounterDefinition* Encounter = F.Root(NewObject<UDwEncounterDefinition>());
			Encounter->ShipDeck = MakeLayout();
			Encounter->ShipTuning.DataTable = Ships;
			Encounter->ShipTuning.RowName = TEXT("EnemyPlaceholder");
			for (int32 Index = 0; Index < 3; ++Index)
			{
				FDwEnemySlot& Slot = Encounter->Units.AddDefaulted_GetRef();
				Slot.UnitDefinition = F.Grunt;
				Slot.Level = Index + 1;
			}

			FDwEncounterSetup Setup;
			TArray<FString> Errors;
			const bool bBuilt = FDwEncounterSetupBuilder::BuildShipBattle(*GameData, *Encounter, F.Setup.Crew, 11, Setup, Errors);
			if (!TestTrue(FString::Printf(TEXT("Built: %s"), *FString::Join(Errors, TEXT("; "))), bBuilt)) { return; }
			TestEqual(TEXT("Enemies in encounter order"), Setup.Enemies.Num(), 3);
			TestEqual(TEXT("Board 10 wide"), Setup.Grid.Size.X, 10);
			if (!TestTrue(TEXT("Started"), F.Combat->StartEncounter(Setup))) { return; }
			TestEqual(TEXT("Six units"), F.Combat->GetUnits().Num(), 6);
			TestEqual(TEXT("Enemy ship 250"), F.ShipHealth(EDwSide::Enemy), 250.f);
			TestEqual(TEXT("Player ship 250"), F.ShipHealth(EDwSide::Player), 250.f);
			TestEqual(TEXT("Enemy level 3: 120 HP"), F.Health(F.EnemyHandle(2)), 120.f);
			TestTrue(TEXT("Common ability granted"), F.Combat->GetSnapshot().FindUnit(F.EnemyHandle(0))->GrantedAbilityTags.HasTagExact(F.Latent->AbilityTag));
		});

		It("reports every missing piece of data", [this]()
		{
			UDwGameData* GameData = F.Root(NewObject<UDwGameData>());
			UDwEncounterDefinition* Encounter = F.Root(NewObject<UDwEncounterDefinition>());
			Encounter->Units.AddDefaulted();
			FDwEncounterSetup Setup;
			TArray<FString> Errors;
			TestFalse(TEXT("Refused"), FDwEncounterSetupBuilder::BuildShipBattle(*GameData, *Encounter, {}, 0, Setup, Errors));
			TestEqual(TEXT("Decks, combat row, two ship rows, empty slot"), Errors.Num(), 5);
		});
	});

	Describe("Turns", [this]()
	{
		It("starts with the player side; the round advances only after both sides", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			TestEqual(TEXT("Player first"), F.Combat->GetActiveSide(), EDwSide::Player);
			TestEqual(TEXT("Round 1"), F.Combat->GetRound(), 1);
			TestEqual(TEXT("End turn"), F.EndTurn(), EDwCommandResult::Accepted);
			// The passive enemy ended each of its three units in order, then the turn came back.
			TestEqual(TEXT("Three enemy EndUnits"), CountResolved(EDwCommandType::EndUnit), 3);
			TestEqual(TEXT("Player again"), F.Combat->GetActiveSide(), EDwSide::Player);
			TestEqual(TEXT("Round 2"), F.Combat->GetRound(), 2);
			TestEqual(TEXT("TurnStarted x3 (P, E, P)"), F.Listener->Count(EDwCombatEventType::TurnStarted), 3);
		});

		It("lets enemies act one at a time in encounter order (C14)", [this]()
		{
			TArray<FDwUnitHandle> Order;
			ScriptEnemies([this, &Order](UDwCombatSubsystem& Combat, FDwUnitHandle Unit)
			{
				Order.Add(Unit);
				TestEqual(TEXT("Snapshot names the acting enemy"), Combat.GetSnapshot().ActingUnit, Unit);
				TestEqual(TEXT("Enemy strikes crew 2"), F.Use(Unit, F.Strike, F.CrewHandle(1)), EDwCommandResult::Accepted);
				TestEqual(TEXT("Another enemy can't act out of turn"),
					F.Use(Unit == F.EnemyHandle(0) ? F.EnemyHandle(1) : F.EnemyHandle(0), F.Strike, F.CrewHandle(1)), EDwCommandResult::NotYourTurn);
			});
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.EndTurn();
			TestTrue(TEXT("Order"), Order == TArray<FDwUnitHandle>{ F.EnemyHandle(0), F.EnemyHandle(1), F.EnemyHandle(2) });
			TestEqual(TEXT("Three hits of 30"), F.Health(F.CrewHandle(1)), 10.f);
			TestEqual(TEXT("Player again"), F.Combat->GetActiveSide(), EDwSide::Player);
		});

		It("refreshes budgets at each side-turn start", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.Use(F.CrewHandle(0), F.Strike, F.EnemyHandle(0));
			TestEqual(TEXT("Action spent"), F.Attr(F.CrewHandle(0), UDwUnitAttributeSet::GetActionPointsAttribute()), 0.f);
			F.EndTurn();
			TestEqual(TEXT("Action back"), F.Attr(F.CrewHandle(0), UDwUnitAttributeSet::GetActionPointsAttribute()), 1.f);
		});

		It("allows End Turn with every unit unused, but only for the active side", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			TestEqual(TEXT("Enemy can't end the player's turn"), F.EndTurn(EDwSide::Enemy), EDwCommandResult::NotYourTurn);
			TestEqual(TEXT("Player ends with nothing done"), F.EndTurn(), EDwCommandResult::Accepted);
		});

		It("suggests End Turn once when the last player action is spent, and never ends it", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.Use(F.CrewHandle(0), F.Strike, F.EnemyHandle(0));
			F.Use(F.CrewHandle(1), F.Strike, F.EnemyHandle(0));
			TestEqual(TEXT("Not yet: crew 3 can still act"), F.Listener->EndTurnSuggestions, 0);
			F.Use(F.CrewHandle(2), F.Strike, F.EnemyHandle(0));
			TestEqual(TEXT("Suggested once"), F.Listener->EndTurnSuggestions, 1);
			TestEqual(TEXT("Rejected command doesn't re-suggest"), F.Use(F.CrewHandle(2), F.Strike, F.EnemyHandle(0)), EDwCommandResult::CannotAfford);
			TestEqual(TEXT("Still once"), F.Listener->EndTurnSuggestions, 1);
			TestEqual(TEXT("Still the player's turn"), F.Combat->GetActiveSide(), EDwSide::Player);
		});

		It("skips a frozen enemy exactly once", [this]()
		{
			ScriptEnemies([](UDwCombatSubsystem&, FDwUnitHandle) {});
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.ApplyEffect(DwGas::GetSharedEffects().Frozen, F.EnemyHandle(0), F.CrewHandle(0));
			F.EndTurn();
			TestEqual(TEXT("Two enemies acted"), Enemy->UnitTurns, 2);
			TestEqual(TEXT("Skipped once"), F.Listener->Count(EDwCombatEventType::UnitTurnSkipped, F.EnemyHandle(0)), 1);
			TestFalse(TEXT("Thawed after its turn"), F.Asc(F.EnemyHandle(0))->HasMatchingGameplayTag(DwTags::Status_Incapacitated_Frozen));
			F.EndTurn();
			TestEqual(TEXT("All three act next round"), Enemy->UnitTurns, 5);
			TestEqual(TEXT("Still skipped only once"), F.Listener->Count(EDwCombatEventType::UnitTurnSkipped, F.EnemyHandle(0)), 1);
		});

		It("makes an enemy at MaxFear cower for its turn, then applies the fear policy", [this]()
		{
			ScriptEnemies([](UDwCombatSubsystem&, FDwUnitHandle) {});
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.Asc(F.EnemyHandle(1))->SetNumericAttributeBase(UDwUnitAttributeSet::GetFearAttribute(), 3.f);
			F.EndTurn();
			TestEqual(TEXT("Cowering enemy skipped"), Enemy->UnitTurns, 2);
			const FDwCombatEvent* Skip = F.Listener->Events.FindByPredicate([this](const FDwCombatEvent& E)
				{ return E.Type == EDwCombatEventType::UnitTurnSkipped && E.Target == F.EnemyHandle(1); });
			TestTrue(TEXT("Skip names cowering"), Skip && Skip->Tag == DwTags::Status_Incapacitated_Cowering);
			TestEqual(TEXT("Cowering status shown"), F.Listener->Count(EDwCombatEventType::StatusApplied, F.EnemyHandle(1)), 2); // Afraid + Cowering
			TestEqual(TEXT("ResetToZero after cowering"), F.Attr(F.EnemyHandle(1), UDwUnitAttributeSet::GetFearAttribute()), 0.f);
			TestFalse(TEXT("No longer cowering"), F.Asc(F.EnemyHandle(1))->HasMatchingGameplayTag(DwTags::Status_Incapacitated_Cowering));
			F.EndTurn();
			TestEqual(TEXT("Acts next round"), Enemy->UnitTurns, 5);
		});

		It("never skips a fear-immune unit at MaxFear", [this]()
		{
			ScriptEnemies([](UDwCombatSubsystem&, FDwUnitHandle) {});
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.Asc(F.EnemyHandle(0))->AddLooseGameplayTag(DwTags::Unit_Immune_Fear);
			F.Asc(F.EnemyHandle(0))->SetNumericAttributeBase(UDwUnitAttributeSet::GetFearAttribute(), 3.f);
			F.EndTurn();
			TestEqual(TEXT("All acted"), Enemy->UnitTurns, 3);
		});
	});

	Describe("Commands", [this]()
	{
		It("applies an accepted attack and records it", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			TestEqual(TEXT("Accepted"), F.Use(F.CrewHandle(0), F.Strike, F.EnemyHandle(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("30 damage"), F.Health(F.EnemyHandle(0)), 70.f);
			const FDwCombatEvent* Hit = F.Listener->Events.FindByPredicate([](const FDwCombatEvent& E) { return E.Type == EDwCombatEventType::UnitDamaged; });
			TestTrue(TEXT("Damage event with source and amount"), Hit && Hit->Source == F.CrewHandle(0) && Hit->Target == F.EnemyHandle(0) && Hit->Magnitude == 30.f);
			TestEqual(TEXT("Recorded"), F.Combat->GetRecord().Commands.Num(), 1);
			TestEqual(TEXT("Resolved broadcast"), CountResolved(EDwCommandType::Ability), 1);
		});

		It("rejects wrong side, wrong target, dead units, unknown abilities and unaffordable actions", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			TestEqual(TEXT("Enemy on the player turn"), F.Use(F.EnemyHandle(0), F.Strike, F.CrewHandle(0)), EDwCommandResult::NotYourTurn);
			TestEqual(TEXT("Attack an ally"), F.Use(F.CrewHandle(0), F.Strike, F.CrewHandle(1)), EDwCommandResult::InvalidTarget);
			TestEqual(TEXT("No such unit"), F.Use(FDwUnitHandle(42), F.Strike, F.EnemyHandle(0)), EDwCommandResult::InvalidUnit);
			TestEqual(TEXT("Not granted"), F.Combat->SubmitCommand(FDwCombatCommand::MakeMove(F.CrewHandle(0), FIntPoint(1, 0))), EDwCommandResult::UnknownAbility);
			F.Use(F.CrewHandle(0), F.Execute, F.EnemyHandle(0));
			TestEqual(TEXT("Dead target"), F.Use(F.CrewHandle(1), F.Strike, F.EnemyHandle(0)), EDwCommandResult::InvalidTarget);
			TestEqual(TEXT("Second action"), F.Use(F.CrewHandle(0), F.Strike, F.EnemyHandle(1)), EDwCommandResult::CannotAfford);
			F.Use(F.CrewHandle(1), F.Execute, F.CrewHandle(0)); // invalid: own side
			TestEqual(TEXT("Only the accepted command is recorded"), F.Combat->GetRecord().Commands.Num(), 1);
		});

		It("refuses every command while an ability is resolving (input lock)", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			const FDwUnitHandle Captain = F.CrewHandle(0);
			TestEqual(TEXT("Latent accepted"), F.Combat->SubmitCommand(FDwCombatCommand::MakeAbility(Captain, F.Latent->AbilityTag,
				FDwAbilityTarget::MakeUnit(Captain, F.Unit(Captain)->GetTile()))), EDwCommandResult::Accepted);
			TestTrue(TEXT("Resolving"), F.Combat->IsResolving());
			TestEqual(TEXT("Other unit busy"), F.Use(F.CrewHandle(1), F.Strike, F.EnemyHandle(0)), EDwCommandResult::Busy);
			TestEqual(TEXT("End turn busy"), F.EndTurn(), EDwCommandResult::Busy);
			FGameplayAbilitySpec* Spec = F.Asc(Captain)->FindSpecByAbilityTag(F.Latent->AbilityTag);
			F.Asc(Captain)->CancelAbilityHandle(Spec->Handle); // the animation finishes
			TestFalse(TEXT("Resolved"), F.Combat->IsResolving());
			TestEqual(TEXT("Input back"), F.Use(F.CrewHandle(1), F.Strike, F.EnemyHandle(0)), EDwCommandResult::Accepted);
		});
	});

	Describe("Death and outcome", [this]()
	{
		It("credits the killer, frees the tile and skips the dead in the enemy sequence", [this]()
		{
			ScriptEnemies([](UDwCombatSubsystem&, FDwUnitHandle) {});
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.Use(F.CrewHandle(1), F.Execute, F.EnemyHandle(0));
			TestFalse(TEXT("Dead"), F.Unit(F.EnemyHandle(0))->IsAlive());
			TestFalse(TEXT("Tile freed"), F.Combat->GetGrid().IsOccupied(FIntPoint(9, 0)));
			TestEqual(TEXT("UnitDied once"), F.Listener->Count(EDwCombatEventType::UnitDied, F.EnemyHandle(0)), 1);
			F.EndTurn();
			TestEqual(TEXT("Dead enemy doesn't act"), Enemy->UnitTurns, 2);
		});

		It("wins by defeating the crew, then refuses further commands", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			for (int32 Index = 0; Index < 3; ++Index)
			{
				F.Use(F.CrewHandle(Index), F.Execute, F.EnemyHandle(Index));
			}
			TestEqual(TEXT("Plunder"), F.Combat->GetOutcome(), EDwCombatOutcome::VictoryCrewDefeated);
			if (!TestEqual(TEXT("Ended once"), F.Listener->Ended.Num(), 1)) { return; }
			const FDwCombatResult& Result = F.Listener->Ended[0];
			TestEqual(TEXT("Three kills"), Result.Kills.Num(), 3);
			TestEqual(TEXT("Kill credit to crew 2's roster id"), Result.Kills[1].KillerRosterId, F.Setup.Crew[1].RosterId);
			TestEqual(TEXT("Crew results in roster order"), Result.Crew.Num(), 3);
			TestTrue(TEXT("All alive at full health"), Result.Crew[2].bAlive && Result.Crew[2].Health == 100.f);
			TestEqual(TEXT("Round 1"), Result.Rounds, 1);
			TestEqual(TEXT("Over"), F.EndTurn(), EDwCommandResult::CombatOver);
		});

		It("sinking the enemy ship kills its crew and the boarders, with no kill credit (C11)", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.Combat->MoveUnit(F.CrewHandle(2), FIntPoint(6, 0)); // crew 3 boards
			TestEqual(TEXT("Fire"), F.FireAtShip(F.CrewHandle(0), EDwSide::Enemy), EDwCommandResult::Accepted);
			TestEqual(TEXT("Ship at 0"), F.ShipHealth(EDwSide::Enemy), 0.f);
			for (int32 Index = 0; Index < 3; ++Index)
			{
				TestFalse(TEXT("Enemy went down with the ship"), F.Unit(F.EnemyHandle(Index))->IsAlive());
			}
			TestFalse(TEXT("Boarder went down too"), F.Unit(F.CrewHandle(2))->IsAlive());
			TestTrue(TEXT("Crew on own deck lives"), F.Unit(F.CrewHandle(1))->IsAlive());
			TestEqual(TEXT("VictorySunk"), F.Combat->GetOutcome(), EDwCombatOutcome::VictorySunk);
			TestEqual(TEXT("No kill credit"), F.Combat->GetResult().Kills.Num(), 0);
			TestFalse(TEXT("Boarder dead in the result"), F.Combat->GetResult().Crew[2].bAlive);
			TestEqual(TEXT("ShipSunk event"), F.Listener->Count(EDwCombatEventType::ShipSunk), 1);
		});

		It("sinking the enemy ship while the captain boards loses the run (C10/C12)", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.Combat->MoveUnit(F.CrewHandle(0), FIntPoint(5, 1));
			F.FireAtShip(F.CrewHandle(1), EDwSide::Enemy);
			TestEqual(TEXT("Captain went down with it"), F.Combat->GetOutcome(), EDwCombatOutcome::DefeatCaptainDead);
		});

		It("loses when the enemy sinks the player ship, and the rest of the enemy turn is cancelled", [this]()
		{
			ScriptEnemies([this](UDwCombatSubsystem&, FDwUnitHandle Unit) { F.FireAtShip(Unit, EDwSide::Player); });
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.EndTurn();
			TestEqual(TEXT("DefeatShipSunk"), F.Combat->GetOutcome(), EDwCombatOutcome::DefeatShipSunk);
			TestEqual(TEXT("Only the first enemy acted"), Enemy->UnitTurns, 1);
			TestEqual(TEXT("Ended once"), F.Listener->Ended.Num(), 1);
			TestEqual(TEXT("Ship health in result"), F.Combat->GetResult().PlayerShipHealth, 0.f);
		});

		It("loses when the captain dies even with crew alive (C10)", [this]()
		{
			ScriptEnemies([this](UDwCombatSubsystem&, FDwUnitHandle Unit) { F.Use(Unit, F.Execute, F.CrewHandle(0)); });
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.EndTurn();
			TestEqual(TEXT("DefeatCaptainDead"), F.Combat->GetOutcome(), EDwCombatOutcome::DefeatCaptainDead);
			TestEqual(TEXT("Enemy kill recorded with no roster id"), F.Combat->GetResult().Kills.Num(), 1);
		});
	});

	Describe("Determinism", [this]()
	{
		It("replaying the recorded commands from the same seed gives an identical result", [this]()
		{
			ScriptEnemies([this](UDwCombatSubsystem&, FDwUnitHandle Unit) { F.Use(Unit, F.Strike, F.CrewHandle(2)); });
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			for (int32 Round = 0; Round < 3 && F.Combat->GetOutcome() == EDwCombatOutcome::None; ++Round)
			{
				F.Use(F.CrewHandle(0), F.Strike, F.EnemyHandle(Round));
				F.Use(F.CrewHandle(1), F.Execute, F.EnemyHandle(Round));
				F.EndTurn();
			}
			const FDwCombatRecord Record = F.Combat->GetRecord();
			const FDwCombatResult First = F.Combat->GetResult();
			TestNotEqual(TEXT("The fight ended"), First.Outcome, EDwCombatOutcome::None);

			// Replay: nobody drives either side; every recorded command is submitted in order.
			UDwTestScriptedController* Silent = F.Root(NewObject<UDwTestScriptedController>());
			Silent->bAutoEndUnit = false;
			F.Combat->SetSideController(EDwSide::Enemy, Silent);
			F.Setup.Seed = Record.Seed;
			if (!TestTrue(TEXT("Restarted"), F.Start())) { return; }
			for (const FDwCombatCommand& Command : Record.Commands)
			{
				TestEqual(*FString::Printf(TEXT("Replayed %s"), *Command.ToString()), F.Combat->SubmitCommand(Command), EDwCommandResult::Accepted);
			}
			TestTrue(*FString::Printf(TEXT("Same result: %s vs %s"), *First.ToString(), *F.Combat->GetResult().ToString()), F.Combat->GetResult() == First);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
