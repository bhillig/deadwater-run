// Copyright Brandon Hillig 2026

// DW-105: player interaction state (select, hover, click, cancel, highlights, previews) and the camera rig.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Board/DwBoard.h"
#include "Combat/DwCombatInteraction.h"
#include "Presentation/DwCameraRig.h"
#include "Tests/Combat/DwCombatTestFixture.h"

BEGIN_DEFINE_SPEC(FDwInteractionSpec, "DeadwaterRun.Combat.Interaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	FDwCombatFixture F;
	UDwCombatInteraction* Interaction = nullptr;

	FDwUnitHandle C(int32 Index) const { return FDwCombatFixture::CrewHandle(Index); }
	FDwUnitHandle E(int32 Index) const { return F.EnemyHandle(Index); }
	int32 Lit(EDwHighlightLayer Layer) const { return F.Combat->GetBoard()->GetHighlightCount(Layer); }

	/** Action bar of the Sailor: [0] Melee, [1] Volley, [2] Cannon, [3] Potion x2, [4] Tonic x1. */
	int32 SlotOf(const UDwAbilityDefinition* Ability, FName Item = NAME_None) const
	{
		return Interaction->GetActionBar().IndexOfByPredicate([&](const FDwActionSlot& Slot) { return Slot.Ability == Ability && Slot.ItemId == Item; });
	}

	bool Begin()
	{
		if (!TestTrue(TEXT("Started"), F.Start()))
		{
			return false;
		}
		Interaction = F.Root(NewObject<UDwCombatInteraction>());
		Interaction->Initialize(*F.Combat);
		Interaction->BeginPlayerTurn();
		return true;
	}

END_DEFINE_SPEC(FDwInteractionSpec)

void FDwInteractionSpec::Define()
{
	BeforeEach([this]()
	{
		F.Create();
		F.UseRealActions();
	});
	AfterEach([this]()
	{
		if (Interaction)
		{
			Interaction->Shutdown();
			Interaction = nullptr;
		}
		F.Destroy();
	});

	Describe("Selection and movement", [this]()
	{
		It("selects the first pirate at turn start and lights where it can walk", [this]()
		{
			if (!Begin()) { return; }
			TestEqual(TEXT("Move mode"), Interaction->GetMode(), EDwInteractionMode::Move);
			TestEqual(TEXT("First crew selected"), Interaction->GetSelectedUnit(), C(0));
			TestTrue(TEXT("Reachable tiles"), Interaction->GetReachable().Num() > 0);
			TestEqual(TEXT("Move layer = reachable tiles"), Lit(EDwHighlightLayer::Move), Interaction->GetReachable().Num());
			TestEqual(TEXT("Selected tile marked"), Lit(EDwHighlightLayer::Hover), 1);
		});

		It("previews the path on hover and moves on click", [this]()
		{
			if (!Begin()) { return; }
			Interaction->HoverTile(FIntPoint(2, 0));
			TestTrue(TEXT("Path (1,0) -> (2,0)"), Interaction->GetPath() == TArray<FIntPoint>{ FIntPoint(1, 0), FIntPoint(2, 0) });
			TestEqual(TEXT("Path layer"), Lit(EDwHighlightLayer::Path), 2);
			Interaction->ClickTile(FIntPoint(2, 0));
			TestEqual(TEXT("Moved"), F.Unit(C(0))->GetTile(), FIntPoint(2, 0));
			TestEqual(TEXT("Still selected"), Interaction->GetSelectedUnit(), C(0));
			TestEqual(TEXT("No move left: nothing to light"), Lit(EDwHighlightLayer::Move), 0);
		});

		It("selects an own pirate on click and explains a refused click", [this]()
		{
			if (!Begin()) { return; }
			Interaction->ClickTile(FIntPoint(0, 1));
			TestEqual(TEXT("Crew 2 selected"), Interaction->GetSelectedUnit(), C(1));
			Interaction->ClickTile(FIntPoint(9, 1)); // an enemy far beyond the melee range
			TestFalse(TEXT("Told why"), Interaction->GetMessage().IsEmpty());
			TestEqual(TEXT("Nothing happened"), F.Health(E(1)), 100.f);
			TestEqual(TEXT("Nothing recorded"), F.Combat->GetRecord().Commands.Num(), 0);
		});

		It("attacks an enemy in reach with the basic attack, previewed on hover", [this]()
		{
			if (!Begin()) { return; }
			F.Combat->MoveUnit(C(0), FIntPoint(8, 0), /*bSnapActor*/ true);
			Interaction->Refresh();
			TestEqual(TEXT("Attackable enemy lit"), Lit(EDwHighlightLayer::Target), 1);
			Interaction->HoverTile(FIntPoint(9, 0));
			TestTrue(TEXT("Preview 25"), Interaction->GetPreview().bValid && Interaction->GetPreview().Targets[0].Damage == 25);
			Interaction->ClickTile(FIntPoint(9, 0));
			TestEqual(TEXT("Hit for 25"), F.Health(E(0)), 75.f);
		});

		It("cycles to the next pirate that can still act", [this]()
		{
			if (!Begin()) { return; }
			Interaction->SelectNextUnit();
			TestEqual(TEXT("Crew 2"), Interaction->GetSelectedUnit(), C(1));
			Interaction->SelectNextUnit();
			Interaction->SelectNextUnit();
			TestEqual(TEXT("Wraps to crew 1"), Interaction->GetSelectedUnit(), C(0));
		});
	});

	Describe("Abilities", [this]()
	{
		It("arms an ability, lights its targets, previews on hover and fires on click", [this]()
		{
			if (!Begin()) { return; }
			TestTrue(TEXT("Volley armed"), Interaction->SelectSlot(SlotOf(F.Volley)));
			TestEqual(TEXT("Targeting"), Interaction->GetMode(), EDwInteractionMode::Targeting);
			TestEqual(TEXT("Three enemies"), Interaction->GetValidTargets().Num(), 3);
			TestEqual(TEXT("Group: area layer"), Lit(EDwHighlightLayer::AreaOfEffect), 3);
			Interaction->HoverTile(FIntPoint(9, 0));
			TestTrue(TEXT("3-5 hits previewed"), Interaction->GetPreview().bValid && Interaction->GetPreview().MinHits == 3 && Interaction->GetPreview().MaxHits == 5);
			Interaction->ClickTile(FIntPoint(9, 0));
			TestEqual(TEXT("Fired"), F.Combat->GetRecord().Commands.Num(), 1);
			TestEqual(TEXT("Back to moving"), Interaction->GetMode(), EDwInteractionMode::Move);
			TestNull(TEXT("Disarmed"), Interaction->GetArmedAbility());
		});

		It("refuses to arm what can't be used, with the reason", [this]()
		{
			if (!Begin()) { return; }
			TestFalse(TEXT("Melee: nobody in reach"), Interaction->SelectSlot(SlotOf(F.Melee)));
			TestFalse(TEXT("Has a reason"), Interaction->GetMessage().IsEmpty());
			TestFalse(TEXT("Cannon: none in reach"), Interaction->SelectSlot(SlotOf(F.Cannon)));
			TestTrue(TEXT("Cannon reason"), Interaction->GetMessage().EqualTo(UDwGA_FireCannon::GetNoCannonReason()));
			TestEqual(TEXT("Still moving"), Interaction->GetMode(), EDwInteractionMode::Move);
		});

		It("aims a cannon at the enemy ship by clicking any tile of its deck", [this]()
		{
			if (!Begin()) { return; }
			F.Combat->MoveUnit(C(0), FIntPoint(2, 0), /*bSnapActor*/ true);
			TestTrue(TEXT("Cannon armed"), Interaction->SelectSlot(SlotOf(F.Cannon)));
			TestEqual(TEXT("Whole enemy deck lit"), Lit(EDwHighlightLayer::AreaOfEffect), 15);
			Interaction->ClickTile(FIntPoint(6, 2));
			TestEqual(TEXT("Ship hit"), F.ShipHealth(EDwSide::Enemy), 180.f);
		});

		It("uses an item on an ally from the action bar", [this]()
		{
			F.Setup.Crew[1].Health = 40.f;
			if (!Begin()) { return; }
			const TArray<FDwActionSlot> Bar = Interaction->GetActionBar();
			const int32 Potion = SlotOf(F.Item, F.Potion->GetFName());
			if (!TestTrue(TEXT("Potion slot"), Bar.IsValidIndex(Potion))) { return; }
			TestEqual(TEXT("Count shown"), Bar[Potion].Count, 2);
			TestTrue(TEXT("Armed"), Interaction->SelectSlot(Potion));
			Interaction->ClickTile(FIntPoint(0, 1)); // crew 2: a target here, not a selection
			TestEqual(TEXT("Healed"), F.Health(C(1)), 70.f);
			TestEqual(TEXT("Crew 1 still selected"), Interaction->GetSelectedUnit(), C(0));
			TestEqual(TEXT("One left"), Interaction->GetActionBar()[SlotOf(F.Item, F.Potion->GetFName())].Count, 1);
		});

		It("greys out actions once the action is spent", [this]()
		{
			if (!Begin()) { return; }
			Interaction->SelectSlot(SlotOf(F.Volley));
			Interaction->ClickTile(FIntPoint(9, 0));
			const FDwActionSlot Volley = Interaction->GetActionBar()[SlotOf(F.Volley)];
			TestFalse(TEXT("Unavailable"), Volley.bAvailable);
			TestFalse(TEXT("With a reason"), Volley.Reason.IsEmpty());
		});

		It("cancels targeting, then the selection", [this]()
		{
			if (!Begin()) { return; }
			Interaction->SelectSlot(SlotOf(F.Volley));
			TestTrue(TEXT("Cancel targeting"), Interaction->Cancel());
			TestEqual(TEXT("Moving again"), Interaction->GetMode(), EDwInteractionMode::Move);
			TestEqual(TEXT("Targets cleared"), Lit(EDwHighlightLayer::AreaOfEffect), 0);
			TestTrue(TEXT("Cancel selection"), Interaction->Cancel());
			TestEqual(TEXT("Idle"), Interaction->GetMode(), EDwInteractionMode::Idle);
			TestEqual(TEXT("Nothing lit"), Lit(EDwHighlightLayer::Move), 0);
			TestFalse(TEXT("Nothing left to cancel"), Interaction->Cancel());
		});
	});

	Describe("Turn flow", [this]()
	{
		It("locks clicks while an action plays", [this]()
		{
			F.World->GetSubsystem<UDwPresentationSubsystem>()->SetInstant(false);
			if (!Begin()) { return; }
			Interaction->ClickTile(FIntPoint(2, 0));
			TestTrue(TEXT("Walking"), F.Combat->IsResolving());
			TestFalse(TEXT("Can't act"), Interaction->CanAct());
			Interaction->ClickTile(FIntPoint(0, 1));
			TestEqual(TEXT("Selection unchanged"), Interaction->GetSelectedUnit(), C(0));
			TestFalse(TEXT("Told to wait"), Interaction->GetMessage().IsEmpty());
			TestEqual(TEXT("No highlights while busy"), Lit(EDwHighlightLayer::Move), 0);
		});

		It("ends the turn on request and flags the End Turn suggestion", [this]()
		{
			if (!Begin()) { return; }
			for (int32 Index = 0; Index < 3; ++Index)
			{
				Interaction->SelectUnit(C(Index));
				Interaction->SelectSlot(SlotOf(F.Volley));
				Interaction->ClickTile(F.Unit(E(0))->GetTile());
				F.Asc(C(Index))->SetNumericAttributeBase(UDwUnitAttributeSet::GetMovePointsAttribute(), 0.f);
			}
			Interaction->Refresh();
			F.Combat->SubmitCommand(FDwCombatCommand::MakeEndUnit(C(0))); // any resolved command re-checks the suggestion
			TestTrue(TEXT("Suggested"), Interaction->IsEndTurnSuggested());
			Interaction->RequestEndTurn();
			TestEqual(TEXT("Round 2"), F.Combat->GetRound(), 2);
			TestEqual(TEXT("Player again"), F.Combat->GetActiveSide(), EDwSide::Player);
		});
	});

	Describe("Camera rig", [this]()
	{
		It("frames the board centre, clamps pan to the board and zoom to its limits", [this]()
		{
			if (!Begin()) { return; }
			ADwCameraRig* Rig = F.World->SpawnActor<ADwCameraRig>();
			Rig->FrameBoard(F.Combat->GetBoard());
			const ADwBoard* Board = F.Combat->GetBoard();
			// The framed view looks FramingOffset past the board's centre, so the board clears the HUD's unit cards.
			const FVector Centre = (Board->TileToWorld(FIntPoint(0, 0)) + Board->TileToWorld(FIntPoint(9, 2))) * 0.5f + FVector(Rig->FramingOffset, 0.f);
			TestTrue(TEXT("Focus on the board centre"), Rig->GetFocus().Equals(Centre, 1.f));
			TestTrue(TEXT("Looks down at the board from the +Y side"), Rig->GetActorLocation().Z > Centre.Z && Rig->GetActorLocation().Y > Centre.Y);
			Rig->Pan(FVector2D(100.f, 100.f), 10.f);
			TestTrue(TEXT("Pan clamped to half the board"), FMath::Abs(Rig->GetFocus().X - Centre.X) <= 500.f + 1.f && FMath::Abs(Rig->GetFocus().Y - Centre.Y) <= 150.f + 1.f);
			Rig->Zoom(100.f);
			TestEqual(TEXT("Zoom-in limit"), Rig->GetZoom(), Rig->MinZoom);
			Rig->Zoom(-100.f);
			TestEqual(TEXT("Zoom-out limit"), Rig->GetZoom(), Rig->MaxZoom);
			Rig->Destroy();
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
