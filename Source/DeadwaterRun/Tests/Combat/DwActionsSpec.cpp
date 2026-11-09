// Copyright Brandon Hillig 2026

// DW-104: core actions in a fight (Docs/Architecture/02 "How it's tested", 06 cannons) and the pure cannon/query rules.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Board/DwBoard.h"
#include "Core/DwGameData.h"
#include "Combat/DwCombatRules.h"
#include "Tests/Combat/DwCombatTestFixture.h"

namespace DwActionsSpec
{
	static FDwAbilityTarget Ship(EDwSide Side) { return FDwAbilityTarget::MakeShip(Side); }
}

BEGIN_DEFINE_SPEC(FDwActionsSpec, "DeadwaterRun.Combat.Actions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	FDwCombatFixture F;

	FDwUnitHandle C(int32 Index) const { return FDwCombatFixture::CrewHandle(Index); }
	FDwUnitHandle E(int32 Index) const { return F.EnemyHandle(Index); }
	float MovePoints(FDwUnitHandle Unit) const { return F.Attr(Unit, UDwUnitAttributeSet::GetMovePointsAttribute()); }
	float Actions(FDwUnitHandle Unit) const { return F.Attr(Unit, UDwUnitAttributeSet::GetActionPointsAttribute()); }
	EDwCommandResult Fire(FDwUnitHandle Unit, EDwSide At = EDwSide::Enemy)
	{
		return F.Combat->SubmitCommand(FDwCombatCommand::MakeAbility(Unit, DwTags::Ability_Cannon, DwActionsSpec::Ship(At)));
	}
	EDwCommandResult Shoot(FDwUnitHandle Unit, FDwUnitHandle Target) { return F.Use(Unit, F.Volley, Target); }

	/** Ticks every ASC (ability tasks) until nothing resolves, at most Seconds of game time. */
	void RunPresentation(float Seconds)
	{
		for (float Time = 0.f; Time < Seconds && F.Combat->IsResolving(); Time += 0.05f)
		{
			for (ADwCombatUnit* Unit : F.Combat->GetUnits())
			{
				Unit->GetDwAbilitySystem()->TickComponent(0.05f, LEVELTICK_All, nullptr);
			}
		}
	}

END_DEFINE_SPEC(FDwActionsSpec)

void FDwActionsSpec::Define()
{
	BeforeEach([this]()
	{
		F.Create();
		F.UseRealActions();
	});
	AfterEach([this]() { F.Destroy(); });

	Describe("Move", [this]()
	{
		It("walks a path, pays for it and loses the rest of the move (C3)", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			TestEqual(TEXT("Move accepted"), F.MoveTo(C(0), FIntPoint(2, 0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("Unit tile"), F.Unit(C(0))->GetTile(), FIntPoint(2, 0));
			TestEqual(TEXT("Grid occupant"), F.Combat->GetGrid().GetOccupant(FIntPoint(2, 0)).Unit, C(0));
			TestFalse(TEXT("Old tile free"), F.Combat->GetGrid().IsOccupied(FIntPoint(0, 0)));
			TestEqual(TEXT("Cost 2 of 5, but no move left"), MovePoints(C(0)), 0.f);
			TestEqual(TEXT("Action untouched"), Actions(C(0)), 1.f);
			TestTrue(TEXT("Actor snapped to the tile (instant)"), F.Unit(C(0))->GetActorLocation().Equals(F.Combat->GetBoard()->TileToWorld(FIntPoint(2, 0))));
			TestEqual(TEXT("UnitMoved event"), F.Listener->Count(EDwCombatEventType::UnitMoved, C(0)), 1);
		});

		It("allows move-then-act and act-then-move, never a second move", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			TestEqual(TEXT("Move"), F.MoveTo(C(0), FIntPoint(1, 0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("Second move"), F.MoveTo(C(0), FIntPoint(2, 0)), EDwCommandResult::CannotAfford);
			TestEqual(TEXT("Then act"), Shoot(C(0), E(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("Act first"), Shoot(C(1), E(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("Then move"), F.MoveTo(C(1), FIntPoint(1, 1)), EDwCommandResult::Accepted);
		});

		It("refuses unreachable, occupied and rooted moves", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			TestEqual(TEXT("6 steps away"), F.MoveTo(C(0), FIntPoint(4, 2)), EDwCommandResult::InvalidTarget);
			TestEqual(TEXT("Onto an ally"), F.MoveTo(C(0), FIntPoint(0, 1)), EDwCommandResult::InvalidTarget);
			TestEqual(TEXT("Onto water / off the board"), F.MoveTo(C(0), FIntPoint(-1, 0)), EDwCommandResult::InvalidTarget);
			F.ApplyEffect(DwGas::GetSharedEffects().Rooted, C(2), E(0));
			TestEqual(TEXT("Rooted"), F.MoveTo(C(2), FIntPoint(1, 2)), EDwCommandResult::Blocked);
			TestEqual(TEXT("Rooted can still act"), Shoot(C(2), E(0)), EDwCommandResult::Accepted);
		});

		It("boards the enemy ship across the boarding edge", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			TestEqual(TEXT("5 steps to the enemy deck"), F.MoveTo(C(1), FIntPoint(5, 1)), EDwCommandResult::Accepted);
			TestTrue(TEXT("On the enemy deck"), F.Asc(C(1))->HasMatchingGameplayTag(DwTags::State_OnEnemyDeck));
		});

		It("lets a single-choice unit act or move, not both (Conqueror crew)", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.Asc(C(1))->AddLooseGameplayTag(DwTags::Rule_Turn_SingleChoice);
			TestEqual(TEXT("Act"), Shoot(C(1), E(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("No move after acting"), F.MoveTo(C(1), FIntPoint(1, 1)), EDwCommandResult::CannotAfford);
		});

		It("walks over time outside instant mode, with the grid updated at once and input locked until arrival", [this]()
		{
			F.World->GetSubsystem<UDwPresentationSubsystem>()->SetInstant(false);
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			const FVector Start = F.Unit(C(0))->GetActorLocation();
			TestEqual(TEXT("Accepted"), F.MoveTo(C(0), FIntPoint(3, 0)), EDwCommandResult::Accepted);
			TestTrue(TEXT("Resolving while walking"), F.Combat->IsResolving());
			TestEqual(TEXT("Logic moved at commit"), F.Combat->GetGrid().GetOccupant(FIntPoint(3, 0)).Unit, C(0));
			TestTrue(TEXT("Actor hasn't moved yet"), F.Unit(C(0))->GetActorLocation().Equals(Start));
			TestEqual(TEXT("Input locked"), Shoot(C(1), E(0)), EDwCommandResult::Busy);
			RunPresentation(5.f);
			TestFalse(TEXT("Arrived"), F.Combat->IsResolving());
			TestTrue(TEXT("At the tile"), F.Unit(C(0))->GetActorLocation().Equals(F.Combat->GetBoard()->TileToWorld(FIntPoint(3, 0)), 1.f));
		});
	});

	Describe("Cannon", [this]()
	{
		It("fires from next to an own cannon: the enemy ship takes 70 and the action is spent", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.Combat->MoveUnit(C(0), FIntPoint(2, 0)); // next to the cannon at (2,1)
			TestEqual(TEXT("Fire"), Fire(C(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("250 - 70"), F.ShipHealth(EDwSide::Enemy), 180.f);
			TestEqual(TEXT("Own ship untouched"), F.ShipHealth(EDwSide::Player), 250.f);
			TestEqual(TEXT("Action spent"), Actions(C(0)), 0.f);
			TestTrue(TEXT("Cannon marked used"), F.Combat->GetSnapshot().CannonsUsed.Contains(FIntPoint(2, 1)));
			TestEqual(TEXT("ShipDamaged event"), F.Listener->Count(EDwCombatEventType::ShipDamaged), 1);
		});

		It("fires from the cannon tile itself", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.Combat->MoveUnit(C(0), FIntPoint(2, 1));
			TestEqual(TEXT("Fire"), Fire(C(0)), EDwCommandResult::Accepted);
		});

		It("refuses a diagonal cannon, the enemy's cannon and a cannon already fired this turn", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.Combat->MoveUnit(C(0), FIntPoint(1, 0)); // diagonal to (2,1)
			TestEqual(TEXT("Diagonal"), Fire(C(0)), EDwCommandResult::InvalidTarget);
			F.Combat->MoveUnit(C(1), FIntPoint(6, 1)); // next to the enemy cannon (7,1)
			TestEqual(TEXT("Enemy cannon"), Fire(C(1)), EDwCommandResult::InvalidTarget);

			F.Combat->MoveUnit(C(0), FIntPoint(2, 0));
			F.Combat->MoveUnit(C(2), FIntPoint(2, 2));
			TestEqual(TEXT("First gunner"), Fire(C(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("Same cannon again"), Fire(C(2)), EDwCommandResult::InvalidTarget);
			F.EndTurn();
			TestEqual(TEXT("Reloaded next turn"), Fire(C(2)), EDwCommandResult::Accepted);
			TestEqual(TEXT("Two hits"), F.ShipHealth(EDwSide::Enemy), 110.f);
		});

		It("never hits units, and attacks never hit ships (C8)", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.Combat->MoveUnit(C(0), FIntPoint(2, 0));
			TestEqual(TEXT("Cannon at a unit"), F.Combat->SubmitCommand(FDwCombatCommand::MakeAbility(C(0), DwTags::Ability_Cannon,
				FDwAbilityTarget::MakeUnit(E(0), FIntPoint(9, 0)))), EDwCommandResult::InvalidTarget);
			F.Combat->MoveUnit(C(1), FIntPoint(8, 1));
			TestEqual(TEXT("Melee at the ship"), F.Combat->SubmitCommand(FDwCombatCommand::MakeAbility(C(1), DwTags::Ability_Attack_Melee,
				DwActionsSpec::Ship(EDwSide::Enemy))), EDwCommandResult::InvalidTarget);
			F.Combat->SubmitCommand(FDwCombatCommand::MakeAbility(C(2), DwTags::Ability_Attack_Ranged, DwActionsSpec::Ship(EDwSide::Enemy)));
			TestEqual(TEXT("A volley aimed at the ship hits crew, not hull"), F.ShipHealth(EDwSide::Enemy), 250.f);
		});

		It("previews exactly the damage it deals", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.Combat->MoveUnit(C(0), FIntPoint(2, 0));
			const FDwCombatSnapshot Snap = F.Combat->GetSnapshot();
			const FDwAbilityPreview Preview = FDwCombatQueries::GetAbilityCDO(F.Cannon)->PreviewOutcome(F.Context(Snap, C(0), F.Cannon), DwActionsSpec::Ship(EDwSide::Enemy));
			if (!TestTrue(TEXT("Valid preview"), Preview.bValid && Preview.Targets.Num() == 1)) { return; }
			Fire(C(0));
			TestEqual(TEXT("Preview == result"), static_cast<float>(Preview.Targets[0].Damage), 250.f - F.ShipHealth(EDwSide::Enemy));
		});

		It("explains why it can't fire", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			const FDwCombatSnapshot Snap = F.Combat->GetSnapshot();
			const FDwAbilityPreview Preview = FDwCombatQueries::GetAbilityCDO(F.Cannon)->PreviewOutcome(F.Context(Snap, C(0), F.Cannon), DwActionsSpec::Ship(EDwSide::Enemy));
			TestFalse(TEXT("Invalid"), Preview.bValid);
			TestTrue(TEXT("Reason names the cannon"), Preview.Reason.EqualTo(UDwGA_FireCannon::GetNoCannonReason()));
		});

		It("lets enemies fire their own cannons at the player ship", [this]()
		{
			UDwTestScriptedController* Script = F.Root(NewObject<UDwTestScriptedController>());
			Script->OnUnitTurn = [this](UDwCombatSubsystem& Combat, FDwUnitHandle Unit)
			{
				if (Unit == E(0))
				{
					Combat.MoveUnit(Unit, FIntPoint(7, 0)); // next to (7,1)
					TestEqual(TEXT("Enemy fires"), Fire(Unit, EDwSide::Player), EDwCommandResult::Accepted);
				}
			};
			F.Combat->SetSideController(EDwSide::Enemy, Script);
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.EndTurn();
			TestEqual(TEXT("Player ship hit"), F.ShipHealth(EDwSide::Player), 180.f);
		});
	});

	Describe("Payload", [this]()
	{
		It("hits an adjacent enemy for its tuning damage, as previewed", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.Combat->MoveUnit(C(0), FIntPoint(8, 0)); // next to enemy 1 at (9,0)
			const FDwCombatSnapshot Snap = F.Combat->GetSnapshot();
			const FDwAbilityPreview Preview = FDwCombatQueries::GetAbilityCDO(F.Melee)->PreviewOutcome(F.Context(Snap, C(0), F.Melee),
				FDwAbilityTarget::MakeUnit(E(0), FIntPoint(9, 0)));
			TestEqual(TEXT("Melee"), F.Use(C(0), F.Melee, E(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("25 damage"), F.Health(E(0)), 75.f);
			TestTrue(TEXT("Preview == result"), Preview.bValid && Preview.Targets.Num() == 1 && Preview.Targets[0].Damage == 25);
			TestEqual(TEXT("Out of melee range"), F.Use(C(0), F.Melee, E(1)), EDwCommandResult::CannotAfford); // action already spent
			TestEqual(TEXT("Range check"), F.Use(C(1), F.Melee, E(1)), EDwCommandResult::InvalidTarget);
		});

		It("draws random hits from the combat stream: same seed, same hits", [this]()
		{
			auto Play = [this]()
			{
				TArray<float> Health;
				if (F.Start() && Shoot(C(0), E(0)) == EDwCommandResult::Accepted)
				{
					for (int32 Index = 0; Index < 3; ++Index) { Health.Add(F.Health(E(Index))); }
				}
				return Health;
			};
			const TArray<float> First = Play();
			const TArray<float> Second = Play();
			if (!TestEqual(TEXT("Both fights ran"), First.Num() + Second.Num(), 6)) { return; }
			TestTrue(TEXT("Identical hits"), First == Second);
			const float Total = 300.f - (First[0] + First[1] + First[2]);
			TestTrue(*FString::Printf(TEXT("3-5 hits of 10 (dealt %.0f)"), Total), Total >= 30.f && Total <= 50.f);
		});

		It("applies its effects in order: damage only the already afraid, then fear for all", [this]()
		{
			F.Setup.Crew[0].ExtraAbilities = { F.Terror };
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.Asc(E(0))->SetNumericAttributeBase(UDwUnitAttributeSet::GetFearAttribute(), 1.f);
			TestEqual(TEXT("Terror"), F.Use(C(0), F.Terror, E(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("Afraid enemy hurt"), F.Health(E(0)), 85.f);
			TestEqual(TEXT("Calm enemy unhurt"), F.Health(E(1)), 100.f);
			TestEqual(TEXT("Fear 1 -> 2"), F.Attr(E(0), UDwUnitAttributeSet::GetFearAttribute()), 2.f);
			TestEqual(TEXT("Fear 0 -> 1"), F.Attr(E(1), UDwUnitAttributeSet::GetFearAttribute()), 1.f);
		});

		It("lands its hit at the impact beat outside instant mode", [this]()
		{
			F.World->GetSubsystem<UDwPresentationSubsystem>()->SetInstant(false);
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.Combat->MoveUnit(C(0), FIntPoint(8, 0));
			TestEqual(TEXT("Accepted"), F.Use(C(0), F.Melee, E(0)), EDwCommandResult::Accepted);
			TestTrue(TEXT("Resolving"), F.Combat->IsResolving());
			TestEqual(TEXT("Cost paid at commit"), Actions(C(0)), 0.f);
			TestEqual(TEXT("No damage before impact"), F.Health(E(0)), 100.f);
			F.Asc(C(0))->TickComponent(0.3f, LEVELTICK_All, nullptr); // impact is halfway through the 0.5 s placeholder beat
			TestEqual(TEXT("Damage at impact"), F.Health(E(0)), 75.f);
			TestTrue(TEXT("Still resolving after impact"), F.Combat->IsResolving());
			RunPresentation(1.f);
			TestFalse(TEXT("Done"), F.Combat->IsResolving());
		});
	});

	Describe("Item", [this]()
	{
		It("heals an adjacent ally, as previewed, and spends one item and the action", [this]()
		{
			F.Setup.Crew[1].Health = 50.f;
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			const FDwCombatSnapshot Snap = F.Combat->GetSnapshot();
			const FDwAbilityPreview Preview = FDwCombatQueries::GetAbilityCDO(F.Item)->PreviewOutcome(
				F.Context(Snap, C(0), F.Item, F.Potion->GetFName()), FDwAbilityTarget::MakeUnit(C(1), FIntPoint(0, 1)));
			TestEqual(TEXT("Potion"), F.UseItem(C(0), F.Potion, C(1)), EDwCommandResult::Accepted);
			TestEqual(TEXT("50 + 30"), F.Health(C(1)), 80.f);
			TestTrue(TEXT("Preview heal 30"), Preview.bValid && Preview.Targets.Num() == 1 && Preview.Targets[0].Heal == 30);
			TestEqual(TEXT("One potion left"), F.Combat->GetSnapshot().Items.FindRef(F.Potion->GetFName()), 1);
			TestEqual(TEXT("Action spent"), Shoot(C(0), E(0)), EDwCommandResult::CannotAfford);
		});

		It("never heals past max, and previews the capped amount", [this]()
		{
			F.Setup.Crew[1].Health = 90.f;
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			const FDwCombatSnapshot Snap = F.Combat->GetSnapshot();
			const FDwAbilityPreview Preview = FDwCombatQueries::GetAbilityCDO(F.Item)->PreviewOutcome(
				F.Context(Snap, C(0), F.Item, F.Potion->GetFName()), FDwAbilityTarget::MakeUnit(C(1), FIntPoint(0, 1)));
			F.UseItem(C(0), F.Potion, C(1));
			TestEqual(TEXT("Capped"), F.Health(C(1)), 100.f);
			TestTrue(TEXT("Preview 10"), Preview.Targets.Num() == 1 && Preview.Targets[0].Heal == 10);
		});

		It("restores mana", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.Asc(C(0))->SetNumericAttributeBase(UDwUnitAttributeSet::GetManaAttribute(), 5.f);
			TestEqual(TEXT("Tonic on self"), F.UseItem(C(0), F.Tonic, C(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("5 + 10"), F.Attr(C(0), UDwUnitAttributeSet::GetManaAttribute()), 15.f);
		});

		It("refuses an ally out of reach, an enemy and an empty stack", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			TestEqual(TEXT("Two tiles away"), F.UseItem(C(0), F.Potion, C(2)), EDwCommandResult::InvalidTarget);
			TestEqual(TEXT("Enemy"), F.UseItem(C(0), F.Potion, E(0)), EDwCommandResult::InvalidTarget);
			TestEqual(TEXT("Only tonic"), F.UseItem(C(0), F.Tonic, C(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("None left"), F.UseItem(C(1), F.Tonic, C(1)), EDwCommandResult::NoItem);
			TestEqual(TEXT("No item named"), F.Combat->SubmitCommand(FDwCombatCommand::MakeAbility(C(1), DwTags::Ability_Item,
				FDwAbilityTarget::MakeUnit(C(1), FIntPoint(0, 1)))), EDwCommandResult::NoItem);
		});

		It("reports the items used in the result", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.UseItem(C(0), F.Potion, C(0));
			F.Combat->MoveUnit(C(1), FIntPoint(2, 1));
			Fire(C(1));
			F.EndTurn();
			F.Combat->MoveUnit(C(2), FIntPoint(2, 0));
			for (int32 Round = 0; Round < 3 && F.Combat->GetOutcome() == EDwCombatOutcome::None; ++Round)
			{
				Fire(Round % 2 == 0 ? C(2) : C(1));
				F.EndTurn();
			}
			TestEqual(TEXT("Sunk"), F.Combat->GetOutcome(), EDwCombatOutcome::VictorySunk);
			TestEqual(TEXT("One potion used"), F.Combat->GetResult().ItemsUsed.FindRef(F.Potion->GetFName()), 1);
		});
	});

	Describe("Replay", [this]()
	{
		It("replays moves, random volleys, cannons and items to an identical result", [this]()
		{
			UDwTestScriptedController* Script = F.Root(NewObject<UDwTestScriptedController>());
			Script->OnUnitTurn = [this](UDwCombatSubsystem&, FDwUnitHandle Unit) { Shoot(Unit, C(2)); };
			F.Combat->SetSideController(EDwSide::Enemy, Script);
			if (!TestTrue(TEXT("Started"), F.Start())) { return; }
			F.MoveTo(C(0), FIntPoint(2, 0));
			Fire(C(0));
			F.MoveTo(C(1), FIntPoint(1, 1));
			Shoot(C(1), E(0));
			F.UseItem(C(2), F.Potion, C(2));
			for (int32 Round = 0; Round < 12 && F.Combat->GetOutcome() == EDwCombatOutcome::None; ++Round)
			{
				F.EndTurn();
				Fire(C(0));
				Shoot(C(1), E(1));
				Shoot(C(2), E(2));
			}
			const FDwCombatRecord Record = F.Combat->GetRecord();
			const FDwCombatResult First = F.Combat->GetResult();
			if (!TestNotEqual(TEXT("The fight ended"), First.Outcome, EDwCombatOutcome::None)) { return; }

			UDwTestScriptedController* Silent = F.Root(NewObject<UDwTestScriptedController>());
			Silent->bAutoEndUnit = false;
			F.Combat->SetSideController(EDwSide::Enemy, Silent);
			if (!TestTrue(TEXT("Restarted"), F.Start())) { return; }
			for (const FDwCombatCommand& Command : Record.Commands)
			{
				TestEqual(*FString::Printf(TEXT("Replayed %s"), *Command.ToString()), F.Combat->SubmitCommand(Command), EDwCommandResult::Accepted);
			}
			TestTrue(*FString::Printf(TEXT("Same result: %s vs %s"), *First.ToString(), *F.Combat->GetResult().ToString()), F.Combat->GetResult() == First);
		});
	});
}

// ---------------------------------------------------------------------------------------------------------------------
// Pure rules

BEGIN_DEFINE_SPEC(FDwCannonRulesSpec, "DeadwaterRun.Rules.Cannon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDwCannonRulesSpec)

void FDwCannonRulesSpec::Define()
{
	// Two player cannons (2,0) and (2,2); the enemy deck mirrors them to (7,0) and (7,2).
	const FDwGridModel Grid = FDwBoardBuilder::BuildShipBattle(
		FDwDeckAscii::Parse({ TEXT("S.C.E"), TEXT("S...E"), TEXT("S.C.E") }),
		FDwDeckAscii::Parse({ TEXT("S.C.E"), TEXT("S...E"), TEXT("S.C.E") }));

	It("finds an own cannon on the unit's tile or 4-way next to it, not diagonally", [this, Grid]()
	{
		TestTrue(TEXT("On it"), FDwCannonRules::FindUsableCannon(Grid, FIntPoint(2, 0), EDwSide::Player, {}) == TOptional<FIntPoint>(FIntPoint(2, 0)));
		TestTrue(TEXT("Next to it"), FDwCannonRules::FindUsableCannon(Grid, FIntPoint(1, 0), EDwSide::Player, {}) == TOptional<FIntPoint>(FIntPoint(2, 0)));
		TestFalse(TEXT("Diagonal"), FDwCannonRules::FindUsableCannon(Grid, FIntPoint(3, 1), EDwSide::Player, {}).IsSet());
	});

	It("never uses the other side's cannon or one already fired", [this, Grid]()
	{
		TestFalse(TEXT("Enemy cannon"), FDwCannonRules::FindUsableCannon(Grid, FIntPoint(7, 1), EDwSide::Player, {}).IsSet());
		TestTrue(TEXT("Enemy uses its own"), FDwCannonRules::FindUsableCannon(Grid, FIntPoint(7, 1), EDwSide::Enemy, {}).IsSet());
		TestFalse(TEXT("Used"), FDwCannonRules::FindUsableCannon(Grid, FIntPoint(1, 0), EDwSide::Player, { FIntPoint(2, 0) }).IsSet());
	});

	It("prefers the unit's own tile, then the lowest board index", [this, Grid]()
	{
		TestTrue(TEXT("Between two: lowest index"), FDwCannonRules::FindUsableCannon(Grid, FIntPoint(2, 1), EDwSide::Player, {}) == TOptional<FIntPoint>(FIntPoint(2, 0)));
		TestTrue(TEXT("First used: the other"), FDwCannonRules::FindUsableCannon(Grid, FIntPoint(2, 1), EDwSide::Player, { FIntPoint(2, 0) }) == TOptional<FIntPoint>(FIntPoint(2, 2)));
	});
}

BEGIN_DEFINE_SPEC(FDwQueriesSpec, "DeadwaterRun.Rules.Queries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FDwCombatFixture F;

	/** A hand-built snapshot: one player unit at (0,0) with melee (range 1) and Move, one enemy far away at (9,0). */
	FDwCombatSnapshot Snapshot(float ActionPoints, float MovePoints) const
	{
		FDwCombatSnapshot S;
		S.Grid = FDwBoardBuilder::BuildShipBattle(FDwDeckAscii::Parse(FDwCombatFixture::DefaultDeck()), FDwDeckAscii::Parse(FDwCombatFixture::DefaultDeck()));
		auto Add = [&S](int32 Id, EDwSide Side, FIntPoint Tile)
		{
			FDwUnitSnapshot& U = S.Units.AddDefaulted_GetRef();
			U.Handle = FDwUnitHandle(Id);
			U.Side = Side;
			U.Tile = Tile;
			U.Health = U.MaxHealth = 100.f;
			S.Grid.SetOccupant(Tile, U.Handle, Side);
			return &U;
		};
		FDwUnitSnapshot* Player = Add(1, EDwSide::Player, FIntPoint(0, 0));
		Player->Abilities = { F.Melee, F.Move };
		Player->ActionPoints = ActionPoints;
		Player->MovePoints = MovePoints;
		Add(2, EDwSide::Enemy, FIntPoint(9, 0))->Abilities = { F.Melee };
		return S;
	}
END_DEFINE_SPEC(FDwQueriesSpec)

void FDwQueriesSpec::Define()
{
	BeforeEach([this]()
	{
		F.Create();
		F.UseRealActions();
	});
	AfterEach([this]() { F.Destroy(); });

	It("is false when every unit has spent its move and action", [this]()
	{
		TestFalse(TEXT("Spent"), FDwCombatQueries::SideHasAvailableAction(Snapshot(0.f, 0.f), EDwSide::Player));
	});

	It("is false when the only action left has no valid target", [this]()
	{
		TestFalse(TEXT("Melee, nobody in reach, no move"), FDwCombatQueries::SideHasAvailableAction(Snapshot(1.f, 0.f), EDwSide::Player));
	});

	It("is true while a unit still has a reachable tile", [this]()
	{
		TestTrue(TEXT("Can still move"), FDwCombatQueries::SideHasAvailableAction(Snapshot(0.f, 5.f), EDwSide::Player));
	});

	It("ignores incapacitated and dead units", [this]()
	{
		FDwCombatSnapshot S = Snapshot(1.f, 5.f);
		S.Units[0].Tags.AddTag(DwTags::Status_Incapacitated_Frozen);
		TestFalse(TEXT("Frozen"), FDwCombatQueries::SideHasAvailableAction(S, EDwSide::Player));
		S = Snapshot(1.f, 5.f);
		S.Units[0].bAlive = false;
		TestFalse(TEXT("Dead"), FDwCombatQueries::SideHasAvailableAction(S, EDwSide::Player));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
