// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UI/DwRunScreens.h"
#include "UI/DwShellViewModels.h"
#include "DwShellScreens.generated.h"

class UPanelWidget;
class UTextBlock;

/*
 * Shell screens (Docs/Architecture/16-Shell.md, DW-210): WBP_Menu (main menu, pause, confirm, run summary), WBP_CaptainSelect,
 * WBP_Settings and WBP_SettingRow. Esc is the CommonUI back action: each screen forwards it to its view model, which
 * knows what it means there (back, resume, "No").
 */

/** WBP_Menu: one UDwMenuViewModel. */
UCLASS(Abstract)
class DEADWATERRUN_API UDwMenuScreen : public UDwRunScreen
{
	GENERATED_BODY()

public:
	UDwMenuScreen();

	/** One-line titles longer than LongTitleChars ("Quit Deadwater Run?") drop to LongTitleSize so they fit the page; 0 = never. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") int32 LongTitleSize = 0;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") int32 LongTitleChars = 12;

protected:
	virtual void NativeOnViewModelChanged() override;
	virtual bool NativeOnHandleBackAction() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> LineList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> ButtonList;

private:
	int32 TitleSize = 0; // the layout's own title size, cached on the first refresh
};

/** WBP_CaptainSelect. */
UCLASS(Abstract)
class DEADWATERRUN_API UDwCaptainSelectScreen : public UDwRunScreen
{
	GENERATED_BODY()

public:
	UDwCaptainSelectScreen();

protected:
	virtual void NativeOnViewModelChanged() override;
	virtual bool NativeOnHandleBackAction() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> CaptainList;
	/** Degrees each wanted poster leans, cycled. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") TArray<float> PosterTilts;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> DetailTitleText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> DetailList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> ButtonList;
};

/** WBP_SettingRow: label, < value >, note. */
UCLASS(Abstract)
class DEADWATERRUN_API UDwSettingRowWidget : public UDwUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnViewModelChanged() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> LabelText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ValueText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> NoteText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDwChoiceButtonWidget> DecreaseButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UDwChoiceButtonWidget> IncreaseButton;
};

/** WBP_Settings. */
UCLASS(Abstract)
class DEADWATERRUN_API UDwSettingsScreen : public UDwRunScreen
{
	GENERATED_BODY()

public:
	UDwSettingsScreen();

	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|UI") TSubclassOf<UDwSettingRowWidget> RowClass;

protected:
	virtual void NativeOnViewModelChanged() override;
	virtual bool NativeOnHandleBackAction() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> RowList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> ButtonList;
};
