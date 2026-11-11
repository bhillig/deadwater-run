// Copyright Brandon Hillig 2026

// UDwUIManagerSubsystem, shell part (Docs/Architecture/16-Shell.md, DW-210): main menu, captain select, settings,
// pause, confirm and the run summary. Screens are WBP_Menu / WBP_CaptainSelect / WBP_Settings from DA_UIScreenSet;
// the view models get their intents as functions from here, so navigation lives in one place.

#include "UI/DwUIManagerSubsystem.h"

#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Core/DwGameMode.h"
#include "Core/DwGameplayTags.h"
#include "Core/DwLog.h"
#include "Data/DwCaptainDefinition.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Run/DwRun.h"
#include "Run/DwRunSubsystem.h"
#include "Shell/DwGameUserSettings.h"
#include "UI/DwActivatableScreen.h"
#include "UI/DwPrimaryLayoutWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "UI/DwShellViewModels.h"
#include "UI/DwUIScreenSet.h"

#define LOCTEXT_NAMESPACE "DwShell"

UDwActivatableScreen* UDwUIManagerSubsystem::PushShellScreen(FGameplayTag LayerTag, const TSoftClassPtr<UDwActivatableScreen>& ScreenClass,
	UDwViewModelBase* ViewModel)
{
	const TSubclassOf<UDwActivatableScreen> Class = ScreenClass.LoadSynchronous();
	if (!Layout || !Class)
	{
		UE_LOG(LogDwUI, Warning, TEXT("No shell screen '%s' (run Tools/CreateUIContent.ps1)."), *ScreenClass.ToString());
		return nullptr;
	}
	UDwActivatableScreen* Screen = PushScreen(LayerTag, Class);
	if (Screen)
	{
		Screen->SetViewModel(ViewModel);
		ShellScreens.Add(Screen);
	}
	return Screen;
}

void UDwUIManagerSubsystem::CloseShellScreen(UDwActivatableScreen* Screen)
{
	if (!Screen || !ShellScreens.Contains(Screen))
	{
		return;
	}
	ShellScreens.Remove(Screen);
	PopScreen(Screen);
	if (Screen == PauseScreen)
	{
		PauseScreen = nullptr;
		SetPaused(false);
	}
}

void UDwUIManagerSubsystem::CloseAllShellScreens()
{
	// Clear whole layers: popping two screens from one stack in the same frame leaves the stack's transition showing the
	// first one removed (seen: captain select stuck over the new run's map). The shell layers only ever hold shell
	// screens; UI.Layer.Game (HUD, run screens) is never touched here.
	if (Layout)
	{
		const FGameplayTag ShellLayers[] = { DwTags::UI_Layer_Modal, DwTags::UI_Layer_Menu, DwTags::UI_Layer_GameMenu };
		for (const FGameplayTag& Tag : ShellLayers)
		{
			if (UCommonActivatableWidgetStack* Layer = Layout->GetLayer(Tag))
			{
				Layer->ClearWidgets();
			}
		}
	}
	ShellScreens.Reset();
	if (PauseScreen)
	{
		PauseScreen = nullptr;
		SetPaused(false);
	}
}

void UDwUIManagerSubsystem::SetPaused(bool bPaused)
{
	if (APlayerController* PC = PlayerController.Get())
	{
		UGameplayStatics::SetGamePaused(PC, bPaused);
	}
}

void UDwUIManagerSubsystem::ShowMainMenu()
{
	const UDwUIScreenSet* Screens = GetScreenSet();
	if (!Screens)
	{
		return;
	}
	CloseAllShellScreens();
	UDwMenuViewModel* Menu = NewObject<UDwMenuViewModel>(this);
	TArray<FDwMenuButton> Buttons;
	Buttons.Add({ LOCTEXT("NewRun", "New Run"), FText::GetEmpty(), [this]() { OpenCaptainSelect(); return true; } });
	Buttons.Add({ LOCTEXT("Settings", "Settings"), FText::GetEmpty(), [this]() { OpenSettings(); return true; } });
	Buttons.Add({ LOCTEXT("Quit", "Quit"), FText::GetEmpty(), [this]()
	{
		Confirm(LOCTEXT("QuitTitle", "Quit Deadwater Run?"), FText::GetEmpty(), LOCTEXT("QuitYes", "Quit"),
			[this]() { UKismetSystemLibrary::QuitGame(this, PlayerController.Get(), EQuitPreference::Quit, false); });
		return true;
	} });
	// No Esc on the main menu: Quit is a button, never an accidental key press.
	Menu->Set(LOCTEXT("GameTitle", "DEADWATER RUN"), { LOCTEXT("Tagline", "Pick a captain, sail the chart, and face the Kraken.") }, MoveTemp(Buttons));
	PushShellScreen(DwTags::UI_Layer_Menu, Screens->Menu, Menu);
}

void UDwUIManagerSubsystem::OpenCaptainSelect()
{
	const UDwUIScreenSet* Screens = GetScreenSet();
	const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
	if (!Screens || !GameData)
	{
		return;
	}
	UDwCaptainSelectViewModel* Select = NewObject<UDwCaptainSelectViewModel>(this);
	const TWeakObjectPtr<UDwActivatableScreen> Screen = PushShellScreen(DwTags::UI_Layer_Menu, Screens->CaptainSelect, Select);
	TArray<const UDwCaptainDefinition*> Captains;
	for (const UDwCaptainDefinition* Captain : GameData->Captains)
	{
		Captains.Add(Captain);
	}
	Select->Initialize(Captains,
		[this](const UDwCaptainDefinition& Captain)
		{
			// From the menu this travels to L_Sea; in L_Sea (summary -> New Run) HandleRunStarted closes the shell.
			UDwRunSubsystem* RunSubsystem = Runs.Get();
			return RunSubsystem && RunSubsystem->StartRun(&Captain) != nullptr;
		},
		[this, Screen]() { CloseShellScreen(Screen.Get()); return true; });
}

void UDwUIManagerSubsystem::OpenSettings()
{
	const UDwUIScreenSet* Screens = GetScreenSet();
	UDwGameUserSettings* Settings = UDwGameUserSettings::Get();
	if (!Screens || !Settings)
	{
		UE_LOG(LogDwUI, Warning, TEXT("No settings screen: GameUserSettingsClassName is not DwGameUserSettings."));
		return;
	}
	TArray<FIntPoint> Resolutions;
	if (!UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions))
	{
		UKismetSystemLibrary::GetConvenientWindowedResolutions(Resolutions);
	}
	UDwSettingsViewModel* View = NewObject<UDwSettingsViewModel>(this);
	const TWeakObjectPtr<UDwActivatableScreen> Screen = PushShellScreen(DwTags::UI_Layer_Menu, Screens->Settings, View);
	View->Initialize(Settings, /*bApplyAndSave*/ true, MoveTemp(Resolutions), [this, Screen]() { CloseShellScreen(Screen.Get()); return true; });
}

bool UDwUIManagerSubsystem::TogglePause()
{
	if (PauseScreen)
	{
		CloseShellScreen(PauseScreen);
		return true;
	}
	const UDwUIScreenSet* Screens = GetScreenSet();
	const UWorld* World = PlayerController.IsValid() ? PlayerController->GetWorld() : nullptr;
	if (!Screens || !Layout || !World || !Cast<ADwGameMode>(World->GetAuthGameMode()) || !ShellScreens.IsEmpty())
	{
		return false; // only L_Sea pauses; open shell screens handle Esc themselves
	}
	const UDwRun* Run = BoundRun.Get();
	const bool bInRun = Run && !Run->IsOver();
	UDwMenuViewModel* Pause = NewObject<UDwMenuViewModel>(this);
	TArray<FDwMenuButton> Buttons;
	Buttons.Add({ LOCTEXT("Resume", "Resume"), FText::GetEmpty(), [this]() { return TogglePause(); } });
	Buttons.Add({ LOCTEXT("PauseSettings", "Settings"), FText::GetEmpty(), [this]() { OpenSettings(); return true; } });
	FDwMenuButton Abandon;
	Abandon.Title = LOCTEXT("Abandon", "Abandon Run");
	Abandon.OnSelect = [this]()
	{
		Confirm(LOCTEXT("AbandonTitle", "Abandon this run?"), LOCTEXT("AbandonBody", "Your crew, ship and gold are lost. The voyage ends here."),
			LOCTEXT("AbandonYes", "Abandon"), [this]()
			{
				CloseAllShellScreens(); // unpauses
				if (UDwRunSubsystem* RunSubsystem = Runs.Get())
				{
					RunSubsystem->AbandonRun(); // Defeat (Abandoned) -> HandleRunPhaseChanged -> the summary (U5)
				}
			});
		return true;
	};
	Abandon.bEnabled = bInRun;
	Abandon.DisabledReason = bInRun ? FText::GetEmpty() : LOCTEXT("NoRun", "No run to abandon (dev fight).");
	Buttons.Add(MoveTemp(Abandon));
	Buttons.Add({ LOCTEXT("QuitDesktop", "Quit to Desktop"), FText::GetEmpty(), [this, bInRun]()
	{
		Confirm(LOCTEXT("QuitRunTitle", "Quit to desktop?"), bInRun ? LOCTEXT("QuitRunBody", "This run is lost: runs are never saved.") : FText::GetEmpty(),
			LOCTEXT("QuitRunYes", "Quit"), [this]() { UKismetSystemLibrary::QuitGame(this, PlayerController.Get(), EQuitPreference::Quit, false); });
		return true;
	} });
	Pause->Set(LOCTEXT("Paused", "Paused"), {}, MoveTemp(Buttons), [this]() { return TogglePause(); });
	PauseScreen = PushShellScreen(DwTags::UI_Layer_GameMenu, Screens->Menu, Pause);
	if (PauseScreen)
	{
		SetPaused(true);
	}
	return PauseScreen != nullptr;
}

void UDwUIManagerSubsystem::Confirm(const FText& Title, const FText& Body, const FText& YesLabel, TFunction<void()> OnYes)
{
	const UDwUIScreenSet* Screens = GetScreenSet();
	if (!Screens)
	{
		return;
	}
	UDwMenuViewModel* Question = NewObject<UDwMenuViewModel>(this);
	const TWeakObjectPtr<UDwActivatableScreen> Screen = PushShellScreen(DwTags::UI_Layer_Modal, Screens->Menu, Question);
	TArray<FDwMenuButton> Buttons;
	Buttons.Add({ YesLabel, FText::GetEmpty(), [this, Screen, OnYes]()
	{
		CloseShellScreen(Screen.Get());
		if (OnYes)
		{
			OnYes();
		}
		return true;
	} });
	Buttons.Add({ LOCTEXT("No", "No"), FText::GetEmpty(), [this, Screen]() { CloseShellScreen(Screen.Get()); return true; } });
	TArray<FText> Lines;
	if (!Body.IsEmpty())
	{
		Lines.Add(Body);
	}
	Question->Set(Title, MoveTemp(Lines), MoveTemp(Buttons), [this, Screen]() { CloseShellScreen(Screen.Get()); return true; });
}

void UDwUIManagerSubsystem::ShowRunSummary()
{
	const UDwUIScreenSet* Screens = GetScreenSet();
	const UDwRun* Run = BoundRun.Get();
	if (!Screens || !Run)
	{
		return;
	}
	CloseAllShellScreens();
	const FDwRunStats& Stats = Run->GetState().Stats;
	UDwMenuViewModel* Summary = NewObject<UDwMenuViewModel>(this);
	TArray<FDwMenuButton> Buttons;
	Buttons.Add({ LOCTEXT("SummaryNewRun", "New Run"), FText::GetEmpty(), [this]() { OpenCaptainSelect(); return true; } });
	Buttons.Add({ LOCTEXT("MainMenu", "Main Menu"), FText::GetEmpty(), [this]()
	{
		const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
		if (!GameData || GameData->MainMenuLevel.IsNull() || !PlayerController.IsValid())
		{
			UE_LOG(LogDwUI, Error, TEXT("Main Menu: DA_GameData.MainMenuLevel is not set."));
			return false;
		}
		if (UDwRunSubsystem* RunSubsystem = Runs.Get())
		{
			RunSubsystem->ClearRun(); // the summary data is no longer needed (16-Shell "EndRun")
		}
		UGameplayStatics::OpenLevelBySoftObjectPtr(PlayerController.Get(), GameData->MainMenuLevel);
		return true;
	} });
	// No Esc on the summary: the player chooses where to go.
	Summary->Set(DwShell::Headline(Stats.EndReason), DwShell::SummaryLines(Stats), MoveTemp(Buttons));
	PushShellScreen(DwTags::UI_Layer_Menu, Screens->Menu, Summary);
}

#undef LOCTEXT_NAMESPACE
