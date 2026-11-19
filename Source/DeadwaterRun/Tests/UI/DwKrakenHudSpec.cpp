// Copyright Brandon Hillig 2026

// DW-303: the Kraken HUD (Docs/Architecture/14-UI.md "Kraken HUD additions", 08 "Intents"). Played on the real Kraken
// fight (Tools/CreateCombatContent.ps1) with the real WBP_CombatHud / WBP_Intent (Tools/CreateUIContent.ps1): the boss
// bar (health, phase, tentacles and their damage cut), every declared intent with its target and numbers (B6), the
// dodge read-out for tile aims (B6a), the red danger tiles, the phase banner (B7) and "Missed!" over a dodged tile.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/DwAttributeSets.h"
#include "Board/DwBoard.h"
#include "Combat/DwCombatInteraction.h"
#include "Combat/DwEncounterSetupBuilder.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Tests/Combat/DwCombatTestFixture.h"
#include "Tests/UI/DwUITestListener.h"
#include "UI/DwCombatHudWidgets.h"
#include "UI/DwCombatViewModels.h"
#include "UI/DwFloatingText.h"

BEGIN_DEFINE_SPEC(FDwKrakenHudSpec, "DeadwaterRun.UI.Kraken",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	FDwCombatFixture F;
	const UDwGameData* GameData = nullptr;
	UDwCombatInteraction* Interaction = nullptr;
	UDwCombatHudViewModel* Hud = nullptr;

	static FDwUnitHandle Captain() { return FDwCombatFixture::CrewHandle(0); }
	static FDwUnitHandle Swordsman() { return FDwCombatFixture::CrewHandle(1); }
	static FDwUnitHandle Sniper() { return FDwCombatFixture::CrewHandle(2); }
	static FDwUnitHandle Body() { return FDwUnitHandle(4); }
	static FDwUnitHandle Tentacle(int32 Index) { return FDwUnitHandle(5 + Index); }

	/** The Kraken fight with its real brain, plus the interaction and HUD view model a player would have. */
	bool StartKraken()
	{
		const UDwEncounterDefinition* Kraken = GameData ? GameData->KrakenEncounter.Get() : nullptr;
		if (!TestNotNull(TEXT("KrakenEncounter"), Kraken))
		{
			return false;
		}
		TArray<FDwCombatantSetup> Crew;
		for (const UDwUnitDefinition* Unit : GameData->DevCrew)
		{
			FDwCombatantSetup& Member = Crew.AddDefaulted_GetRef();
			Member.Definition = Unit;
			Member.RosterId = FGuid::NewGuid();
			Member.bIsCaptain = Crew.Num() == 1;
		}
		FDwEncounterSetup Setup;
		TArray<FString> Errors;
		if (!TestTrue(TEXT("Built"), FDwEncounterSetupBuilder::Build(*GameData, *Kraken, Crew, 7, Setup, Errors))
			|| !TestTrue(TEXT("Started"), F.Combat->StartEncounter(Setup)))
		{
			return false;
		}
		return Attach();
	}

	bool Attach()
	{
		Interaction = F.Root(NewObject<UDwCombatInteraction>());
		Interaction->Initialize(*F.Combat);
		Interaction->BeginPlayerTurn();
		Hud = F.Root(NewObject<UDwCombatHudViewModel>());
		Hud->Initialize(*F.Combat, Interaction, FDwHudKeys());
		return true;
	}

	void Declare(const FGameplayTag& Move, const FDwAbilityTarget& Target = FDwAbilityTarget())
	{
		FDwAiIntent Intent;
		Intent.AbilityTag = Move;
		Intent.Target = Target;
		Intent.bTelegraph = true;
		F.Combat->DeclareIntents(Body(), { Intent });
		Hud->Refresh();
	}

	FIntPoint TileOf(FDwUnitHandle Unit) const { return F.Unit(Unit)->GetTile(); }

	FIntPoint FreePlayerTile() const
	{
		const FDwGridModel& Grid = F.Combat->GetGrid();
		for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
		{
			const FIntPoint Tile = Grid.ToTile(Index);
			const FDwTile* Data = Grid.GetTile(Tile);
			if (Data && Data->Owner == EDwSide::Player && Data->Elevation == 0 && Grid.IsStandable(Tile) && !Grid.IsOccupied(Tile))
			{
				return Tile;
			}
		}
		return FIntPoint(-1, -1);
	}

	void SetHealth(FDwUnitHandle Unit, float Health) { F.Asc(Unit)->SetNumericAttributeBase(UDwUnitAttributeSet::GetHealthAttribute(), Health); }

	const FDwIntentView* OnlyIntent()
	{
		return TestEqual(TEXT("One intent shown"), Hud->GetIntents().Num(), 1) ? &Hud->GetIntents()[0] : nullptr;
	}

	UDwCombatHudScreen* MakeScreen()
	{
		const TSubclassOf<UDwCombatHudScreen> Class = LoadClass<UDwCombatHudScreen>(nullptr, TEXT("/Game/DeadwaterRun/UI/WBP_CombatHud.WBP_CombatHud_C"));
		if (!TestNotNull(TEXT("WBP_CombatHud (Tools/CreateUIContent.ps1)"), Class.Get()))
		{
			return nullptr;
		}
		UDwCombatHudScreen* Screen = F.Root(CreateWidget<UDwCombatHudScreen>(F.World, Class));
		if (Screen)
		{
			Screen->SetViewModel(Hud);
		}
		return Screen;
	}

END_DEFINE_SPEC(FDwKrakenHudSpec)

void FDwKrakenHudSpec::Define()
{
	BeforeEach([this]()
	{
		F.Create();
		GameData = UDwAssetManager::Get().GetGameData();
	});
	AfterEach([this]()
	{
		if (Hud) { Hud->Shutdown(); }
		if (Interaction) { Interaction->Shutdown(); }
		Hud = nullptr;
		Interaction = nullptr;
		F.Destroy();
	});

	Describe("Boss bar", [this]()
	{
		It("shows the Kraken's health, calm phase and tentacles, and takes the body out of the enemy row", [this]()
		{
			if (!StartKraken()) { return; }
			const FDwBossView& Boss = Hud->GetBoss();
			TestTrue(TEXT("Present"), Boss.bPresent);
			TestEqual(TEXT("Name"), Boss.Name.ToString(), FString(TEXT("The Kraken")));
			TestTrue(TEXT("500 / 500"), Boss.Health == 500.f && Boss.MaxHealth == 500.f);
			TestFalse(TEXT("Calm"), Boss.bEnraged);
			TestTrue(TEXT("No phase label while calm"), Boss.PhaseText.IsEmpty());
			TestEqual(TEXT("Tentacles"), Boss.TentacleText.ToString(), FString(TEXT("Tentacles 4/4")));
			TestEqual(TEXT("Enemy row = the four tentacles"), Hud->GetEnemies().Num(), 4);
			TestNull(TEXT("No card for the body"), Hud->FindUnit(Body()));
		});

		It("counts cut tentacles and the damage they take off (B2)", [this]()
		{
			if (!StartKraken()) { return; }
			F.ApplyEffect(DwGas::GetSharedEffects().Kill, Tentacle(0), Swordsman());
			F.ApplyEffect(DwGas::GetSharedEffects().Kill, Tentacle(1), Swordsman());
			Hud->Refresh();
			const FDwBossView& Boss = Hud->GetBoss();
			TestTrue(TEXT("2 of 4 left"), Boss.TentaclesAlive == 2 && Boss.TentaclesTotal == 4);
			TestEqual(TEXT("Read-out"), Boss.TentacleText.ToString(), FString(TEXT("Tentacles 2/4   -20% damage")));
		});

		It("is absent in a crew fight", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start()) || !Attach()) { return; }
			TestFalse(TEXT("No boss"), Hud->GetBoss().bPresent);
			TestEqual(TEXT("No intents"), Hud->GetIntents().Num(), 0);
			UDwCombatHudScreen* Screen = MakeScreen();
			TestTrue(TEXT("No intent rows"), Screen && Screen->GetIntentRowCount() == 0);
		});
	});

	Describe("Intents", [this]()
	{
		It("shows the opening move the Kraken declared before turn 1", [this]()
		{
			if (!StartKraken()) { return; }
			const FDwIntentView* Intent = OnlyIntent();
			TestTrue(TEXT("Named, targeted and numbered"), Intent && Intent->Order == 1 && !Intent->MoveName.IsEmpty()
				&& !Intent->TargetText.IsEmpty() && !Intent->EffectText.IsEmpty());
		});

		It("reads a slam at a pirate's tile: who, how much, dodgeable, and that tile in danger", [this]()
		{
			if (!StartKraken()) { return; }
			const FIntPoint Aimed = TileOf(Swordsman());
			Declare(DwTags::Ability_Kraken_TentacleSlam, FDwAbilityTarget::MakeTile(Aimed));
			const FDwIntentView* Intent = OnlyIntent();
			if (!Intent) { return; }
			TestEqual(TEXT("Move"), Intent->MoveName.ToString(), FString(TEXT("Tentacle Slam")));
			TestEqual(TEXT("Target"), Intent->TargetText.ToString(), FString(TEXT("Swordsman's tile")));
			TestEqual(TEXT("Effect"), Intent->EffectText.ToString(), FString(TEXT("50 damage")));
			TestTrue(TEXT("Dodgeable, not a miss yet"), Intent->bDodgeable && !Intent->bWillMiss);
			TestTrue(TEXT("Danger on that tile only"), Intent->DangerTiles.Num() == 1 && Intent->DangerTiles[0] == Aimed);
		});

		It("says the slam will miss once the pirate steps off, and the tile stays marked", [this]()
		{
			if (!StartKraken()) { return; }
			const FIntPoint Aimed = TileOf(Swordsman());
			Declare(DwTags::Ability_Kraken_TentacleSlam, FDwAbilityTarget::MakeTile(Aimed));
			F.Combat->MoveUnit(Swordsman(), FreePlayerTile(), true);
			Hud->Refresh();
			const FDwIntentView* Intent = OnlyIntent();
			if (!Intent) { return; }
			TestEqual(TEXT("Empty tile"), Intent->TargetText.ToString(), FString(TEXT("Empty tile")));
			TestTrue(TEXT("Will miss"), Intent->bWillMiss);
			TestEqual(TEXT("Says so"), Intent->EffectText.ToString(), FString(TEXT("Nobody there - it will miss")));
			TestTrue(TEXT("Still marked"), Intent->DangerTiles.Contains(Aimed));
		});

		It("reads group moves, self moves and ship slams", [this]()
		{
			if (!StartKraken()) { return; }
			Declare(DwTags::Ability_Kraken_KrankySwipe);
			const FDwIntentView* Swipe = OnlyIntent();
			TestTrue(TEXT("Swipe: every pirate, 20 each, three danger tiles"), Swipe && Swipe->TargetText.ToString() == TEXT("Every pirate")
				&& Swipe->EffectText.ToString() == TEXT("20 to each pirate") && Swipe->DangerTiles.Num() == 3 && !Swipe->bDodgeable);

			Declare(DwTags::Ability_Kraken_ArmsOfFury);
			const FDwIntentView* Arms = OnlyIntent();
			TestTrue(TEXT("Arms of Fury: random, 3-5 hits of 40"), Arms && Arms->TargetText.ToString() == TEXT("Random pirates")
				&& Arms->EffectText.ToString() == TEXT("3-5 hits of 40"));

			Declare(DwTags::Ability_Kraken_EyesOfTheSea, FDwAbilityTarget::MakeUnit(Body(), TileOf(Body())));
			const FDwIntentView* Eyes = OnlyIntent();
			TestTrue(TEXT("Eyes: itself, its description, no danger"), Eyes && Eyes->TargetText.ToString() == TEXT("Itself")
				&& !Eyes->EffectText.IsEmpty() && Eyes->DangerTiles.IsEmpty());

			Declare(DwTags::Ability_Kraken_TentacleSlam, FDwAbilityTarget::MakeShip(EDwSide::Player));
			const FDwIntentView* Ship = OnlyIntent();
			TestTrue(TEXT("Ship slam: your ship, 50, no tiles"), Ship && Ship->TargetText.ToString() == TEXT("Your ship")
				&& Ship->EffectText.ToString() == TEXT("50 damage") && Ship->DangerTiles.IsEmpty());
		});

		It("warns when a hit would kill, louder when it would kill the captain, and shows the tentacle cut", [this]()
		{
			if (!StartKraken()) { return; }
			SetHealth(Captain(), 40.f);
			Declare(DwTags::Ability_Kraken_TentacleSlam, FDwAbilityTarget::MakeTile(TileOf(Captain())));
			const FDwIntentView* Intent = OnlyIntent();
			TestTrue(TEXT("Lethal to the captain"), Intent && Intent->bLethal && Intent->bThreatensCaptain);

			F.ApplyEffect(DwGas::GetSharedEffects().Kill, Tentacle(0), Swordsman());
			Hud->Refresh();
			Intent = OnlyIntent();
			TestTrue(TEXT("45 after one cut tentacle"), Intent && Intent->EffectText.ToString() == TEXT("45 damage"));
		});

		It("lights the danger tiles on the board, on either side's turn", [this]()
		{
			if (!StartKraken()) { return; }
			ADwBoard* Board = F.Combat->GetBoard();
			if (!TestNotNull(TEXT("Board"), Board)) { return; }
			Declare(DwTags::Ability_Kraken_KrankySwipe);
			Interaction->Refresh();
			TestEqual(TEXT("Three crew tiles"), Board->GetHighlightCount(EDwHighlightLayer::Danger), 3);
			TestEqual(TEXT("Same as the pure query"), UDwCombatInteraction::GatherDangerTiles(F.Combat->GetSnapshot()).Num(), 3);
			Declare(DwTags::Ability_Kraken_EyesOfTheSea, FDwAbilityTarget::MakeUnit(Body(), TileOf(Body())));
			Interaction->Refresh();
			TestEqual(TEXT("Nothing to dodge"), Board->GetHighlightCount(EDwHighlightLayer::Danger), 0);
		});

		It("fills one WBP_Intent row per intent in WBP_CombatHud", [this]()
		{
			if (!StartKraken()) { return; }
			UDwCombatHudScreen* Screen = MakeScreen();
			if (!Screen) { return; }
			TestEqual(TEXT("One row"), Screen->GetIntentRowCount(), 1);
			FDwAiIntent Swipe;
			Swipe.AbilityTag = DwTags::Ability_Kraken_KrankySwipe;
			FDwAiIntent Grab;
			Grab.AbilityTag = DwTags::Ability_Kraken_TentacleGrab;
			Grab.Target = FDwAbilityTarget::MakeTile(TileOf(Sniper()));
			F.Combat->DeclareIntents(Body(), { Swipe, Grab });
			TestEqual(TEXT("Two rows after the next declaration"), Screen->GetIntentRowCount(), 2);
			TestTrue(TEXT("Second row is the grab"), Hud->GetIntents().Num() == 2 && Hud->GetIntents()[1].Order == 2
				&& Hud->GetIntents()[1].TargetText.ToString() == TEXT("Sniper's tile"));
		});
	});

	Describe("Phase and misses", [this]()
	{
		It("announces the enraged phase with a banner and marks the bar (B7)", [this]()
		{
			if (!StartKraken()) { return; }
			UDwUITestListener* Listener = F.Root(NewObject<UDwUITestListener>());
			Hud->OnPhaseBanner.AddDynamic(Listener, &UDwUITestListener::HandlePhaseBanner);
			UDwCombatHudScreen* Screen = MakeScreen();
			for (int32 Index = 0; Index < 3; ++Index)
			{
				F.Asc(FDwCombatFixture::CrewHandle(Index))->SetNumericAttributeBase(UDwUnitAttributeSet::GetMaxHealthAttribute(), 5000.f);
				SetHealth(FDwCombatFixture::CrewHandle(Index), 5000.f);
			}
			SetHealth(Body(), 100.f);
			F.EndTurn();
			TestTrue(TEXT("One banner: The Kraken is ENRAGED!"), Listener->PhaseBanners.Num() == 1
				&& Listener->PhaseBanners[0].Key == TEXT("The Kraken is ENRAGED!") && Listener->PhaseBanners[0].Value == 1);
			TestTrue(TEXT("Bar says ENRAGED"), Hud->GetBoss().bEnraged && Hud->GetBoss().PhaseText.ToString() == TEXT("ENRAGED"));
			TestTrue(TEXT("The HUD shows it"), Screen && Screen->IsPhaseBannerVisible());
		});

		It("floats Missed! over a dodged tile", [this]()
		{
			FDwCombatEvent Event;
			Event.Type = EDwCombatEventType::AttackMissed;
			Event.Source = Body();
			Event.Tile = FIntPoint(3, 2);
			FDwFloatingTextRequest Request;
			TestTrue(TEXT("Described"), UDwFloatingTextManager::Describe(Event, Request));
			TestTrue(TEXT("Missed! at the tile"), Request.Text.ToString() == TEXT("Missed!") && Request.bAtTile && Request.Tile == FIntPoint(3, 2)
				&& !Request.Unit.IsValid());
		});
	});
}

#endif
