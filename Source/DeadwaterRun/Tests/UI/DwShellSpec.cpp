// Copyright Brandon Hillig 2026

// DW-210: shell view models without widgets (Docs/Architecture/16-Shell.md "How it's tested") and UDwGameUserSettings.
// Settings tests use their own settings object and a temporary ini, never the player's GameUserSettings.ini.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwCaptainDefinition.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Presentation/DwPresentationSubsystem.h"
#include "Shell/DwGameUserSettings.h"
#include "Tests/Combat/DwCombatTestFixture.h"
#include "UI/DwShellScreens.h"
#include "UI/DwShellViewModels.h"
#include "UI/DwUIScreenSet.h"

BEGIN_DEFINE_SPEC(FDwShellSettingsSpec, "DeadwaterRun.Shell.Settings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	TArray<UObject*> Rooted;

	template <typename T>
	T* Make()
	{
		T* Object = NewObject<T>(GetTransientPackage());
		Object->AddToRoot();
		Rooted.Add(Object);
		return Object;
	}

	/** A settings object at the defaults, whatever the player's GameUserSettings.ini says. */
	UDwGameUserSettings* MakeSettings()
	{
		UDwGameUserSettings* Settings = Make<UDwGameUserSettings>();
		Settings->SetToDefaults();
		return Settings;
	}
END_DEFINE_SPEC(FDwShellSettingsSpec)

void FDwShellSettingsSpec::Define()
{
	AfterEach([this]()
	{
		for (UObject* Object : Rooted)
		{
			Object->RemoveFromRoot();
		}
		Rooted.Reset();
	});

	It("is the engine's settings class", [this]()
	{
		TestNotNull(TEXT("GameUserSettingsClassName = DwGameUserSettings"), UDwGameUserSettings::Get());
	});

	It("round-trips every Deadwater setting through an ini", [this]()
	{
		const FString Ini = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/DwSettingsTest.ini"));
		UDwGameUserSettings* Saved = MakeSettings();
		Saved->SetVolume(EDwVolume::Music, 0.3f);
		Saved->SetVolume(EDwVolume::Ui, 0.f);
		Saved->SetGameSpeed(EDwGameSpeed::Fastest);
		Saved->SetScreenShakeEnabled(false);
		Saved->SetHighlightPalette(EDwHighlightPalette::ColorblindSafe);
		Saved->SaveConfig(CPF_Config, *Ini, GConfig, /*bAllowCopyToDefaultObject*/ false); // never touch the class defaults
		GConfig->Flush(/*bRemoveFromCache*/ true, Ini);

		UDwGameUserSettings* Loaded = MakeSettings();
		Loaded->LoadConfig(nullptr, *Ini);
		TestEqual(TEXT("Music"), Loaded->GetVolume(EDwVolume::Music), 0.3f, 1e-4f);
		TestEqual(TEXT("UI"), Loaded->GetVolume(EDwVolume::Ui), 0.f);
		TestEqual(TEXT("Master default"), Loaded->GetVolume(EDwVolume::Master), 1.f);
		TestEqual(TEXT("Speed"), Loaded->GetGameSpeed(), EDwGameSpeed::Fastest);
		TestFalse(TEXT("Shake"), Loaded->IsScreenShakeEnabled());
		TestEqual(TEXT("Palette"), Loaded->GetHighlightPalette(), EDwHighlightPalette::ColorblindSafe);
		IFileManager::Get().Delete(*Ini);
	});

	It("maps game speed to 1x / 1.5x / 2x and applies it to a world's presentation", [this]()
	{
		TestEqual(TEXT("Normal"), UDwGameUserSettings::GetPlaybackRate(EDwGameSpeed::Normal), 1.f);
		TestEqual(TEXT("Fast"), UDwGameUserSettings::GetPlaybackRate(EDwGameSpeed::Fast), 1.5f);
		TestEqual(TEXT("Fastest"), UDwGameUserSettings::GetPlaybackRate(EDwGameSpeed::Fastest), 2.f);
		FDwCombatFixture F;
		F.Create();
		UDwPresentationSubsystem* Presentation = F.World->GetSubsystem<UDwPresentationSubsystem>();
		TestEqual(TEXT("Test worlds start at 1x whatever the player's setting"), Presentation->GetPlaybackRate(), 1.f);
		UDwGameUserSettings* Settings = MakeSettings();
		Settings->SetGameSpeed(EDwGameSpeed::Fast);
		Settings->ApplyPresentation(F.World);
		TestEqual(TEXT("Applied"), Presentation->GetPlaybackRate(), 1.5f);
		F.Destroy();
	});

	It("clamps volumes", [this]()
	{
		UDwGameUserSettings* Settings = MakeSettings();
		Settings->SetVolume(EDwVolume::Sfx, 3.f);
		TestEqual(TEXT("Max 1"), Settings->GetVolume(EDwVolume::Sfx), 1.f);
		Settings->SetVolume(EDwVolume::Sfx, -1.f);
		TestEqual(TEXT("Min 0"), Settings->GetVolume(EDwVolume::Sfx), 0.f);
	});
}

BEGIN_DEFINE_SPEC(FDwShellViewModelSpec, "DeadwaterRun.UI.Shell",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	TArray<UObject*> Rooted;

	template <typename T>
	T* Make()
	{
		T* Object = NewObject<T>(GetTransientPackage());
		Object->AddToRoot();
		Rooted.Add(Object);
		return Object;
	}

	/** A settings object at the defaults, whatever the player's GameUserSettings.ini says. */
	UDwGameUserSettings* MakeSettings()
	{
		UDwGameUserSettings* Settings = Make<UDwGameUserSettings>();
		Settings->SetToDefaults();
		return Settings;
	}

	TArray<const UDwCaptainDefinition*> Captains()
	{
		TArray<const UDwCaptainDefinition*> Out;
		if (const UDwGameData* GameData = UDwAssetManager::Get().GetGameData())
		{
			for (const UDwCaptainDefinition* Captain : GameData->Captains)
			{
				Out.Add(Captain);
			}
		}
		return Out;
	}
END_DEFINE_SPEC(FDwShellViewModelSpec)

void FDwShellViewModelSpec::Define()
{
	AfterEach([this]()
	{
		for (UObject* Object : Rooted)
		{
			Object->RemoveFromRoot();
		}
		Rooted.Reset();
	});

	Describe("Menu", [this]()
	{
		It("shows its buttons, sends their intents, and runs Esc only when it has one", [this]()
		{
			UDwMenuViewModel* Menu = Make<UDwMenuViewModel>();
			int32 Resumed = 0, Backs = 0;
			TArray<FDwMenuButton> Buttons;
			Buttons.Add({ FText::FromString(TEXT("Resume")), FText::GetEmpty(), [&Resumed]() { ++Resumed; return true; } });
			FDwMenuButton Abandon{ FText::FromString(TEXT("Abandon Run")), FText::GetEmpty(), []() { return true; } };
			Abandon.bEnabled = false;
			Abandon.DisabledReason = FText::FromString(TEXT("No run to abandon (dev fight)."));
			Buttons.Add(MoveTemp(Abandon));
			Menu->Set(FText::FromString(TEXT("Paused")), {}, MoveTemp(Buttons), [&Backs]() { ++Backs; return true; });

			TestEqual(TEXT("Two buttons"), Menu->GetButtons().Num(), 2);
			TestTrue(TEXT("Resume"), Menu->FindButton(TEXT("Resume"))->Select());
			TestEqual(TEXT("Resumed once"), Resumed, 1);
			UDwChoiceViewModel* Disabled = Menu->FindButton(TEXT("Abandon Run"));
			TestFalse(TEXT("A disabled button does nothing"), Disabled->Select());
			TestTrue(TEXT("And says why"), Disabled->GetView().DisabledReason.ToString().Contains(TEXT("dev fight")));
			TestTrue(TEXT("Esc"), Menu->HandleBack());
			TestEqual(TEXT("Esc ran"), Backs, 1);

			Menu->Set(FText::FromString(TEXT("Main menu")), {}, {});
			TestFalse(TEXT("No Esc on the main menu"), Menu->HasBack() || Menu->HandleBack());
			TestNull(TEXT("Unknown button"), Menu->FindButton(TEXT("Resume")));
		});
	});

	Describe("Run summary", [this]()
	{
		It("headlines every end reason", [this]()
		{
			TestTrue(TEXT("Victory"), DwShell::Headline(EDwRunEndReason::Victory).ToString().Contains(TEXT("Victory")));
			TestTrue(TEXT("Sunk"), DwShell::Headline(EDwRunEndReason::ShipSunk).ToString().Contains(TEXT("ship")));
			TestTrue(TEXT("Crew"), DwShell::Headline(EDwRunEndReason::CrewDead).ToString().Contains(TEXT("crew")));
			TestTrue(TEXT("Captain"), DwShell::Headline(EDwRunEndReason::CaptainDead).ToString().Contains(TEXT("captain")));
			TestEqual(TEXT("Abandoned (U5)"), DwShell::Headline(EDwRunEndReason::Abandoned).ToString(), FString(TEXT("Run abandoned")));
		});

		It("tells the voyage from FDwRunStats (U4)", [this]()
		{
			FDwRunStats Stats;
			const TArray<const UDwCaptainDefinition*> All = Captains();
			Stats.Captain = All.IsEmpty() ? nullptr : All[0];
			Stats.Seed = 1234567;
			Stats.NodesCleared = 6;
			Stats.FightsSunk = 2;
			Stats.FightsPlundered = 3;
			Stats.FightsSurrendered = 1;
			Stats.GoldEarned = 1050;
			Stats.Kills = 14;
			Stats.ElapsedSeconds = 125.f;
			FDwCrewLoss& Loss = Stats.CrewLost.AddDefaulted_GetRef();
			Loss.Name = FText::FromString(TEXT("Sniper"));
			FString All2;
			for (const FText& Line : DwShell::SummaryLines(Stats))
			{
				All2 += Line.ToString() + TEXT("\n");
			}
			for (const TCHAR* Expected : { TEXT("Seed: 1234567"), TEXT("Stops cleared: 6"), TEXT("Ships sunk: 2"), TEXT("Crews plundered: 3"),
				TEXT("Surrenders: 1"), TEXT("Gold earned: 1,050"), TEXT("Kills: 14"), TEXT("Crew lost: Sniper"), TEXT("Time at sea: 2:05") })
			{
				TestTrue(*FString::Printf(TEXT("'%s' in:\n%s"), Expected, *All2), All2.Contains(Expected));
			}
			if (Stats.Captain)
			{
				TestTrue(TEXT("Captain name"), All2.Contains(Stats.Captain->DisplayName.ToString()));
			}
		});
	});

	Describe("Captain select", [this]()
	{
		It("lists the captains; Set Sail waits for a pick, then starts with that captain", [this]()
		{
			const TArray<const UDwCaptainDefinition*> All = Captains();
			if (!TestEqual(TEXT("Three captains"), All.Num(), 3)) { return; }
			UDwCaptainSelectViewModel* Select = Make<UDwCaptainSelectViewModel>();
			const UDwCaptainDefinition* Started = nullptr;
			int32 Backs = 0;
			Select->Initialize(All, [&Started](const UDwCaptainDefinition& C) { Started = &C; return true; }, [&Backs]() { ++Backs; return true; });

			TestEqual(TEXT("A card each"), Select->GetCaptains().Num(), 3);
			TestEqual(TEXT("Select order"), Select->GetCaptains()[0]->GetView().Title.ToString(), All[0]->DisplayName.ToString());
			TestFalse(TEXT("Set Sail disabled"), Select->GetSetSail()->GetView().bEnabled);
			TestTrue(TEXT("Says why"), Select->GetSetSail()->GetView().DisabledReason.ToString().Contains(TEXT("Pick a captain")));
			TestFalse(TEXT("Nothing starts"), Select->GetSetSail()->Select());
			TestNull(TEXT("Not started"), Started);

			TestTrue(TEXT("Pick the Conqueror"), Select->GetCaptains()[2]->Select());
			TestTrue(TEXT("Card highlighted"), Select->GetCaptains()[2]->GetView().bSelected && !Select->GetCaptains()[0]->GetView().bSelected);
			TestEqual(TEXT("Detail title"), Select->GetDetailTitle().ToString(), All[2]->DisplayName.ToString());
			TestEqual(TEXT("Description + two abilities"), Select->GetDetailLines().Num(), 3);
			TestTrue(TEXT("Ability line"), Select->GetDetailLines()[1].ToString().StartsWith(All[2]->CaptainAbilities[0]->DisplayName.ToString()));
			TestTrue(TEXT("Set Sail"), Select->GetSetSail()->Select());
			TestTrue(TEXT("Started with the Conqueror"), Started == All[2]);
			TestTrue(TEXT("Back"), Select->GetBack()->Select());
			TestEqual(TEXT("Back ran"), Backs, 1);
		});
	});

	Describe("Settings", [this]()
	{
		It("has every doc-16 row; each steps within its range and says when it can't", [this]()
		{
			using ERow = UDwSettingsViewModel::ERow;
			UDwGameUserSettings* Settings = MakeSettings();
			Settings->SetScreenResolution(FIntPoint(1920, 1080));
			Settings->SetFullscreenMode(EWindowMode::Windowed);
			UDwSettingsViewModel* View = Make<UDwSettingsViewModel>();
			int32 Backs = 0;
			View->Initialize(Settings, /*bApplyAndSave*/ false, { FIntPoint(2560, 1440), FIntPoint(1280, 720), FIntPoint(1920, 1080) },
				[&Backs]() { ++Backs; return true; });
			TestEqual(TEXT("Ten rows (U3)"), View->GetRows().Num(), static_cast<int32>(ERow::Count));

			// Volumes: 10% steps, stop at 0 and 100, the buttons at the ends disabled.
			UDwSettingRowViewModel* Master = View->GetRow(ERow::MasterVolume);
			TestEqual(TEXT("100%"), Master->GetValue().ToString(), FString(TEXT("100%")));
			TestFalse(TEXT("Can't go above 100"), Master->GetIncrease()->GetView().bEnabled || View->Step(ERow::MasterVolume, 1));
			TestTrue(TEXT("Down"), Master->GetDecrease()->Select());
			TestEqual(TEXT("90%"), Master->GetValue().ToString(), FString(TEXT("90%")));
			TestEqual(TEXT("Stored"), Settings->GetVolume(EDwVolume::Master), 0.9f, 1e-4f);
			TestFalse(TEXT("Volumes say they're not applied yet"), Master->GetNote().IsEmpty());

			// Game speed 1x -> 1.5x -> 2x, then stops.
			TestTrue(TEXT("Faster"), View->Step(ERow::GameSpeed, 1));
			TestEqual(TEXT("1.5x"), View->GetRow(ERow::GameSpeed)->GetValue().ToString(), FString(TEXT("1.5x")));
			TestTrue(TEXT("Fastest"), View->Step(ERow::GameSpeed, 1));
			TestFalse(TEXT("No faster"), View->Step(ERow::GameSpeed, 1));
			TestEqual(TEXT("Stored"), Settings->GetGameSpeed(), EDwGameSpeed::Fastest);

			// Toggles.
			TestTrue(TEXT("Shake off"), View->Step(ERow::ScreenShake, -1));
			TestFalse(TEXT("Off already"), View->Step(ERow::ScreenShake, -1));
			TestTrue(TEXT("Colourblind-safe"), View->Step(ERow::Highlights, 1));
			TestEqual(TEXT("Palette"), Settings->GetHighlightPalette(), EDwHighlightPalette::ColorblindSafe);
			TestTrue(TEXT("VSync on"), View->Step(ERow::VSync, 1) && Settings->IsVSyncEnabled());

			// Display: resolutions sorted small to big, window modes Fullscreen / Borderless / Windowed.
			TestTrue(TEXT("Up to 1440p"), View->Step(ERow::Resolution, 1));
			TestEqual(TEXT("1440p"), Settings->GetScreenResolution(), FIntPoint(2560, 1440));
			TestFalse(TEXT("Largest"), View->Step(ERow::Resolution, 1));
			TestTrue(TEXT("Down"), View->Step(ERow::Resolution, -1) && View->Step(ERow::Resolution, -1));
			TestEqual(TEXT("720p"), Settings->GetScreenResolution(), FIntPoint(1280, 720));
			TestEqual(TEXT("Windowed"), View->GetRow(ERow::WindowMode)->GetValue().ToString(), FString(TEXT("Windowed")));
			TestTrue(TEXT("To borderless"), View->Step(ERow::WindowMode, -1));
			TestEqual(TEXT("Borderless"), Settings->GetFullscreenMode(), EWindowMode::WindowedFullscreen);

			TestTrue(TEXT("Back"), View->GetBack()->Select());
			TestEqual(TEXT("Back ran"), Backs, 1);
		});
	});

#if WITH_EDITOR
	Describe("Content", [this]()
	{
		It("has the shell screens in DA_UIScreenSet and both levels in DA_GameData", [this]()
		{
			const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
			const UDwUIScreenSet* Screens = GameData ? GameData->UIScreenSet.Get() : nullptr;
			if (!TestNotNull(TEXT("DA_UIScreenSet"), Screens)) { return; }
			const TPair<const TSoftClassPtr<UDwActivatableScreen>*, UClass*> Expected[] = {
				{ &Screens->Menu, UDwMenuScreen::StaticClass() }, { &Screens->CaptainSelect, UDwCaptainSelectScreen::StaticClass() },
				{ &Screens->Settings, UDwSettingsScreen::StaticClass() } };
			for (const TPair<const TSoftClassPtr<UDwActivatableScreen>*, UClass*>& Entry : Expected)
			{
				const UClass* Class = Entry.Key->LoadSynchronous();
				TestTrue(*FString::Printf(TEXT("%s"), *GetNameSafe(Class)), Class && Class->IsChildOf(Entry.Value));
				const UDwRunScreen* Defaults = Class ? Class->GetDefaultObject<UDwRunScreen>() : nullptr;
				TestTrue(TEXT("Choice button class"), Defaults && Defaults->ChoiceButtonClass);
			}
			const UClass* SettingsClass = Screens->Settings.LoadSynchronous();
			TestTrue(TEXT("Settings rows"), SettingsClass && SettingsClass->GetDefaultObject<UDwSettingsScreen>()->RowClass);
			TestEqual(TEXT("Main menu level"), GameData->MainMenuLevel.GetLongPackageName(), FString(TEXT("/Game/DeadwaterRun/Maps/L_MainMenu")));
			TestEqual(TEXT("Sea level"), GameData->SeaLevel.GetLongPackageName(), FString(TEXT("/Game/DeadwaterRun/Maps/L_Sea")));
		});
	});
#endif
}

#endif // WITH_DEV_AUTOMATION_TESTS
