// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Core/DwTypes.h"
#include "UI/DwActivatableScreen.h"
#include "UI/DwCombatViewModels.h"
#include "UI/DwUserWidget.h"
#include "DwCombatHudWidgets.generated.h"

class UBorder;
class UButton;
class UHorizontalBox;
class UPanelWidget;
class UProgressBar;
class UTextBlock;
class UVerticalBox;

/**
 * WBP_UnitCard: a crew card (left column) or an enemy bar (right column) for one UDwUnitViewModel.
 * Name, captain/acting/down marker, HP and MP bars with numbers, move/action left, statuses with turns left,
 * enemy Fear as pips out of MaxFear (Docs/Architecture/14 "Readability rules"). Clicking a crew card selects it.
 * Juice: Anim_Hover / Anim_Press / Anim_Highlight (selected), OnHealthChanged(Old, New).
 */
UCLASS(Abstract)
class DEADWATERRUN_API UDwUnitCardWidget : public UDwUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnViewModelChanged() override;

	/** Juice hook: Health changed (damage when New < Old). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Deadwater|UI|Juice")
	void OnHealthChanged(float OldHealth, float NewHealth);

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> CardButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> Frame;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> RoleText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> BudgetText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> HealthBar;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> HealthText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> ManaBar;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ManaText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> FearRow;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UHorizontalBox> FearPips;
	/** Any panel (a wrap box in WBP_UnitCard, so many statuses wrap instead of clipping). */
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> StatusList;

	// Placeholder palette (art direction U6 pending; side colours move to DA_HighlightPalette_* with DW-216).
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor AllyColor = FLinearColor(0.25f, 0.55f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor EnemyColor = FLinearColor(0.95f, 0.3f, 0.25f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor ManaColor = FLinearColor(0.35f, 0.6f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor FrameColor = FLinearColor(0.02f, 0.03f, 0.05f, 0.82f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor SelectedFrameColor = FLinearColor(1.f, 0.78f, 0.2f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor HoveredFrameColor = FLinearColor(0.85f, 0.85f, 0.9f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor ActingFrameColor = FLinearColor(1.f, 0.3f, 0.2f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor FearOnColor = FLinearColor(0.75f, 0.45f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor FearOffColor = FLinearColor(0.2f, 0.2f, 0.25f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") int32 StatusFontSize = 14;

private:
	UFUNCTION() void HandleClicked();
	UFUNCTION() void HandleHovered();

	float LastHealth = -1.f;
	bool bWasSelected = false;
};

/**
 * WBP_ActionButton: one action bar slot for a UDwActionViewModel - hotkey, name, MP cost, items left, and why it is
 * disabled (always shown, Docs/Architecture/14 "Disabled actions always show why"). Click = same as the hotkey.
 * Juice: Anim_Hover / Anim_Press / Anim_Disabled / Anim_Highlight (armed); UI.Sound.Hover / Click / Error.
 */
UCLASS(Abstract)
class DEADWATERRUN_API UDwActionButtonWidget : public UDwUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnViewModelChanged() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ActionButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> HotkeyText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CostText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ReasonText;

	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor ReadyColor = FLinearColor(0.12f, 0.16f, 0.22f, 0.95f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor ArmedColor = FLinearColor(0.75f, 0.58f, 0.12f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor DisabledColor = FLinearColor(0.08f, 0.08f, 0.09f, 0.9f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor NameColor = FLinearColor(0.95f, 0.92f, 0.85f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor DisabledNameColor = FLinearColor(0.55f, 0.55f, 0.58f);

private:
	UFUNCTION() void HandleClicked();
	UFUNCTION() void HandleHovered();

	bool bWasEnabled = true;
	bool bWasArmed = false;
};

/**
 * WBP_CombatHud on UI.Layer.Game (Docs/Architecture/14 "Screens"): both ship bars with numbers, crew cards, enemy bars,
 * the selected pirate's action bar, End Turn with the non-modal "End turn?" prompt (C14), the hint / damage preview /
 * message lines (C13), the turn banner on every side change and the outcome banner.
 * Input mode All: the board, hotkeys and Space keep going to Enhanced Input; HUD buttons take only their own clicks.
 * Juice: Anim_Intro / Anim_Outro, OnTurnBannerShown(Side), OnShipHealthChanged(Side, Old, New).
 */
UCLASS(Abstract)
class DEADWATERRUN_API UDwCombatHudScreen : public UDwActivatableScreen
{
	GENERATED_BODY()

public:
	UDwCombatHudScreen(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|HUD") TSubclassOf<UDwUnitCardWidget> UnitCardClass;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|HUD") TSubclassOf<UDwActionButtonWidget> ActionButtonClass;
	/** Gap between crew cards, enemy bars and action buttons (Slate units). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|HUD") float EntrySpacing = 8.f;
	/** How long the turn banner stays up. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|HUD", meta = (ClampMin = "0.1")) float TurnBannerSeconds = 1.5f;

	bool IsTurnBannerVisible() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnViewModelChanged() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Deadwater|UI|Juice")
	void OnTurnBannerShown(EDwSide Side);

	UFUNCTION(BlueprintImplementableEvent, Category = "Deadwater|UI|Juice")
	void OnShipHealthChanged(EDwSide Side, float OldHealth, float NewHealth);

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TurnText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> PlayerShipBar;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> PlayerShipText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> EnemyShipPanel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> EnemyShipBar;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> EnemyShipText;
	/** Any panel: the layout decides whether cards stack in a row or a column. */
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> CrewList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> EnemyList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UHorizontalBox> ActionBar;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> EndTurnButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> EndTurnText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> EndTurnKeyText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> EndTurnPrompt;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> HintText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UVerticalBox> PreviewList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> TurnBanner;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TurnBannerText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> OutcomeBanner;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> OutcomeText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ControlsText;

	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor AllyColor = FLinearColor(0.25f, 0.55f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor EnemyColor = FLinearColor(0.95f, 0.3f, 0.25f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor PreviewColor = FLinearColor(0.55f, 1.f, 0.55f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor LethalColor = FLinearColor(1.f, 0.35f, 0.3f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor EndTurnColor = FLinearColor(0.16f, 0.2f, 0.27f, 0.95f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor EndTurnSuggestedColor = FLinearColor(0.75f, 0.58f, 0.12f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") int32 PreviewFontSize = 20;

private:
	UFUNCTION() void HandleEndTurnClicked();
	UFUNCTION() void HandleTurnBanner(EDwSide Side, int32 Round);

	template <typename TWidget, typename TViewModel>
	void SyncList(UPanelWidget& Panel, TSubclassOf<TWidget> EntryClass, const TArray<TObjectPtr<TViewModel>>& ViewModels);
	void RefreshShips(const UDwCombatHudViewModel& Hud);
	void RefreshPrompt(const UDwCombatHudViewModel& Hud);
	void HideTurnBanner();

	TWeakObjectPtr<UDwCombatHudViewModel> BoundHud;
	FTimerHandle BannerTimer;
	float LastShipHealth[2] = { -1.f, -1.f };
};
