// Copyright Brandon Hillig 2026

// DW-107: combat HUD view models and floating text (Docs/Architecture/14-UI.md "How it's tested"). No widgets except the
// floating text pool, which needs WBP_FloatingText (Tools/CreateUIContent.ps1).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/DwGameplayAbility.h"
#include "Combat/DwCombatInteraction.h"
#include "Core/DwGameData.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Tests/Combat/DwCombatTestFixture.h"
#include "Tests/UI/DwUITestListener.h"
#include "UI/DwCombatViewModels.h"
#include "UI/DwFloatingText.h"

BEGIN_DEFINE_SPEC(FDwUIViewModelSpec, "DeadwaterRun.UI",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	FDwCombatFixture F;
	UDwCombatInteraction* Interaction = nullptr;
	UDwCombatHudViewModel* Hud = nullptr;

	FDwUnitHandle C(int32 Index) const { return FDwCombatFixture::CrewHandle(Index); }
	FDwUnitHandle E(int32 Index) const { return F.EnemyHandle(Index); }

	/** Keys "1".."6" and Space, as IMC_Combat maps them. */
	static FDwHudKeys TestKeys()
	{
		FDwHudKeys Keys;
		for (int32 Slot = 1; Slot <= 6; ++Slot)
		{
			Keys.Abilities.Add(FText::AsNumber(Slot));
		}
		Keys.EndTurn = FText::FromString(TEXT("Space"));
		return Keys;
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
		Hud = F.Root(NewObject<UDwCombatHudViewModel>());
		Hud->Initialize(*F.Combat, Interaction, TestKeys());
		return true;
	}

	/** The action view model for an ability (and item), or nullptr. */
	UDwActionViewModel* Action(const UDwAbilityDefinition* Ability, FName Item = NAME_None) const
	{
		for (UDwActionViewModel* Slot : Hud->GetActions())
		{
			const TArray<FDwActionSlot> Bar = Interaction->GetActionBar();
			const int32 Index = Slot->GetView().SlotIndex;
			if (Bar.IsValidIndex(Index) && Bar[Index].Ability == Ability && Bar[Index].ItemId == Item)
			{
				return Slot;
			}
		}
		return nullptr;
	}

	FDwCombatCommand AttackCommand(const UDwAbilityDefinition* Ability, FDwUnitHandle Source, FDwUnitHandle Target) const
	{
		return FDwCombatCommand::MakeAbility(Source, Ability->AbilityTag, FDwAbilityTarget::MakeUnit(Target, F.Unit(Target)->GetTile()));
	}

	/** Spends every crew member's action (volley) and move, like the DW-105 interaction spec. */
	void SpendEverything()
	{
		for (int32 Index = 0; Index < 3; ++Index)
		{
			int32 Alive = 0;
			while (Alive < 2 && !F.Unit(E(Alive))->IsAlive()) { ++Alive; } // volleys hit at random; aim at a living one
			F.Combat->SubmitCommand(AttackCommand(F.Volley, C(Index), E(Alive)));
			F.Asc(C(Index))->SetNumericAttributeBase(UDwUnitAttributeSet::GetMovePointsAttribute(), 0.f);
		}
	}

END_DEFINE_SPEC(FDwUIViewModelSpec)

void FDwUIViewModelSpec::Define()
{
	BeforeEach([this]()
	{
		F.Create();
		F.UseRealActions();
	});
	AfterEach([this]()
	{
		if (Hud)
		{
			Hud->Shutdown();
			Hud = nullptr;
		}
		if (Interaction)
		{
			Interaction->Shutdown();
			Interaction = nullptr;
		}
		F.Destroy();
	});

	Describe("Action view model", [this]()
	{
		It("mirrors the selected pirate's action bar: names, MP cost, items left and hotkeys", [this]()
		{
			F.AbilityTable->FindRow<FDwAbilityTuningRow>(TEXT("Volley"), TEXT(""))->ManaCost = 5;
			if (!Begin()) { return; }
			const TArray<FDwActionSlot> Bar = Interaction->GetActionBar();
			TestEqual(TEXT("One view model per slot"), Hud->GetActions().Num(), Bar.Num());
			const UDwActionViewModel* Volley = Action(F.Volley);
			const UDwActionViewModel* Potion = Action(F.Item, F.Potion->GetFName());
			if (!TestNotNull(TEXT("Volley slot"), Volley) || !TestNotNull(TEXT("Potion slot"), Potion)) { return; }
			TestEqual(TEXT("Name"), Volley->GetView().Name.ToString(), F.Volley->DisplayName.ToString());
			TestEqual(TEXT("MP cost from tuning"), Volley->GetView().ManaCost, 5);
			TestEqual(TEXT("Cost text"), Volley->GetView().CostText.ToString(), FString(TEXT("5 MP")));
			TestTrue(TEXT("Usable"), Volley->GetView().bEnabled);
			TestTrue(TEXT("No reason when usable"), Volley->GetView().DisabledReason.IsEmpty());
			TestEqual(TEXT("Hotkey = slot key"), Volley->GetView().Hotkey.ToString(), FString::FromInt(Volley->GetView().SlotIndex + 1));
			TestEqual(TEXT("Items left"), Potion->GetView().Count, 2);
		});

		It("reads hotkey labels from the input mapping context", [this]()
		{
			UInputMappingContext* Context = F.Root(NewObject<UInputMappingContext>());
			UInputAction* Ability1 = F.Root(NewObject<UInputAction>());
			UInputAction* EndTurn = F.Root(NewObject<UInputAction>());
			Context->MapKey(Ability1, EKeys::One);
			Context->MapKey(EndTurn, EKeys::SpaceBar);
			TestEqual(TEXT("Slot key"), FDwHudKeys::KeyFor(Context, Ability1).ToString(), EKeys::One.GetDisplayName(false).ToString());
			TestEqual(TEXT("End turn key"), FDwHudKeys::KeyFor(Context, EndTurn).ToString(), EKeys::SpaceBar.GetDisplayName(false).ToString());
			TestTrue(TEXT("Unmapped = empty"), FDwHudKeys::KeyFor(Context, nullptr).IsEmpty());
		});

		It("gives the same reason SubmitCommand gives when MP is short", [this]()
		{
			F.AbilityTable->FindRow<FDwAbilityTuningRow>(TEXT("Volley"), TEXT(""))->ManaCost = 30; // MaxMana is 20
			if (!Begin()) { return; }
			const UDwActionViewModel* Volley = Action(F.Volley);
			if (!TestNotNull(TEXT("Volley slot"), Volley)) { return; }
			FText Reason;
			const EDwCommandResult Result = F.Combat->SubmitCommand(AttackCommand(F.Volley, C(0), E(0)), &Reason);
			TestEqual(TEXT("Command refused"), Result, EDwCommandResult::CannotAfford);
			TestFalse(TEXT("Disabled"), Volley->GetView().bEnabled);
			TestTrue(TEXT("Same reason as SubmitCommand"), Volley->GetView().DisabledReason.EqualTo(Reason));
		});

		It("gives the same reason SubmitCommand gives once the action is spent", [this]()
		{
			if (!Begin()) { return; }
			TestEqual(TEXT("First volley"), F.Combat->SubmitCommand(AttackCommand(F.Volley, C(0), E(0))), EDwCommandResult::Accepted);
			const UDwActionViewModel* Volley = Action(F.Volley);
			if (!TestNotNull(TEXT("Volley slot"), Volley)) { return; }
			FText Reason;
			const EDwCommandResult Result = F.Combat->SubmitCommand(AttackCommand(F.Volley, C(0), E(1)), &Reason);
			TestEqual(TEXT("Second volley refused"), Result, EDwCommandResult::CannotAfford);
			TestFalse(TEXT("Disabled"), Volley->GetView().bEnabled);
			TestTrue(TEXT("Same reason as SubmitCommand"), Volley->GetView().DisabledReason.EqualTo(Reason));
		});

		It("disables a slot with no target, and SubmitCommand refuses every target for it", [this]()
		{
			if (!Begin()) { return; }
			const UDwActionViewModel* Melee = Action(F.Melee); // range 1: every enemy is 9 columns away
			if (!TestNotNull(TEXT("Melee slot"), Melee)) { return; }
			TestFalse(TEXT("Disabled"), Melee->GetView().bEnabled);
			TestFalse(TEXT("With a reason"), Melee->GetView().DisabledReason.IsEmpty());
			for (int32 Index = 0; Index < 3; ++Index)
			{
				TestEqual(TEXT("Refused"), F.Combat->SubmitCommand(AttackCommand(F.Melee, C(0), E(Index))), EDwCommandResult::InvalidTarget);
			}
			// No-target reasons come from the interaction's action bar ("Stand on or next to one of your loaded cannons"),
			// while SubmitCommand reports IsValidTarget's generic "No valid target": same refusal, different text. Raised
			// for gameplay-engineer in the DW-107 report; this test pins the part that must hold today.
			const UDwActionViewModel* Cannon = Action(F.Cannon);
			if (!TestNotNull(TEXT("Cannon slot"), Cannon)) { return; }
			TestFalse(TEXT("Cannon disabled"), Cannon->GetView().bEnabled);
			TestTrue(TEXT("Cannon reason"), Cannon->GetView().DisabledReason.EqualTo(UDwGA_FireCannon::GetNoCannonReason()));
			TestEqual(TEXT("Cannon refused"), F.Combat->SubmitCommand(FDwCombatCommand::MakeAbility(C(0), DwTags::Ability_Cannon,
				FDwAbilityTarget::MakeShip(EDwSide::Enemy))), EDwCommandResult::InvalidTarget);
		});

		It("arms its slot through the interaction when selected", [this]()
		{
			if (!Begin()) { return; }
			UDwActionViewModel* Volley = Action(F.Volley);
			if (!TestNotNull(TEXT("Volley slot"), Volley)) { return; }
			TestTrue(TEXT("Selected"), Volley->Select());
			TestEqual(TEXT("Interaction targeting"), Interaction->GetMode(), EDwInteractionMode::Targeting);
			TestTrue(TEXT("Shown as armed"), Volley->GetView().bArmed);
			TestTrue(TEXT("Cancel through the HUD"), Hud->Cancel());
			TestFalse(TEXT("Disarmed"), Volley->GetView().bArmed);
		});
	});

	Describe("Combat HUD view model", [this]()
	{
		It("shows both ships, the crew and the enemies from the snapshot", [this]()
		{
			if (!Begin()) { return; }
			TestEqual(TEXT("Crew cards"), Hud->GetCrew().Num(), 3);
			TestEqual(TEXT("Enemy bars"), Hud->GetEnemies().Num(), 3);
			TestTrue(TEXT("Player ship"), Hud->GetPlayerShip().bPresent);
			TestTrue(TEXT("Enemy ship"), Hud->GetEnemyShip().bPresent);
			TestEqual(TEXT("Ship health"), Hud->GetEnemyShip().Health, F.ShipHealth(EDwSide::Enemy));
			const FDwUnitView& Captain = Hud->GetCrew()[0]->GetView();
			TestTrue(TEXT("Captain marked"), Captain.bIsCaptain);
			TestTrue(TEXT("Selected at turn start"), Captain.bSelected);
			TestEqual(TEXT("HP"), Captain.Health, F.Health(C(0)));
			TestEqual(TEXT("Max MP"), Captain.MaxMana, 20.f);
			TestEqual(TEXT("Name from the definition"), Captain.Name.ToString(), F.Sailor->DisplayName.ToString());
			TestTrue(TEXT("Move and action left"), Captain.bHasMove && Captain.bHasAction);
			TestEqual(TEXT("Enemy Fear max"), Hud->GetEnemies()[0]->GetView().MaxFear, F.Setup.MaxFear);
			TestEqual(TEXT("Round 1"), Hud->GetRound(), 1);
			TestTrue(TEXT("Player may act"), Hud->CanAct());
		});

		It("follows damage, and only the changed unit broadcasts", [this]()
		{
			if (!Begin()) { return; }
			const UDwUnitViewModel* Target = Hud->FindUnit(E(0));
			const UDwUnitViewModel* Other = Hud->FindUnit(E(2));
			if (!TestNotNull(TEXT("Target"), Target) || !TestNotNull(TEXT("Other"), Other)) { return; }
			const int32 TargetChanges = Target->GetChangeCount();
			const int32 OtherChanges = Other->GetChangeCount();
			TestEqual(TEXT("Volley"), F.Combat->SubmitCommand(AttackCommand(F.Volley, C(0), E(0))), EDwCommandResult::Accepted);
			// Volley hits random enemies; whichever was hit must match its live HP.
			for (int32 Index = 0; Index < 3; ++Index)
			{
				TestEqual(TEXT("HP follows the unit"), Hud->FindUnit(E(Index))->GetView().Health, F.Health(E(Index)));
			}
			TestTrue(TEXT("Hit units broadcast"), Target->GetChangeCount() > TargetChanges || F.Health(E(0)) == 100.f);
			if (F.Health(E(2)) == 100.f)
			{
				TestEqual(TEXT("An untouched enemy stays quiet"), Other->GetChangeCount(), OtherChanges);
			}
			TestFalse(TEXT("Shooter has no action left"), Hud->GetCrew()[0]->GetView().bHasAction);
		});

		It("lists statuses with the turns they have left", [this]()
		{
			if (!Begin()) { return; }
			F.ApplyEffect(DwGas::GetSharedEffects().Frozen, E(0), C(0));
			Hud->Refresh();
			const TArray<FDwStatusView>& Statuses = Hud->FindUnit(E(0))->GetView().Statuses;
			const FDwStatusView* Frozen = Statuses.FindByPredicate([](const FDwStatusView& S) { return S.Tag == DwTags::Status_Incapacitated_Frozen; });
			if (!TestNotNull(TEXT("Frozen listed"), Frozen)) { return; }
			TestEqual(TEXT("Label"), Frozen->Label.ToString(), FString(TEXT("Frozen")));
			TestTrue(TEXT("Turns left"), Frozen->TurnsRemaining >= 1);
			TestTrue(TEXT("Others clean"), Hud->FindUnit(E(1))->GetView().Statuses.IsEmpty());
		});

		It("raises End turn? on OnEndTurnSuggested and clears it on the next turn", [this]()
		{
			if (!Begin()) { return; }
			SpendEverything();
			TestFalse(TEXT("Not yet (moves were zeroed outside a command)"), Hud->IsEndTurnSuggested());
			F.Combat->SubmitCommand(FDwCombatCommand::MakeEndUnit(C(0))); // any resolved command re-checks the suggestion
			TestTrue(TEXT("Suggested"), Hud->IsEndTurnSuggested());
			TestTrue(TEXT("The turn did not end by itself"), Hud->IsPlayerTurn());
			Hud->EndTurn();
			TestEqual(TEXT("Round 2"), Hud->GetRound(), 2);
			TestTrue(TEXT("Player again"), Hud->IsPlayerTurn());
			TestFalse(TEXT("Cleared on the next turn"), Hud->IsEndTurnSuggested());
		});

		It("clears End turn? on the next command", [this]()
		{
			if (!Begin()) { return; }
			SpendEverything();
			F.Combat->SubmitCommand(FDwCombatCommand::MakeEndUnit(C(0)));
			if (!TestTrue(TEXT("Suggested"), Hud->IsEndTurnSuggested())) { return; }
			F.Combat->SubmitCommand(FDwCombatCommand::MakeEndUnit(C(1)));
			TestFalse(TEXT("Cleared"), Hud->IsEndTurnSuggested());
		});

		It("announces every side change for the turn banner", [this]()
		{
			if (!Begin()) { return; }
			UDwUITestListener* Listener = F.Root(NewObject<UDwUITestListener>());
			Hud->OnTurnBanner.AddDynamic(Listener, &UDwUITestListener::HandleTurnBanner);
			Hud->EndTurn(); // the passive enemy side passes straight back
			if (!TestEqual(TEXT("Enemy then player"), Listener->Banners.Num(), 2)) { return; }
			TestEqual(TEXT("Enemy side"), Listener->Banners[0].Key, EDwSide::Enemy);
			TestEqual(TEXT("Round 1"), Listener->Banners[0].Value, 1);
			TestEqual(TEXT("Player side"), Listener->Banners[1].Key, EDwSide::Player);
			TestEqual(TEXT("Round 2"), Listener->Banners[1].Value, 2);
		});

		It("previews the damage of the armed action before the click", [this]()
		{
			if (!Begin()) { return; }
			Hud->SelectAction(Action(F.Volley)->GetView().SlotIndex);
			Interaction->HoverTile(F.Unit(E(0))->GetTile());
			TestTrue(TEXT("Aiming hint"), Hud->GetHint().ToString().Contains(F.Volley->DisplayName.ToString()));
			if (!TestTrue(TEXT("Preview lines"), Hud->GetPreview().Num() > 0)) { return; }
			const FDwPreviewLine& Line = Hud->GetPreview()[0];
			TestEqual(TEXT("3-5 hits of 10"), Line.Effect.ToString(), FString(TEXT("-30 to -50 HP")));
			TestFalse(TEXT("Not lethal"), Line.bLethal);
			TestTrue(TEXT("Hovered enemy marked"), Hud->FindUnit(E(0))->GetView().bHovered);
		});

		It("selects a pirate through the interaction", [this]()
		{
			if (!Begin()) { return; }
			Hud->GetCrew()[2]->Select();
			TestEqual(TEXT("Interaction selection"), Interaction->GetSelectedUnit(), C(2));
			TestTrue(TEXT("Card shows it"), Hud->GetCrew()[2]->GetView().bSelected);
			TestFalse(TEXT("Previous card cleared"), Hud->GetCrew()[0]->GetView().bSelected);
		});
	});

	Describe("Combat HUD outcome", [this]()
	{
		BeforeEach([this]()
		{
			// Grunts carry Broadside (300 to the enemy ship): one shot sinks it.
			F.Destroy();
			F.Create();
		});

		It("shows the outcome and locks the bar when the fight ends", [this]()
		{
			if (!Begin()) { return; }
			TestEqual(TEXT("Broadside"), F.FireAtShip(C(0), EDwSide::Enemy), EDwCommandResult::Accepted);
			TestEqual(TEXT("Outcome"), Hud->GetOutcome(), EDwCombatOutcome::VictorySunk);
			TestFalse(TEXT("Outcome text"), Hud->GetOutcomeText().IsEmpty());
			TestFalse(TEXT("Player can't act"), Hud->CanAct());
			for (const UDwActionViewModel* Slot : Hud->GetActions())
			{
				TestFalse(TEXT("Bar locked"), Slot->GetView().bEnabled);
			}
		});
	});

	Describe("Floating text", [this]()
	{
		It("describes damage, heals, statuses and fear gains", [this]()
		{
			FDwCombatEvent Event;
			FDwFloatingTextRequest Request;
			Event.Type = EDwCombatEventType::UnitDamaged; Event.Target = E(0); Event.Magnitude = 25.f;
			TestTrue(TEXT("Damage"), UDwFloatingTextManager::Describe(Event, Request) && Request.Text.ToString() == TEXT("-25"));
			Event.Type = EDwCombatEventType::UnitHealed; Event.Magnitude = 30.f;
			TestTrue(TEXT("Heal"), UDwFloatingTextManager::Describe(Event, Request) && Request.Text.ToString() == TEXT("+30"));
			Event.Type = EDwCombatEventType::StatusApplied; Event.Tag = DwTags::Status_Incapacitated_Frozen;
			TestTrue(TEXT("Frozen!"), UDwFloatingTextManager::Describe(Event, Request) && Request.Text.ToString() == TEXT("Frozen!"));
			Event.Type = EDwCombatEventType::FearChanged; Event.Magnitude = 1.f;
			TestTrue(TEXT("+1 Fear"), UDwFloatingTextManager::Describe(Event, Request) && Request.Text.ToString() == TEXT("+1 Fear"));
			Event.Magnitude = -3.f;
			TestFalse(TEXT("Fear reset is silent"), UDwFloatingTextManager::Describe(Event, Request));
			Event.Type = EDwCombatEventType::ShipDamaged; Event.Side = EDwSide::Enemy; Event.Magnitude = 70.f;
			TestTrue(TEXT("Ship"), UDwFloatingTextManager::Describe(Event, Request) && Request.ShipSide == EDwSide::Enemy && !Request.Unit.IsValid());
			Event.Type = EDwCombatEventType::UnitMoved;
			TestFalse(TEXT("Moves show nothing"), UDwFloatingTextManager::Describe(Event, Request));
		});

		It("keeps at most 32 labels, recycling the oldest", [this]()
		{
			const TSubclassOf<UDwFloatingTextWidget> WidgetClass = LoadClass<UDwFloatingTextWidget>(nullptr,
				TEXT("/Game/DeadwaterRun/UI/WBP_FloatingText.WBP_FloatingText_C"));
			if (!TestNotNull(TEXT("WBP_FloatingText (Tools/CreateUIContent.ps1)"), WidgetClass.Get())) { return; }
			if (!Begin()) { return; }
			UDwFloatingTextManager* Manager = F.Root(NewObject<UDwFloatingTextManager>());
			Manager->Initialize(*F.Combat, nullptr, WidgetClass);
			FDwCombatEvent Event;
			Event.Type = EDwCombatEventType::UnitDamaged;
			Event.Target = E(0);
			Event.Magnitude = 5.f;
			for (int32 Index = 0; Index < 40; ++Index)
			{
				F.Combat->OnCombatEvent.Broadcast(Event);
			}
			TestEqual(TEXT("Every event shown"), Manager->GetShownTotal(), 40);
			TestEqual(TEXT("Pool bounded"), Manager->GetPoolCount(), UDwFloatingTextManager::PoolSize);
			TestEqual(TEXT("All labels in use"), Manager->GetShowingCount(), UDwFloatingTextManager::PoolSize);
			Manager->Shutdown();
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
