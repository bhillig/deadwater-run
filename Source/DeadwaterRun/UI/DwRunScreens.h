// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UI/DwActivatableScreen.h"
#include "UI/DwRunViewModels.h"
#include "UI/DwUserWidget.h"
#include "DwRunScreens.generated.h"

class UButton;
class UCanvasPanel;
class UPanelWidget;
class UTextBlock;

/*
 * Run screens (Docs/Architecture/14-UI.md "Screens", DW-211): WBP_Map, WBP_Rewards, WBP_LevelUp and WBP_NodeShell (shop
 * and event until DW-204/205). Structure and logic here; the WBPs (UDwUIContentLibrary) only lay out and style. Every
 * click goes to a view model intent; nothing here reads UDwRun.
 */

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

	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor ReadyColor = FLinearColor(0.12f, 0.16f, 0.22f, 0.95f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor SelectedColor = FLinearColor(0.75f, 0.58f, 0.12f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Style") FLinearColor DisabledColor = FLinearColor(0.08f, 0.08f, 0.09f, 0.9f);

private:
	UFUNCTION() void HandleClicked();
	UFUNCTION() void HandleHovered();
};

/** Base of the run screens: a choice button class and the fill helper. */
UCLASS(Abstract)
class DEADWATERRUN_API UDwRunScreen : public UDwActivatableScreen
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|UI") TSubclassOf<UDwChoiceButtonWidget> ChoiceButtonClass;

protected:
	/** One WBP_ChoiceButton per choice in Panel, reused while the count holds (so juice and hover survive refreshes). */
	void FillChoices(UPanelWidget* Panel, TConstArrayView<TObjectPtr<UDwChoiceViewModel>> Choices);
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

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> NodeButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> LabelText;

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

	/** Where a node sits in the chart canvas (top-left of the node). */
	FVector2D GetNodePosition(int32 Layer, int32 Lane) const;
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

/** WBP_NodeShell (phases Shop and Event until DW-204 / DW-205). */
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
