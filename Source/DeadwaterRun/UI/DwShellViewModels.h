// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Run/DwRunTypes.h"
#include "UI/DwRunViewModels.h"
#include "DwShellViewModels.generated.h"

class UDwCaptainDefinition;
class UDwGameUserSettings;
class UWorld;

/*
 * View models of the shell screens (Docs/Architecture/16-Shell.md, DW-210): main menu, pause, confirm, run summary
 * (all a UDwMenuViewModel: title, lines, buttons, what Esc does), captain select and settings. Navigation between
 * screens is the UI manager's: it hands each view model the intents its buttons send (TFunctions), so the view models
 * stay testable without widgets, levels or a game instance.
 */

/** One button of a menu: its text and what it does. */
struct DEADWATERRUN_API FDwMenuButton
{
	FText Title;
	FText Body;
	TFunction<bool()> OnSelect;
	bool bEnabled = true;
	FText DisabledReason;
};

/** WBP_Menu: a title, lines of text and a column of buttons; Esc runs OnBack (none = Esc does nothing). */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwMenuViewModel : public UDwViewModelBase
{
	GENERATED_BODY()

public:
	void Set(const FText& InTitle, TArray<FText> InLines, TArray<FDwMenuButton> InButtons, TFunction<bool()> InOnBack = nullptr);

	FText GetTitle() const { return Title; }
	const TArray<FText>& GetLines() const { return Lines; }
	const TArray<TObjectPtr<UDwChoiceViewModel>>& GetButtons() const { return Buttons; }
	/** The button titled Title, or null (tests, demos). */
	UDwChoiceViewModel* FindButton(const FString& InTitle) const;

	/** Esc. False when this screen ignores Esc. */
	bool HandleBack();
	bool HasBack() const { return static_cast<bool>(OnBack); }

private:
	FText Title;
	TArray<FText> Lines;
	TFunction<bool()> OnBack;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwChoiceViewModel>> Buttons;
};

namespace DwShell
{
	/** The summary's headline for an end reason (U4): "Victory!", "Your ship was sunk", ..., "Run abandoned". */
	DEADWATERRUN_API FText Headline(EDwRunEndReason Reason);
	/** The summary's lines from FDwRunStats (U4): captain, seed, stops, fights by outcome, gold, crew lost, kills, time. */
	DEADWATERRUN_API TArray<FText> SummaryLines(const FDwRunStats& Stats);
}

// ---------------------------------------------------------------------------------------------------------------------
// Captain select

/** WBP_CaptainSelect: a card per captain, the chosen one's rules and abilities, Set Sail and Back. */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwCaptainSelectViewModel : public UDwViewModelBase
{
	GENERATED_BODY()

public:
	/** Captains in select order (DA_GameData.Captains); OnSetSail starts the run (UDwRunSubsystem::StartRun). */
	void Initialize(TArray<const UDwCaptainDefinition*> InCaptains, TFunction<bool(const UDwCaptainDefinition&)> InOnSetSail, TFunction<bool()> InOnBack);

	const TArray<TObjectPtr<UDwChoiceViewModel>>& GetCaptains() const { return CaptainChoices; }
	FText GetDetailTitle() const { return DetailTitle; }
	/** The selected captain's description, then one line per captain ability. */
	const TArray<FText>& GetDetailLines() const { return DetailLines; }
	UDwChoiceViewModel* GetSetSail() const { return SetSail; }
	UDwChoiceViewModel* GetBack() const { return Back; }
	const UDwCaptainDefinition* GetSelected() const { return Captains.IsValidIndex(Selected) ? Captains[Selected].Get() : nullptr; }

	bool SelectCaptain(int32 Index);
	bool HandleBack() { return OnBack && OnBack(); }

private:
	void Refresh();

	UPROPERTY(Transient) TArray<TObjectPtr<const UDwCaptainDefinition>> Captains;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwChoiceViewModel>> CaptainChoices;
	UPROPERTY(Transient) TObjectPtr<UDwChoiceViewModel> SetSail;
	UPROPERTY(Transient) TObjectPtr<UDwChoiceViewModel> Back;
	int32 Selected = INDEX_NONE;
	FText DetailTitle;
	TArray<FText> DetailLines;
	TFunction<bool(const UDwCaptainDefinition&)> OnSetSail;
	TFunction<bool()> OnBack;
};

// ---------------------------------------------------------------------------------------------------------------------
// Settings

/** One settings row: label, current value, and the step left / right buttons (each with a reason at the ends). */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwSettingRowViewModel : public UDwViewModelBase
{
	GENERATED_BODY()

public:
	FText GetLabel() const { return Label; }
	FText GetValue() const { return Value; }
	/** "Not applied until audio arrives" and the like; empty when the setting takes effect now. */
	FText GetNote() const { return Note; }
	UDwChoiceViewModel* GetDecrease() const { return Decrease; }
	UDwChoiceViewModel* GetIncrease() const { return Increase; }

	void Set(const FText& InLabel, const FText& InValue, const FText& InNote, bool bCanDecrease, bool bCanIncrease, TFunction<bool(int32)> InOnStep);

private:
	FText Label;
	FText Value;
	FText Note;
	UPROPERTY(Transient) TObjectPtr<UDwChoiceViewModel> Decrease;
	UPROPERTY(Transient) TObjectPtr<UDwChoiceViewModel> Increase;
};

/** WBP_Settings (16-Shell "Settings list", U3). Every change is applied and saved at once. */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwSettingsViewModel : public UDwViewModelBase
{
	GENERATED_BODY()

public:
	enum class ERow : uint8 { Resolution, WindowMode, VSync, MasterVolume, MusicVolume, SfxVolume, UiVolume, GameSpeed, ScreenShake, Highlights, Count };

	/**
	 * Settings = the object to edit (the engine's in the game, a transient one in tests). bApplyAndSave: apply display
	 * and presentation and write the ini on every change (off in tests). Resolutions: the choices for the Resolution
	 * row (the monitor's in the game).
	 */
	void Initialize(UDwGameUserSettings* InSettings, bool bInApplyAndSave, TArray<FIntPoint> InResolutions, TFunction<bool()> InOnBack);

	const TArray<TObjectPtr<UDwSettingRowViewModel>>& GetRows() const { return Rows; }
	UDwSettingRowViewModel* GetRow(ERow Row) const { return Rows.IsValidIndex(static_cast<int32>(Row)) ? Rows[static_cast<int32>(Row)].Get() : nullptr; }
	UDwChoiceViewModel* GetBack() const { return Back; }
	bool HandleBack() { return OnBack && OnBack(); }

	/** Intent: move Row one step (-1 / +1). False at the ends (nothing changes). */
	bool Step(ERow Row, int32 Direction);

private:
	void Refresh();
	void Commit(bool bDisplayChanged);

	UPROPERTY(Transient) TObjectPtr<UDwGameUserSettings> Settings;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwSettingRowViewModel>> Rows;
	UPROPERTY(Transient) TObjectPtr<UDwChoiceViewModel> Back;
	TArray<FIntPoint> Resolutions;
	bool bApplyAndSave = false;
	TFunction<bool()> OnBack;
};
