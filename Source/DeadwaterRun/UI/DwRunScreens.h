// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UI/DwActivatableScreen.h"
#include "UI/DwRunViewModels.h"
#include "UI/DwUserWidget.h"
#include "DwRunScreens.generated.h"

class UButton;
class UCanvasPanel;
class UImage;
class UTexture2D;
class UPanelWidget;
class UTextBlock;
class UWidget;
class UDataTable;

/*
 * Run screens (Docs/Architecture/14-UI.md "Screens", DW-211): WBP_Map, WBP_Rewards, WBP_LevelUp, WBP_Shop and WBP_NodeShell
 * (event). Structure and logic here; the WBPs (UDwUIContentLibrary) only lay out and style. Every
 * click goes to a view model intent; nothing here reads UDwRun.
 */

/** How a list of choices is drawn (UDwRunScreen::FillChoices). */
enum class EDwChoiceStyle : uint8
{
	Row,    // WBP_ChoiceButton: a parchment strip
	Card,   // WBP_ChoiceCard: tall card with a big icon (perks, reward moves)
	Ledger, // WBP_LedgerRow: a line of the shop ledger, price on the right
	Poster, // WBP_PosterCard: a wanted poster (captain select)
	Chip,   // WBP_ChoiceChip: a small outlined portrait chip (who learns a move)
	Offer   // WBP_OfferRow: a shop move - big icon, name, italic text, who can learn it, price
};

/** WBP_ChoiceButton: one UDwChoiceViewModel - title, body, disabled reason, selected highlight. */
UCLASS(Abstract)
class DEADWATERRUN_API UDwChoiceButtonWidget : public UDwUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnViewModelChanged() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ChoiceButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> BodyText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ReasonText;
	/** FDwChoiceView::Icon (hidden when the choice has none) and the outline shown while selected. */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> ChoiceIcon;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> SelectedOutline;
	/** Tint for glyph icons (FDwChoiceView::bIconIsGlyph); coloured renders stay untinted. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor GlyphColor = FLinearColor::White;
	/** Ledger rows: the price column ("30", then PriceUnitText "gold"), and a stamp ("SOLD OUT") for short disabled reasons instead of the red line. */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PriceText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> PriceUnitText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> StampPanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StampText;
	/** Posters: FDwChoiceView::Details, one icon + text line each. */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> DetailList;
	/** FDwChoiceView::bPrimary: the button art and title colour of the screen's main action. Unset = no primary look. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FButtonStyle PrimaryStyle;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor PrimaryTitleColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") bool bHasPrimaryStyle = false;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FSlateFontInfo DetailFont;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor DetailColor = FLinearColor::White;
	/** FDwChoiceDetail::Title, before the text on the same line (wraps under it when the line is full). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FSlateFontInfo DetailTitleFont;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor DetailTitleColor = FLinearColor::White;
	/**
	 * Rich text styles (FRichTextStyleRow: "Default" = the text, "b" = the title). Set: each detail line is one paragraph
	 * that wraps as a whole ("<b>Rally Cry</> - Heal all crew 15, clear bad effects"); unset: title and text blocks.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") TObjectPtr<UDataTable> DetailTextStyles;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") float DetailIconSize = 34.f;
	/** Shown before a non-empty body ("- Heal 40 HP in a fight" after the name on a ledger line). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FText BodyPrefix;
	/** Opacity of a disabled, unselected choice (sold-out ledger rows fade). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") float DisabledOpacity = 1.f;
	/** Disabled reasons up to this many characters become a stamp (when StampText exists). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") int32 StampMaxChars = 18;
	/** Rows without an icon centre their text (menu buttons, Leave, Set Sail). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") bool bCentreTextWithoutIcon = false;
	/** Keep the icon's space when a choice has none, so a list's text lines up (ledger). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") bool bReserveIconSpace = false;

	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor ReadyColor = FLinearColor::White; // tints the parchment button art (Ship's Log)
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor SelectedColor = FLinearColor(1.f, 0.9f, 0.8f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor DisabledColor = FLinearColor(0.74f, 0.71f, 0.66f);

private:
	UFUNCTION() void HandleClicked();
	UFUNCTION() void HandleHovered();
	FButtonStyle DefaultStyle;
	FSlateColor DefaultTitleColor;
	FSlateColor DefaultBodyColor;
	bool bCachedDefaults = false;
	/** The icon last asked for: a refresh with the same icon must not restart its async load (UImage cancels and re-requests). */
	FSoftObjectPath ShownIcon;
	/** The details DetailList was built from (rebuilt only when they change, for the same reason). */
	TArray<FDwChoiceDetail> ShownDetails;
	bool bDetailsShown = false;
};

/** Base of the run screens: a choice button class and the fill helper. */
UCLASS(Abstract)
class DEADWATERRUN_API UDwRunScreen : public UDwActivatableScreen
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|UI") TSubclassOf<UDwChoiceButtonWidget> ChoiceButtonClass;
	/** EDwChoiceStyle::Card, ::Ledger and ::Poster entries (Row uses ChoiceButtonClass). Missing = a row. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|UI") TSubclassOf<UDwChoiceButtonWidget> CardButtonClass;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|UI") TSubclassOf<UDwChoiceButtonWidget> LedgerRowClass;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|UI") TSubclassOf<UDwChoiceButtonWidget> PosterCardClass;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|UI") TSubclassOf<UDwChoiceButtonWidget> ChipClass;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|UI") TSubclassOf<UDwChoiceButtonWidget> OfferRowClass;
	/** Text lines (FillLines): the art direction's body font and ink. An unset font keeps the engine font; alpha 0 keeps the caller's colour. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FSlateFontInfo LineFont;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor LineColor = FLinearColor::Transparent;
	/** Used for every line when > 0 (the caller's size otherwise); and the lines' justification. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") int32 LineFontSize = 0;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") TEnumAsByte<ETextJustify::Type> LineJustification = ETextJustify::Left;

protected:
	/** One entry per choice in Panel, drawn in Style; reused while the count and kind hold (so juice and hover survive refreshes). */
	void FillChoices(UPanelWidget* Panel, TConstArrayView<TObjectPtr<UDwChoiceViewModel>> Choices, EDwChoiceStyle Style = EDwChoiceStyle::Row);
	/** Plain text lines into a vertical panel. */
	void FillLines(UPanelWidget* Panel, TConstArrayView<FText> Lines, int32 FontSize, const FLinearColor& Color);
};

/** WBP_MapNode: one node of the chart. */
UCLASS(Abstract)
class DEADWATERRUN_API UDwMapNodeWidget : public UDwUserWidget
{
	GENERATED_BODY()

public:
	void SetNode(const FDwMapNodeView& InNode, UDwMapViewModel* InMap);
	int32 GetNodeIndex() const { return Node.Index; }

	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor AheadColor = FLinearColor(0.1f, 0.13f, 0.18f, 0.9f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor ChoosableColor = FLinearColor(0.75f, 0.58f, 0.12f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor CurrentColor = FLinearColor(0.2f, 0.55f, 0.95f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor SailedColor = FLinearColor(0.28f, 0.24f, 0.16f, 0.95f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor MissedColor = FLinearColor(0.05f, 0.05f, 0.06f, 0.6f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor BossColor = FLinearColor(0.45f, 0.08f, 0.1f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor LabelColor = FLinearColor(0.95f, 0.92f, 0.85f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor MissedLabelColor = FLinearColor(0.5f, 0.5f, 0.5f);
	/** Glyph per node type in NodeIcon (missing = the label alone). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") TObjectPtr<UTexture2D> FightIcon;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") TObjectPtr<UTexture2D> ShopIcon;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") TObjectPtr<UTexture2D> EventIcon;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") TObjectPtr<UTexture2D> BossIcon;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor IconColor = FLinearColor::White;
	/** Ring: the stamp's ink circle (separate from the fill so a choosable stop can be ringed in red). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor RingColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor ChoosableRingColor = FLinearColor::White;
	/** Glyph size for ordinary stops and for the boss (whose stamp the chart draws larger: UDwMapScreen::BossScale). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") float IconSize = 42.f;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") float BossIconSize = 42.f;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> NodeButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> LabelText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> NodeIcon;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Ring;
	/** A red cross on sailed stops, and the "WE ARE HERE" tag on the current one. */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> SailedMark;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> HereTag;

private:
	UFUNCTION() void HandleClicked();
	UFUNCTION() void HandleHovered();
	FDwMapNodeView Node;
	TWeakObjectPtr<UDwMapViewModel> Map;
};

/** One route line on the chart, in chart coordinates. */
struct FDwMapRoute
{
	FVector2D Start = FVector2D::ZeroVector;
	FVector2D End = FVector2D::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	float Thickness = 1.f;
	/** Drawn as an ink dash pattern (routes not yet sailed). */
	bool bDashed = false;
	/** How far the route bows upwards at its middle, in chart units (0 = straight). */
	float Bow = 0.f;
};

/**
 * The routes between chart nodes: a native widget the map screen puts first in the chart canvas (behind the nodes,
 * filling it), so it paints in the chart's own coordinates - no conversion between widget spaces.
 */
UCLASS()
class DEADWATERRUN_API UDwMapRoutesWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetRoutes(TArray<FDwMapRoute> InRoutes);

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TArray<FDwMapRoute> Routes;
};

/** A hand-inked rule across the widget's middle, solid or dashed (the shop ledger's lines). Give it a height (size box). */
UCLASS()
class DEADWATERRUN_API UDwInkRuleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Deadwater|Style") FLinearColor Color = FLinearColor::Black;
	UPROPERTY(EditAnywhere, Category = "Deadwater|Style") float Thickness = 1.f;
	/** Dash and gap lengths; Dash 0 = a solid line. */
	UPROPERTY(EditAnywhere, Category = "Deadwater|Style") float Dash = 0.f;
	UPROPERTY(EditAnywhere, Category = "Deadwater|Style") float Gap = 4.f;

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};

/** WBP_Map (phase Map): header (gold, hull, crew), the chart with its routes, a hint line. */
UCLASS(Abstract)
class DEADWATERRUN_API UDwMapScreen : public UDwRunScreen
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Map") TSubclassOf<UDwMapNodeWidget> NodeWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Map") FVector2D NodeSize = FVector2D(112.f, 56.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Map") FVector2D Spacing = FVector2D(145.f, 116.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Map") FLinearColor RouteColor = FLinearColor(0.55f, 0.55f, 0.6f, 0.45f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Map") FLinearColor OpenRouteColor = FLinearColor(1.f, 0.82f, 0.3f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Map") FLinearColor SailedRouteColor = FLinearColor(0.35f, 0.65f, 1.f, 0.9f);
	/** Hand-drawn chart: routes bow by this much, and the ones not yet sailed are dashed. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Map") float RouteBow = 0.f;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Map") bool bDashUnsailed = false;
	/** Crew faces in the header (CrewPortraits), this big. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Map") float CrewPortraitSize = 62.f;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Map") FSlateBrush CrewPortraitFrame;
	/** The boss stop's stamp is this many times NodeSize, centred where an ordinary stop would sit. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Map") float BossScale = 1.f;
	/** Route thickness: open or sailed, and the rest. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Map") float RouteThickness = 3.f;
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Map") float FarRouteThickness = 1.5f;

	/** Where a node sits in the chart canvas (top-left of an ordinary node). */
	FVector2D GetNodePosition(int32 Layer, int32 Lane) const;
	/** A node's stamp in the chart canvas: top-left and size (the boss is BossScale times larger, same centre). */
	FBox2D GetNodeBox(const FDwMapNodeView& Node) const;
	/** A failed click's reason (or empty), shown under the chart. */
	void ShowHint(const FText& Hint);

protected:
	virtual void NativeOnViewModelChanged() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> GoldText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ShipText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> CrewList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCanvasPanel> Chart;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> HintText;
	/** The crew as framed faces (a horizontal box), next to the gold. */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> CrewPortraits;

private:
	void RefreshRoutes(const UDwMapViewModel& Map);

	UPROPERTY(Transient) TArray<TObjectPtr<UDwMapNodeWidget>> NodeWidgets;
	UPROPERTY(Transient) TObjectPtr<UDwMapRoutesWidget> Routes;
};

/** WBP_Rewards (phase Rewards). */
UCLASS(Abstract)
class DEADWATERRUN_API UDwRewardsScreen : public UDwRunScreen
{
	GENERATED_BODY()

protected:
	virtual void NativeOnViewModelChanged() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> HeadlineText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> LootList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> OfferHintText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> OfferList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> LearnerList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> ForgetList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> ClaimSlot;
};

/** WBP_LevelUp (phase LevelUp). */
UCLASS(Abstract)
class DEADWATERRUN_API UDwLevelUpScreen : public UDwRunScreen
{
	GENERATED_BODY()

protected:
	virtual void NativeOnViewModelChanged() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> HeadlineText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> StatText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> QueueText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> CardList;
};

/**
 * WBP_Shop (phase Shop): header (gold, hull, hold, crew), supplies, moves (with the learner / forget pick and Buy),
 * crew for hire, ship upgrades, disbands, Leave. Empty sections collapse with their label.
 * Juice hooks (doc 14): OnGoldChanged after every gold change, OnPurchase after every purchase attempt.
 */
UCLASS(Abstract)
class DEADWATERRUN_API UDwShopScreen : public UDwRunScreen
{
	GENERATED_BODY()

protected:
	virtual void NativeOnViewModelChanged() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Deadwater|UI|Juice") void OnGoldChanged(int32 OldGold, int32 NewGold);
	UFUNCTION(BlueprintImplementableEvent, Category = "Deadwater|UI|Juice") void OnPurchase(EDwPurchaseResult Result);

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> GoldText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ShipText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> HoldText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> CrewList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> SupplyLabel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> SupplyList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> MoveHintText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> MoveList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> LearnerList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> ForgetList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> BuyMoveSlot;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> RecruitLabel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> RecruitList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> UpgradeLabel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> UpgradeList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> DisbandLabel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> DisbandList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> LeaveSlot;

private:
	/** Fills a section and shows its label only when it has cards. */
	void FillSection(UWidget* Label, UPanelWidget* Panel, TConstArrayView<TObjectPtr<UDwChoiceViewModel>> Choices);

	int32 ShownGold = INDEX_NONE;
	int32 ShownPurchases = 0;
};

/** WBP_NodeShell (phase Event until WBP_Event). */
UCLASS(Abstract)
class DEADWATERRUN_API UDwNodeShellScreen : public UDwRunScreen
{
	GENERATED_BODY()

protected:
	virtual void NativeOnViewModelChanged() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> HeadlineText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> BodyText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> ActionSlot;
};
