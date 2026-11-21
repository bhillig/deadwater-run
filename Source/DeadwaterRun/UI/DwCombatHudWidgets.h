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
class UImage;
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
	/** The unit's face (FDwUnitView::Portrait). */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Portrait;

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
	/** HP bar fill per side (names keep AllyColor / EnemyColor). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor AllyBarColor = FLinearColor(0.25f, 0.55f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor EnemyBarColor = FLinearColor(0.95f, 0.3f, 0.25f);
	/** Shape of a Fear pip (tinted FearOnColor / FearOffColor); empty = a plain square. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FSlateBrush PipBrush;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") float PipSize = 16.f;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FSlateFontInfo StatusFont;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor StatusChipColor = FLinearColor(0.3f, 0.22f, 0.05f, 0.95f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor StatusTextColor = FLinearColor(0.95f, 0.92f, 0.85f);

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
	/** The action's icon (FDwActionView::Icon), its description, and the outline shown while armed. */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> ActionIcon;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DescText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> ArmedOutline;

	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor ReadyColor = FLinearColor(0.12f, 0.16f, 0.22f, 0.95f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor ArmedColor = FLinearColor(0.75f, 0.58f, 0.12f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor DisabledColor = FLinearColor(0.08f, 0.08f, 0.09f, 0.9f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor NameColor = FLinearColor(0.95f, 0.92f, 0.85f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor DisabledNameColor = FLinearColor(0.55f, 0.55f, 0.58f);
	/** How far a card rises when hovered / armed (Slate units, upwards). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") float HoverLift = 26.f;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") float ArmedLift = 40.f;

public:
	/** The card's place in the fanned hand (set by the HUD): tilt in degrees and how far it sits below the top of the arc. */
	void SetFanPose(float AngleDegrees, float Drop);

protected:
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	UFUNCTION() void HandleClicked();
	UFUNCTION() void HandleHovered();
	void ApplyPose();

	bool bWasEnabled = true;
	bool bWasArmed = false;
	bool bArmed = false;
	bool bMouseOver = false;
	float FanAngle = 0.f;
	float FanDrop = 0.f;
};

/**
 * WBP_Intent: one telegraphed enemy move in the HUD's intent list (DW-303, B6): "1  Tentacle Slam", the target
 * ("Swordsman's tile") and the effect ("50 damage"). The frame turns red when it would kill (gold for the captain:
 * the run is at stake) and grey when it will miss.
 */
UCLASS(Abstract)
class DEADWATERRUN_API UDwIntentWidget : public UDwUserWidget
{
	GENERATED_BODY()

public:
	void SetView(const FDwIntentView& View);
	const FDwIntentView& GetShownView() const { return Shown; }

	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor NormalColor = FLinearColor(0.35f, 0.12f, 0.1f, 0.92f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor LethalColor = FLinearColor(0.85f, 0.12f, 0.08f, 0.95f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor CaptainColor = FLinearColor(0.85f, 0.62f, 0.1f, 0.95f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor MissColor = FLinearColor(0.25f, 0.27f, 0.3f, 0.9f);

protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> Frame;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> OrderText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> MoveText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TargetText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> EffectText;

private:
	FDwIntentView Shown;
};

/**
 * WBP_IntentMarker: the Kraken's next move floating above its body (B6b), so the threat reads where the player is
 * looking. Added to the player screen by WBP_CombatHud and positioned every frame from the anchor actor.
 */
UCLASS(Abstract)
class DEADWATERRUN_API UDwIntentMarkerWidget : public UDwUserWidget
{
	GENERATED_BODY()

public:
	/** Shows Text above Anchor (nullptr or empty text hides it). */
	void SetMarker(AActor* InAnchor, const FText& Text, bool bLethal);
	bool IsShowingMarker() const;
	FText GetMarkerText() const;

	/** World units above the anchor actor's origin. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Marker") float HeightAboveAnchor = 320.f;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor NormalColor = FLinearColor(1.f, 0.82f, 0.45f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor LethalColor = FLinearColor(1.f, 0.3f, 0.25f);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> MarkerText;

private:
	void UpdatePosition();

	TWeakObjectPtr<AActor> Anchor;
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
	/** Kraken fight (DW-303): one per declared intent in IntentList, and the marker above the body. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|HUD") TSubclassOf<UDwIntentWidget> IntentClass;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|HUD") TSubclassOf<UDwIntentMarkerWidget> IntentMarkerClass;
	/** Gap between crew cards, enemy bars and action buttons (Slate units). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|HUD") float EntrySpacing = 8.f;
	/** How long the turn banner stays up. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|HUD", meta = (ClampMin = "0.1")) float TurnBannerSeconds = 1.5f;
	/** How long the Kraken's phase banner ("The Kraken is ENRAGED!") stays up. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|HUD", meta = (ClampMin = "0.1")) float PhaseBannerSeconds = 2.5f;

	bool IsTurnBannerVisible() const;
	bool IsPhaseBannerVisible() const;
	/** The marker above the Kraken (null outside the Kraken fight until first needed). */
	const UDwIntentMarkerWidget* GetIntentMarker() const { return IntentMarker; }
	int32 GetIntentRowCount() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnViewModelChanged() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Deadwater|UI|Juice")
	void OnTurnBannerShown(EDwSide Side);

	UFUNCTION(BlueprintImplementableEvent, Category = "Deadwater|UI|Juice")
	void OnShipHealthChanged(EDwSide Side, float OldHealth, float NewHealth);

	UFUNCTION(BlueprintImplementableEvent, Category = "Deadwater|UI|Juice")
	void OnPhaseBannerShown(int32 PhaseIndex);

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
	// Kraken fight (DW-303): the boss bar where the enemy ship bar sits, the intent list under the enemy row, the phase banner.
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> BossPanel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> BossNameText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> BossPhaseText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> BossTentacleText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> BossHealthBar;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> BossHealthText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> IntentPanel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UVerticalBox> IntentList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> PhaseBanner;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> PhaseBannerText;
	// Art pass: "ROUND n" over "YOUR TURN", the selected pirate's plate, the strip behind hint / preview / message.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RoundText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> SelectedPanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> SelectedPortrait;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SelectedNameText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SelectedInfoText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> SelectedHealthBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SelectedHealthText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> SelectedManaBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SelectedManaText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SelectedBudgetText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> PromptPanel;

	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor AllyColor = FLinearColor(0.25f, 0.55f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor EnemyColor = FLinearColor(0.95f, 0.3f, 0.25f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor PreviewColor = FLinearColor(0.55f, 1.f, 0.55f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor LethalColor = FLinearColor(1.f, 0.35f, 0.3f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor EndTurnColor = FLinearColor(0.16f, 0.2f, 0.27f, 0.95f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor EndTurnSuggestedColor = FLinearColor(0.75f, 0.58f, 0.12f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") int32 PreviewFontSize = 20;
	/** Preview line font (empty = the engine default at PreviewFontSize). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FSlateFontInfo PreviewFont;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor SelectedHealthColor = FLinearColor(0.66f, 0.2f, 0.16f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor SelectedManaColor = FLinearColor(0.18f, 0.36f, 0.58f);
	/** The hand of action cards: tilt per card from the middle (degrees) and drop per card (Slate units). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") float FanAnglePerCard = 0.f;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") float FanDropPerCard = 0.f;

private:
	UFUNCTION() void HandleEndTurnClicked();
	UFUNCTION() void HandleTurnBanner(EDwSide Side, int32 Round);
	UFUNCTION() void HandlePhaseBanner(const FText& Text, int32 PhaseIndex);

	template <typename TWidget, typename TViewModel>
	void SyncList(UPanelWidget& Panel, TSubclassOf<TWidget> EntryClass, const TArray<TObjectPtr<TViewModel>>& ViewModels);
	void RefreshShips(const UDwCombatHudViewModel& Hud);
	void RefreshPrompt(const UDwCombatHudViewModel& Hud);
	void RefreshSelected(const UDwCombatHudViewModel& Hud);
	void FanActionCards();
	void HideTurnBanner();
	void HidePhaseBanner();
	void RefreshBoss(const UDwCombatHudViewModel& Hud);
	void RefreshIntentMarker(const UDwCombatHudViewModel& Hud);

	TWeakObjectPtr<UDwCombatHudViewModel> BoundHud;
	FTimerHandle BannerTimer;
	FTimerHandle PhaseBannerTimer;
	UPROPERTY(Transient) TObjectPtr<UDwIntentMarkerWidget> IntentMarker;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwIntentWidget>> IntentRows;
	float LastShipHealth[2] = { -1.f, -1.f };
};
