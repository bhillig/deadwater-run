// Copyright Brandon Hillig 2026

#include "UI/DwShellViewModels.h"

#include "Data/DwAbilityDefinition.h"
#include "Data/DwCaptainDefinition.h"
#include "Shell/DwGameUserSettings.h"

#define LOCTEXT_NAMESPACE "DwShellScreens"

// ---------------------------------------------------------------------------------------------------------------------
// Menu

void UDwMenuViewModel::Set(const FText& InTitle, TArray<FText> InLines, TArray<FDwMenuButton> InButtons, TFunction<bool()> InOnBack)
{
	Title = InTitle;
	Lines = MoveTemp(InLines);
	OnBack = MoveTemp(InOnBack);
	while (Buttons.Num() < InButtons.Num())
	{
		Buttons.Add(NewObject<UDwChoiceViewModel>(this));
	}
	Buttons.SetNum(InButtons.Num());
	for (int32 Index = 0; Index < InButtons.Num(); ++Index)
	{
		FDwMenuButton& Button = InButtons[Index];
		FDwChoiceView View;
		View.Title = Button.Title;
		View.Body = Button.Body;
		View.bEnabled = Button.bEnabled;
		View.DisabledReason = Button.DisabledReason;
		Buttons[Index]->Set(View, MoveTemp(Button.OnSelect));
	}
	NotifyChanged();
}

UDwChoiceViewModel* UDwMenuViewModel::FindButton(const FString& InTitle) const
{
	const TObjectPtr<UDwChoiceViewModel>* Found = Buttons.FindByPredicate([&InTitle](const UDwChoiceViewModel* Button)
		{ return Button && Button->GetView().Title.ToString() == InTitle; });
	return Found ? Found->Get() : nullptr;
}

bool UDwMenuViewModel::HandleBack()
{
	if (!OnBack)
	{
		return false;
	}
	const TFunction<bool()> Intent = OnBack; // the intent may Set() this view model again
	return Intent();
}

namespace DwShell
{
	FText Headline(EDwRunEndReason Reason)
	{
		switch (Reason)
		{
		case EDwRunEndReason::Victory:     return LOCTEXT("Victory", "Victory! The Kraken is slain");
		case EDwRunEndReason::ShipSunk:    return LOCTEXT("ShipSunk", "Your ship went down");
		case EDwRunEndReason::CrewDead:    return LOCTEXT("CrewDead", "Your crew is lost");
		case EDwRunEndReason::CaptainDead: return LOCTEXT("CaptainDead", "Your captain has fallen");
		case EDwRunEndReason::Abandoned:   return LOCTEXT("Abandoned", "Run abandoned");
		default:                           return LOCTEXT("Over", "The voyage is over");
		}
	}

	TArray<FText> SummaryLines(const FDwRunStats& Stats)
	{
		TArray<FText> Lines;
		const int32 Seconds = FMath::RoundToInt(Stats.ElapsedSeconds);
		Lines.Add(FText::Format(LOCTEXT("Captain", "Captain: {0}    Seed: {1}"),
			Stats.Captain ? Stats.Captain->DisplayName : LOCTEXT("NoCaptain", "-"), FText::AsNumber(Stats.Seed, &FNumberFormattingOptions::DefaultNoGrouping())));
		Lines.Add(FText::Format(LOCTEXT("Stops", "Stops cleared: {0}"), Stats.NodesCleared));
		Lines.Add(FText::Format(LOCTEXT("Fights", "Ships sunk: {0}    Crews plundered: {1}    Surrenders: {2}"), Stats.FightsSunk,
			Stats.FightsPlundered, Stats.FightsSurrendered));
		Lines.Add(FText::Format(LOCTEXT("Gold", "Gold earned: {0}    Kills: {1}"), Stats.GoldEarned, Stats.Kills));
		if (Stats.CrewLost.IsEmpty())
		{
			Lines.Add(LOCTEXT("NoLoss", "Crew lost: none"));
		}
		else
		{
			TArray<FString> Names;
			for (const FDwCrewLoss& Loss : Stats.CrewLost)
			{
				Names.Add(Loss.Name.ToString());
			}
			Lines.Add(FText::Format(LOCTEXT("Lost", "Crew lost: {0}"), FText::FromString(FString::Join(Names, TEXT(", ")))));
		}
		Lines.Add(FText::Format(LOCTEXT("Time", "Time at sea: {0}:{1}"), Seconds / 60,
			FText::FromString(FString::Printf(TEXT("%02d"), Seconds % 60))));
		return Lines;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Captain select

void UDwCaptainSelectViewModel::Initialize(TArray<const UDwCaptainDefinition*> InCaptains, TFunction<bool(const UDwCaptainDefinition&)> InOnSetSail,
	TFunction<bool()> InOnBack)
{
	Captains.Reset();
	for (const UDwCaptainDefinition* Captain : InCaptains)
	{
		if (Captain)
		{
			Captains.Add(Captain);
		}
	}
	OnSetSail = MoveTemp(InOnSetSail);
	OnBack = MoveTemp(InOnBack);
	Selected = INDEX_NONE;
	Refresh();
}

bool UDwCaptainSelectViewModel::SelectCaptain(int32 Index)
{
	if (!Captains.IsValidIndex(Index))
	{
		return false;
	}
	Selected = Index;
	Refresh();
	return true;
}

void UDwCaptainSelectViewModel::Refresh()
{
	while (CaptainChoices.Num() < Captains.Num())
	{
		CaptainChoices.Add(NewObject<UDwChoiceViewModel>(this));
	}
	CaptainChoices.SetNum(Captains.Num());
	for (int32 Index = 0; Index < Captains.Num(); ++Index)
	{
		FDwChoiceView View;
		View.Title = Captains[Index]->DisplayName;
		TArray<FString> Abilities;
		for (const UDwAbilityDefinition* Ability : Captains[Index]->CaptainAbilities)
		{
			Abilities.Add(Ability ? Ability->DisplayName.ToString() : FString());
		}
		View.Body = FText::FromString(FString::Join(Abilities, TEXT(", ")));
		View.bSelected = Index == Selected;
		CaptainChoices[Index]->Set(View, [this, Index]() { return SelectCaptain(Index); });
	}

	DetailLines.Reset();
	const UDwCaptainDefinition* Captain = GetSelected();
	if (Captain)
	{
		DetailTitle = Captain->DisplayName;
		DetailLines.Add(Captain->Description);
		for (const UDwAbilityDefinition* Ability : Captain->CaptainAbilities)
		{
			if (Ability)
			{
				DetailLines.Add(FText::Format(LOCTEXT("Ability", "{0}: {1}"), Ability->DisplayName, Ability->Description));
			}
		}
	}
	else
	{
		DetailTitle = LOCTEXT("Choose", "Choose your captain");
		DetailLines.Add(LOCTEXT("ChooseBody", "Each captain fights with two abilities of their own and changes the rules of the voyage."));
	}

	if (!SetSail)
	{
		SetSail = NewObject<UDwChoiceViewModel>(this);
		Back = NewObject<UDwChoiceViewModel>(this);
	}
	FDwChoiceView Sail;
	Sail.Title = LOCTEXT("SetSail", "Set Sail");
	Sail.bEnabled = Captain != nullptr;
	Sail.DisabledReason = Captain ? FText::GetEmpty() : LOCTEXT("PickFirst", "Pick a captain first.");
	SetSail->Set(Sail, [this]() { const UDwCaptainDefinition* C = GetSelected(); return C && OnSetSail && OnSetSail(*C); });
	FDwChoiceView BackView;
	BackView.Title = LOCTEXT("Back", "Back");
	Back->Set(BackView, [this]() { return HandleBack(); });
	NotifyChanged();
}

// ---------------------------------------------------------------------------------------------------------------------
// Settings

void UDwSettingRowViewModel::Set(const FText& InLabel, const FText& InValue, const FText& InNote, bool bCanDecrease, bool bCanIncrease,
	TFunction<bool(int32)> InOnStep)
{
	Label = InLabel;
	Value = InValue;
	Note = InNote;
	if (!Decrease)
	{
		Decrease = NewObject<UDwChoiceViewModel>(this);
		Increase = NewObject<UDwChoiceViewModel>(this);
	}
	FDwChoiceView Left;
	Left.Title = LOCTEXT("Left", "<");
	Left.bEnabled = bCanDecrease;
	FDwChoiceView Right;
	Right.Title = LOCTEXT("Right", ">");
	Right.bEnabled = bCanIncrease;
	Decrease->Set(Left, [InOnStep]() { return InOnStep(-1); });
	Increase->Set(Right, [InOnStep]() { return InOnStep(1); });
	NotifyChanged();
}

void UDwSettingsViewModel::Initialize(UDwGameUserSettings* InSettings, bool bInApplyAndSave, TArray<FIntPoint> InResolutions, TFunction<bool()> InOnBack)
{
	Settings = InSettings;
	bApplyAndSave = bInApplyAndSave;
	Resolutions = MoveTemp(InResolutions);
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X * A.Y < B.X * B.Y || (A.X * A.Y == B.X * B.Y && A.X < B.X); });
	OnBack = MoveTemp(InOnBack);
	Refresh();
}

namespace DwSettingsRows
{
	constexpr float VolumeStep = 0.1f;

	EDwVolume VolumeOf(UDwSettingsViewModel::ERow Row)
	{
		using ERow = UDwSettingsViewModel::ERow;
		return Row == ERow::MusicVolume ? EDwVolume::Music : Row == ERow::SfxVolume ? EDwVolume::Sfx : Row == ERow::UiVolume ? EDwVolume::Ui : EDwVolume::Master;
	}

	const TArray<EWindowMode::Type>& WindowModes()
	{
		static const TArray<EWindowMode::Type> Modes = { EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed };
		return Modes;
	}

	FText WindowModeName(EWindowMode::Type Mode)
	{
		switch (Mode)
		{
		case EWindowMode::Fullscreen:         return LOCTEXT("Fullscreen", "Fullscreen");
		case EWindowMode::WindowedFullscreen: return LOCTEXT("Borderless", "Borderless window");
		default:                              return LOCTEXT("Windowed", "Windowed");
		}
	}
}

bool UDwSettingsViewModel::Step(ERow Row, int32 Direction)
{
	using namespace DwSettingsRows;
	if (!Settings || (Direction != 1 && Direction != -1))
	{
		return false;
	}
	bool bChanged = false;
	bool bDisplay = false;
	switch (Row)
	{
	case ERow::Resolution:
	{
		const int32 Current = Resolutions.IndexOfByKey(Settings->GetScreenResolution());
		const int32 Next = Current == INDEX_NONE ? (Resolutions.IsEmpty() ? INDEX_NONE : Resolutions.Num() - 1) : Current + Direction;
		if (Resolutions.IsValidIndex(Next) && Next != Current)
		{
			Settings->SetScreenResolution(Resolutions[Next]);
			bChanged = bDisplay = true;
		}
		break;
	}
	case ERow::WindowMode:
	{
		const int32 Current = WindowModes().IndexOfByKey(Settings->GetFullscreenMode());
		const int32 Next = Current + Direction;
		if (WindowModes().IsValidIndex(Next))
		{
			Settings->SetFullscreenMode(WindowModes()[Next]);
			bChanged = bDisplay = true;
		}
		break;
	}
	case ERow::VSync:
		if (Settings->IsVSyncEnabled() != (Direction > 0))
		{
			Settings->SetVSyncEnabled(Direction > 0);
			bChanged = bDisplay = true;
		}
		break;
	case ERow::MasterVolume:
	case ERow::MusicVolume:
	case ERow::SfxVolume:
	case ERow::UiVolume:
	{
		const float Old = Settings->GetVolume(VolumeOf(Row));
		const float New = FMath::Clamp(FMath::RoundToFloat((Old + Direction * VolumeStep) * 10.f) / 10.f, 0.f, 1.f);
		if (!FMath::IsNearlyEqual(Old, New))
		{
			Settings->SetVolume(VolumeOf(Row), New);
			bChanged = true;
		}
		break;
	}
	case ERow::GameSpeed:
	{
		const int32 Next = static_cast<int32>(Settings->GetGameSpeed()) + Direction;
		if (Next >= 0 && Next <= static_cast<int32>(EDwGameSpeed::Fastest))
		{
			Settings->SetGameSpeed(static_cast<EDwGameSpeed>(Next));
			bChanged = true;
		}
		break;
	}
	case ERow::ScreenShake:
		if (Settings->IsScreenShakeEnabled() != (Direction > 0))
		{
			Settings->SetScreenShakeEnabled(Direction > 0);
			bChanged = true;
		}
		break;
	case ERow::Highlights:
	{
		const EDwHighlightPalette Want = Direction > 0 ? EDwHighlightPalette::ColorblindSafe : EDwHighlightPalette::Default;
		if (Settings->GetHighlightPalette() != Want)
		{
			Settings->SetHighlightPalette(Want);
			bChanged = true;
		}
		break;
	}
	default:
		break;
	}
	if (bChanged)
	{
		Commit(bDisplay);
		Refresh();
	}
	return bChanged;
}

void UDwSettingsViewModel::Commit(bool bDisplayChanged)
{
	if (!bApplyAndSave || !Settings)
	{
		return;
	}
	if (bDisplayChanged)
	{
		Settings->ApplyResolutionSettings(/*bCheckForCommandLineOverrides*/ false);
	}
	Settings->ApplyPresentation();
	Settings->SaveSettings();
}

void UDwSettingsViewModel::Refresh()
{
	using namespace DwSettingsRows;
	if (!Back)
	{
		Back = NewObject<UDwChoiceViewModel>(this);
	}
	FDwChoiceView BackView;
	BackView.Title = LOCTEXT("SettingsBack", "Back");
	Back->Set(BackView, [this]() { return HandleBack(); });
	if (!Settings)
	{
		NotifyChanged();
		return;
	}
	while (Rows.Num() < static_cast<int32>(ERow::Count))
	{
		Rows.Add(NewObject<UDwSettingRowViewModel>(this));
	}
	auto StepFor = [this](ERow Row) { return [this, Row](int32 Direction) { return Step(Row, Direction); }; };
	auto OnOff = [](bool bOn) { return bOn ? LOCTEXT("On", "On") : LOCTEXT("Off", "Off"); };
	const FText Pending = LOCTEXT("NoAudio", "Saved; takes effect when the game has sound.");

	const FIntPoint Resolution = Settings->GetScreenResolution();
	const int32 ResIndex = Resolutions.IndexOfByKey(Resolution);
	GetRow(ERow::Resolution)->Set(LOCTEXT("Resolution", "Resolution"), FText::Format(LOCTEXT("ResValue", "{0} x {1}"),
		FText::AsNumber(Resolution.X, &FNumberFormattingOptions::DefaultNoGrouping()), FText::AsNumber(Resolution.Y, &FNumberFormattingOptions::DefaultNoGrouping())),
		FText::GetEmpty(), ResIndex > 0 || (ResIndex == INDEX_NONE && !Resolutions.IsEmpty()), ResIndex != INDEX_NONE && ResIndex < Resolutions.Num() - 1, StepFor(ERow::Resolution));

	const int32 Mode = WindowModes().IndexOfByKey(Settings->GetFullscreenMode());
	GetRow(ERow::WindowMode)->Set(LOCTEXT("WindowMode", "Window mode"), WindowModeName(Settings->GetFullscreenMode()), FText::GetEmpty(),
		Mode > 0, Mode < WindowModes().Num() - 1, StepFor(ERow::WindowMode));
	GetRow(ERow::VSync)->Set(LOCTEXT("VSync", "VSync"), OnOff(Settings->IsVSyncEnabled()), FText::GetEmpty(), Settings->IsVSyncEnabled(),
		!Settings->IsVSyncEnabled(), StepFor(ERow::VSync));

	const TPair<ERow, FText> Volumes[] = { { ERow::MasterVolume, LOCTEXT("Master", "Master volume") }, { ERow::MusicVolume, LOCTEXT("Music", "Music volume") },
		{ ERow::SfxVolume, LOCTEXT("Sfx", "Effects volume") }, { ERow::UiVolume, LOCTEXT("Ui", "Interface volume") } };
	for (const TPair<ERow, FText>& Volume : Volumes)
	{
		const float Value = Settings->GetVolume(VolumeOf(Volume.Key));
		GetRow(Volume.Key)->Set(Volume.Value, FText::Format(LOCTEXT("Percent", "{0}%"), FMath::RoundToInt(Value * 100.f)), Pending,
			Value > 0.001f, Value < 0.999f, StepFor(Volume.Key));
	}

	const EDwGameSpeed Speed = Settings->GetGameSpeed();
	GetRow(ERow::GameSpeed)->Set(LOCTEXT("GameSpeed", "Game speed"), FText::Format(LOCTEXT("Speed", "{0}x"),
		FText::AsNumber(UDwGameUserSettings::GetPlaybackRate(Speed))), LOCTEXT("SpeedNote", "Animations only; the rules never change."),
		Speed != EDwGameSpeed::Normal, Speed != EDwGameSpeed::Fastest, StepFor(ERow::GameSpeed));
	GetRow(ERow::ScreenShake)->Set(LOCTEXT("Shake", "Screen shake"), OnOff(Settings->IsScreenShakeEnabled()), LOCTEXT("ShakeNote", "Saved; the camera doesn't shake yet."),
		Settings->IsScreenShakeEnabled(), !Settings->IsScreenShakeEnabled(), StepFor(ERow::ScreenShake));
	const bool bSafe = Settings->GetHighlightPalette() == EDwHighlightPalette::ColorblindSafe;
	GetRow(ERow::Highlights)->Set(LOCTEXT("Highlights", "Highlight colours"), bSafe ? LOCTEXT("Safe", "Colourblind-safe") : LOCTEXT("Default", "Default"),
		LOCTEXT("PaletteNote", "Saved; the colourblind-safe palette arrives with DW-216."), bSafe, !bSafe, StepFor(ERow::Highlights));
	NotifyChanged();
}

#undef LOCTEXT_NAMESPACE
