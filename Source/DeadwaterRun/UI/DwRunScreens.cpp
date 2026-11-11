// Copyright Brandon Hillig 2026

#include "UI/DwRunScreens.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Core/DwGameplayTags.h"
#include "Rendering/DrawElements.h"
#include "UI/DwUIJuice.h"

#define LOCTEXT_NAMESPACE "DwRunScreens"

// ---------------------------------------------------------------------------------------------------------------------
// Choice button

void UDwChoiceButtonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (ChoiceButton)
	{
		ChoiceButton->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
		ChoiceButton->OnHovered.AddDynamic(this, &ThisClass::HandleHovered);
	}
}

void UDwChoiceButtonWidget::HandleClicked()
{
	if (UDwChoiceViewModel* Choice = GetViewModel<UDwChoiceViewModel>())
	{
		PlayJuice(DwJuice::Press);
		PlayUISound(Choice->Select() ? DwTags::UI_Sound_Click : DwTags::UI_Sound_Error);
	}
}

void UDwChoiceButtonWidget::HandleHovered()
{
	PlayJuice(DwJuice::Hover);
	PlayUISound(DwTags::UI_Sound_Hover);
}

void UDwChoiceButtonWidget::NativeOnViewModelChanged()
{
	const UDwChoiceViewModel* Choice = GetViewModel<UDwChoiceViewModel>();
	if (!Choice)
	{
		return;
	}
	const FDwChoiceView& View = Choice->GetView();
	TitleText->SetText(View.Title);
	BodyText->SetText(View.Body);
	BodyText->SetVisibility(View.Body.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	ReasonText->SetText(View.DisabledReason);
	ReasonText->SetVisibility(View.DisabledReason.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	// Disabled buttons stay clickable: the click plays the error sound, the reason is already on the button.
	ChoiceButton->SetBackgroundColor(View.bSelected ? SelectedColor : (View.bEnabled ? ReadyColor : DisabledColor));
	if (View.bSelected)
	{
		PlayJuice(DwJuice::Highlight);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Run screen helpers

void UDwRunScreen::FillChoices(UPanelWidget* Panel, TConstArrayView<TObjectPtr<UDwChoiceViewModel>> Choices)
{
	if (!Panel || !ChoiceButtonClass)
	{
		return;
	}
	if (Panel->GetChildrenCount() != Choices.Num())
	{
		Panel->ClearChildren();
		for (int32 Index = 0; Index < Choices.Num(); ++Index)
		{
			Panel->AddChild(CreateWidget<UDwChoiceButtonWidget>(this, ChoiceButtonClass));
		}
	}
	for (int32 Index = 0; Index < Choices.Num(); ++Index)
	{
		if (UDwChoiceButtonWidget* Button = Cast<UDwChoiceButtonWidget>(Panel->GetChildAt(Index)))
		{
			Button->SetViewModel(Choices[Index]);
		}
	}
	Panel->SetVisibility(Choices.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

void UDwRunScreen::FillLines(UPanelWidget* Panel, TConstArrayView<FText> Lines, int32 FontSize, const FLinearColor& Color)
{
	if (!Panel)
	{
		return;
	}
	Panel->ClearChildren();
	for (const FText& Line : Lines)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
		Text->SetText(Line);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.f, 1.f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
		Panel->AddChild(Text);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Map

void UDwMapNodeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (NodeButton)
	{
		NodeButton->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
		NodeButton->OnHovered.AddDynamic(this, &ThisClass::HandleHovered);
	}
}

void UDwMapNodeWidget::SetNode(const FDwMapNodeView& InNode, UDwMapViewModel* InMap)
{
	const bool bBecameChoosable = InNode.State == EDwMapNodeState::Choosable && Node.State != EDwMapNodeState::Choosable;
	Node = InNode;
	Map = InMap;
	LabelText->SetText(Node.Label);
	FLinearColor Color = AheadColor;
	switch (Node.State)
	{
	case EDwMapNodeState::Choosable: Color = ChoosableColor; break;
	case EDwMapNodeState::Current:   Color = CurrentColor; break;
	case EDwMapNodeState::Sailed:    Color = SailedColor; break;
	case EDwMapNodeState::Missed:    Color = MissedColor; break;
	case EDwMapNodeState::Ahead:     Color = Node.Type == EDwNodeType::Boss ? BossColor : AheadColor; break;
	}
	NodeButton->SetBackgroundColor(Color);
	LabelText->SetColorAndOpacity(FSlateColor(Node.State == EDwMapNodeState::Missed ? FLinearColor(0.5f, 0.5f, 0.5f) : FLinearColor(0.95f, 0.92f, 0.85f)));
	FText Reason;
	const bool bCan = InMap && InMap->CanChoose(Node.Index, &Reason);
	NodeButton->SetToolTipText(bCan ? Node.Tooltip : FText::Format(LOCTEXT("NodeTip", "{0}\n{1}"), Node.Tooltip, Reason));
	if (bBecameChoosable)
	{
		PlayJuice(DwJuice::Highlight);
	}
}

void UDwMapNodeWidget::HandleClicked()
{
	UDwMapViewModel* TheMap = Map.Get();
	FText Reason;
	const bool bCan = TheMap && TheMap->CanChoose(Node.Index, &Reason);
	PlayJuice(DwJuice::Press);
	PlayUISound(bCan ? DwTags::UI_Sound_Confirm : DwTags::UI_Sound_Error);
	if (UDwMapScreen* Screen = GetTypedOuter<UDwMapScreen>())
	{
		Screen->ShowHint(Reason);
	}
	if (bCan)
	{
		TheMap->ChooseNode(Node.Index);
	}
}

void UDwMapNodeWidget::HandleHovered()
{
	PlayJuice(DwJuice::Hover);
	PlayUISound(DwTags::UI_Sound_Hover);
}

FVector2D UDwMapScreen::GetNodePosition(int32 Layer, int32 Lane) const
{
	return FVector2D(Layer * Spacing.X, Lane * Spacing.Y);
}

void UDwMapScreen::ShowHint(const FText& Hint)
{
	HintText->SetText(Hint);
}

void UDwMapScreen::NativeOnViewModelChanged()
{
	UDwMapViewModel* Map = GetViewModel<UDwMapViewModel>();
	if (!Map)
	{
		return;
	}
	TitleText->SetText(Map->GetTitle());
	GoldText->SetText(FText::Format(LOCTEXT("Gold", "{0} gold"), Map->GetGold()));
	ShipText->SetText(Map->GetShipText());
	TArray<FText> Crew;
	for (const FDwCrewLineView& Line : Map->GetCrew())
	{
		Crew.Add(FText::Format(LOCTEXT("CrewLine", "{0}{1}   {2}"), Line.Name, Line.bIsCaptain ? LOCTEXT("Captain", " (captain)") : FText::GetEmpty(), Line.Detail));
	}
	FillLines(CrewList, Crew, 16, FLinearColor(0.95f, 0.92f, 0.85f));

	const TArray<FDwMapNodeView>& Nodes = Map->GetNodes();
	if (NodeWidgets.Num() != Nodes.Num() && NodeWidgetClass)
	{
		Chart->ClearChildren();
		NodeWidgets.Reset();
		for (const FDwMapNodeView& Node : Nodes)
		{
			UDwMapNodeWidget* Widget = CreateWidget<UDwMapNodeWidget>(this, NodeWidgetClass);
			UCanvasPanelSlot* NodeSlot = Chart->AddChildToCanvas(Widget);
			NodeSlot->SetAutoSize(false);
			NodeSlot->SetSize(NodeSize);
			NodeSlot->SetPosition(GetNodePosition(Node.Layer, Node.Lane));
			NodeWidgets.Add(Widget);
		}
	}
	for (int32 Index = 0; Index < NodeWidgets.Num() && Index < Nodes.Num(); ++Index)
	{
		NodeWidgets[Index]->SetNode(Nodes[Index], Map);
		if (UCanvasPanelSlot* NodeSlot = Cast<UCanvasPanelSlot>(NodeWidgets[Index]->Slot))
		{
			NodeSlot->SetPosition(GetNodePosition(Nodes[Index].Layer, Nodes[Index].Lane));
		}
	}
	RefreshRoutes(*Map);
	HintText->SetText(FText::GetEmpty());
}

void UDwMapScreen::RefreshRoutes(const UDwMapViewModel& Map)
{
	// Routes run from a node's right edge to the next node's left edge; gold = open now, blue = sailed.
	TArray<FDwMapRoute> Lines;
	FVector2D Extent = FVector2D::ZeroVector;
	for (const FDwMapNodeView& From : Map.GetNodes())
	{
		Extent = FVector2D::Max(Extent, GetNodePosition(From.Layer, From.Lane) + NodeSize);
		for (const int32 ToIndex : From.Next)
		{
			const FDwMapNodeView* To = Map.FindNode(ToIndex);
			if (!To)
			{
				continue;
			}
			const bool bOpen = From.State == EDwMapNodeState::Current && To->State == EDwMapNodeState::Choosable;
			const bool bSailed = (From.State == EDwMapNodeState::Sailed || From.State == EDwMapNodeState::Current)
				&& (To->State == EDwMapNodeState::Sailed || To->State == EDwMapNodeState::Current);
			FDwMapRoute& Line = Lines.AddDefaulted_GetRef();
			Line.Start = GetNodePosition(From.Layer, From.Lane) + FVector2D(NodeSize.X, NodeSize.Y * 0.5f);
			Line.End = GetNodePosition(To->Layer, To->Lane) + FVector2D(0.f, NodeSize.Y * 0.5f);
			Line.Color = bOpen ? OpenRouteColor : (bSailed ? SailedRouteColor : RouteColor);
			Line.Thickness = bOpen || bSailed ? 3.f : 1.5f;
		}
	}
	if (!Routes)
	{
		Routes = CreateWidget<UDwMapRoutesWidget>(this, UDwMapRoutesWidget::StaticClass());
		Routes->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (Routes->GetParent() != Chart)
	{
		Chart->AddChildToCanvas(Routes);
	}
	if (UCanvasPanelSlot* RoutesSlot = Cast<UCanvasPanelSlot>(Routes->Slot))
	{
		RoutesSlot->SetAutoSize(false);
		RoutesSlot->SetPosition(FVector2D::ZeroVector);
		RoutesSlot->SetSize(Extent);
		RoutesSlot->SetZOrder(-1); // behind the nodes
	}
	Routes->SetRoutes(MoveTemp(Lines));
}

void UDwMapRoutesWidget::SetRoutes(TArray<FDwMapRoute> InRoutes)
{
	Routes = MoveTemp(InRoutes);
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 UDwMapRoutesWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	for (const FDwMapRoute& Route : Routes)
	{
		const TArray<FVector2D> Points = { Route.Start, Route.End }; // already in this widget's space: it fills the chart from (0, 0)
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Route.Color,
			/*bAntialias*/ true, Route.Thickness);
	}
	return Layer;
}

// ---------------------------------------------------------------------------------------------------------------------
// Rewards, level-up, shells

void UDwRewardsScreen::NativeOnViewModelChanged()
{
	const UDwRewardsViewModel* Rewards = GetViewModel<UDwRewardsViewModel>();
	if (!Rewards)
	{
		return;
	}
	HeadlineText->SetText(Rewards->GetHeadline());
	FillLines(LootList, Rewards->GetLines(), 20, FLinearColor(0.95f, 0.92f, 0.85f));
	OfferHintText->SetText(Rewards->GetNoOffersText());
	FillChoices(OfferList, Rewards->GetOffers());
	FillChoices(LearnerList, Rewards->GetLearners());
	FillChoices(ForgetList, Rewards->GetForget());
	const TArray<TObjectPtr<UDwChoiceViewModel>> Claim = { Rewards->GetClaim() };
	FillChoices(ClaimSlot, Claim);
}

void UDwLevelUpScreen::NativeOnViewModelChanged()
{
	const UDwLevelUpViewModel* LevelUp = GetViewModel<UDwLevelUpViewModel>();
	if (!LevelUp)
	{
		return;
	}
	HeadlineText->SetText(LevelUp->GetHeadline());
	StatText->SetText(LevelUp->GetStatText());
	QueueText->SetText(LevelUp->GetQueueText());
	QueueText->SetVisibility(LevelUp->GetQueueText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	FillChoices(CardList, LevelUp->GetCards());
	if (!LevelUp->GetCards().IsEmpty())
	{
		PlayJuice(DwJuice::Highlight); // OnLevelUpShown hook (doc 14)
	}
}

void UDwNodeShellScreen::NativeOnViewModelChanged()
{
	const UDwNodeShellViewModel* Shell = GetViewModel<UDwNodeShellViewModel>();
	if (!Shell)
	{
		return;
	}
	HeadlineText->SetText(Shell->GetHeadline());
	BodyText->SetText(Shell->GetBody());
	const TArray<TObjectPtr<UDwChoiceViewModel>> Leave = { Shell->GetLeave() };
	FillChoices(ActionSlot, Leave);
}

#undef LOCTEXT_NAMESPACE
