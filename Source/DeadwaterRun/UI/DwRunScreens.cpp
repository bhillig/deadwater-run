// Copyright Brandon Hillig 2026

#include "UI/DwRunScreens.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/RichTextBlock.h"
#include "Components/WrapBox.h"
#include "Engine/DataTable.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Core/DwGameplayTags.h"
#include "Rendering/DrawElements.h"
#include "UI/DwUIJuice.h"
#include "UI/DwUIManagerSubsystem.h"

#define LOCTEXT_NAMESPACE "DwRunScreens"

// ---------------------------------------------------------------------------------------------------------------------
// Choice button

void UDwChoiceButtonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (ChoiceButton && !bCachedDefaults)
	{
		DefaultStyle = ChoiceButton->GetStyle();
		DefaultTitleColor = TitleText ? TitleText->GetColorAndOpacity() : FSlateColor(FLinearColor::White);
		DefaultBodyColor = BodyText ? BodyText->GetColorAndOpacity() : FSlateColor(FLinearColor::White);
		bCachedDefaults = true;
	}
	if (ChoiceButton)
	{
		ChoiceButton->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
		ChoiceButton->OnHovered.AddDynamic(this, &ThisClass::HandleHovered);
	}
}

void UDwChoiceButtonWidget::HandleClicked()
{
	UDwChoiceViewModel* Choice = GetViewModel<UDwChoiceViewModel>();
	if (!Choice)
	{
		return;
	}
	PlayJuice(DwJuice::Press);
	// Read before Select: the intent usually refreshes the view model, which replaces the view.
	const FDwChoiceView View = Choice->GetView();
	const FGameplayTag Accepted = View.SelectSound.IsValid() ? View.SelectSound : DwTags::UI_Sound_Click;
	UDwUIManagerSubsystem* Manager = UDwUIManagerSubsystem::Get(GetOwningLocalPlayer());
	if (View.bEnabled && !View.ConfirmTitle.IsEmpty() && Manager)
	{
		// Can't be undone: ask first (WBP_Confirm). The button may be rebuilt before the answer, so hold both weakly.
		PlayUISound(DwTags::UI_Sound_Click);
		const TWeakObjectPtr<UDwChoiceViewModel> WeakChoice = Choice;
		const TWeakObjectPtr<UDwChoiceButtonWidget> WeakThis = this;
		Manager->Confirm(View.ConfirmTitle, View.ConfirmBody, View.ConfirmYes, [WeakChoice, WeakThis, Accepted]()
		{
			const bool bOk = WeakChoice.IsValid() && WeakChoice->Select();
			if (WeakThis.IsValid())
			{
				WeakThis->PlayUISound(bOk ? Accepted : DwTags::UI_Sound_Error);
			}
		});
		return;
	}
	PlayUISound(Choice->Select() ? Accepted : DwTags::UI_Sound_Error);
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
	BodyText->SetText(View.Body.IsEmpty() || BodyPrefix.IsEmpty() ? View.Body : FText::Format(LOCTEXT("PrefixedBody", "{0}{1}"), BodyPrefix, View.Body));
	BodyText->SetVisibility(View.Body.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	ReasonText->SetText(View.DisabledReason);
	ReasonText->SetVisibility(View.DisabledReason.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	// Disabled buttons stay clickable: the click plays the error sound, the reason is already on the button.
	ChoiceButton->SetBackgroundColor(View.bSelected ? SelectedColor : (View.bEnabled ? ReadyColor : DisabledColor));
	if (ChoiceIcon)
	{
		if (View.Icon.ToSoftObjectPath() != ShownIcon)
		{
			ShownIcon = View.Icon.ToSoftObjectPath();
			ChoiceIcon->SetBrushFromSoftTexture(View.Icon);
		}
		ChoiceIcon->SetVisibility(View.Icon.IsNull() ? (bReserveIconSpace ? ESlateVisibility::Hidden : ESlateVisibility::Collapsed) : ESlateVisibility::HitTestInvisible);
		ChoiceIcon->SetRenderOpacity(View.bEnabled || View.bSelected ? 1.f : 0.5f);
		ChoiceIcon->SetColorAndOpacity(View.bIconIsGlyph ? GlyphColor : FLinearColor::White);
	}
	if (SelectedOutline)
	{
		SelectedOutline->SetVisibility(View.bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	// The screen's main action: the primary (red) art while it can be taken; everything else (and a disabled primary,
	// whose red reason must stay readable) keeps the layout's own look.
	if (bHasPrimaryStyle && bCachedDefaults)
	{
		const bool bPrimary = View.bPrimary && View.bEnabled;
		ChoiceButton->SetStyle(bPrimary ? PrimaryStyle : DefaultStyle);
		TitleText->SetColorAndOpacity(bPrimary ? FSlateColor(PrimaryTitleColor) : DefaultTitleColor);
		BodyText->SetColorAndOpacity(bPrimary ? FSlateColor(PrimaryTitleColor) : DefaultBodyColor);
	}
	if (bCentreTextWithoutIcon)
	{
		const ETextJustify::Type Justify = View.Icon.IsNull() ? ETextJustify::Center : ETextJustify::Left;
		TitleText->SetJustification(Justify);
		BodyText->SetJustification(Justify);
		ReasonText->SetJustification(Justify);
	}
	// Ledger: price on the right; a short reason ("Sold out.") becomes a stamp in its place.
	const FString Reason = View.DisabledReason.ToString();
	const bool bStamp = StampText && !View.bEnabled && !Reason.IsEmpty() && Reason.Len() <= StampMaxChars;
	if (bStamp)
	{
		StampText->SetText(FText::FromString(Reason.TrimEnd().LeftChop(Reason.EndsWith(TEXT(".")) ? 1 : 0).ToUpper()));
		ReasonText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (StampPanel)
	{
		StampPanel->SetVisibility(bStamp ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (PriceText)
	{
		PriceText->SetText(View.Price);
		PriceText->SetVisibility(View.Price.IsEmpty() || bStamp ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (PriceUnitText)
	{
		PriceUnitText->SetVisibility(View.Price.IsEmpty() || bStamp ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	// Rebuilt only when the details change: new images would restart their icons' async loads on every refresh.
	auto SameDetails = [](const TArray<FDwChoiceDetail>& A, const TArray<FDwChoiceDetail>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].Icon != B[Index].Icon || !A[Index].Title.EqualTo(B[Index].Title) || !A[Index].Text.EqualTo(B[Index].Text))
			{
				return false;
			}
		}
		return true;
	};
	if (DetailList && (!bDetailsShown || !SameDetails(View.Details, ShownDetails)))
	{
		bDetailsShown = true;
		ShownDetails = View.Details;
		DetailList->ClearChildren();
		for (const FDwChoiceDetail& Detail : View.Details)
		{
			UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
			if (!Detail.Icon.IsNull())
			{
				USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
				Box->SetWidthOverride(DetailIconSize);
				Box->SetHeightOverride(DetailIconSize);
				UImage* Icon = WidgetTree->ConstructWidget<UImage>();
				Icon->SetBrushFromSoftTexture(Detail.Icon);
				Box->SetContent(Icon);
				if (UHorizontalBoxSlot* IconSlot = Line->AddChildToHorizontalBox(Box))
				{
					IconSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
					IconSlot->SetVerticalAlignment(VAlign_Top);
				}
			}
			if (DetailTextStyles)
			{
				// One paragraph: the title in the "b" style, then " - text", wrapping as a whole.
				auto Escape = [](const FText& Value) { return Value.ToString().Replace(TEXT("&"), TEXT("&amp;")).Replace(TEXT("<"), TEXT("&lt;")).Replace(TEXT(">"), TEXT("&gt;")); };
				const FString Markup = Detail.Title.IsEmpty() ? Escape(Detail.Text)
					: FString::Printf(TEXT("<b>%s</> - %s"), *Escape(Detail.Title), *Escape(Detail.Text));
				URichTextBlock* Paragraph = WidgetTree->ConstructWidget<URichTextBlock>();
				Paragraph->SetTextStyleSet(DetailTextStyles);
				Paragraph->SetAutoWrapText(true);
				Paragraph->SetText(FText::FromString(Markup));
				if (UHorizontalBoxSlot* TextSlot = Line->AddChildToHorizontalBox(Paragraph))
				{
					TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
					TextSlot->SetVerticalAlignment(VAlign_Center);
				}
				Line->SetVisibility(ESlateVisibility::HitTestInvisible);
				DetailList->AddChild(Line);
				continue;
			}
			// "Title - text" on one line while it fits; the text wraps under the title when it does not.
			UWrapBox* Words = WidgetTree->ConstructWidget<UWrapBox>();
			Words->SetInnerSlotPadding(FVector2D(0.f, 0.f));
			auto AddWords = [this, Words](const FText& Value, const FSlateFontInfo& Font, const FLinearColor& Color)
			{
				UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
				Text->SetText(Value);
				if (Font.HasValidFont())
				{
					Text->SetFont(Font);
				}
				Text->SetColorAndOpacity(FSlateColor(Color));
				Text->SetAutoWrapText(true);
				if (UWrapBoxSlot* WordSlot = Words->AddChildToWrapBox(Text))
				{
					WordSlot->SetVerticalAlignment(VAlign_Bottom);
				}
			};
			if (!Detail.Title.IsEmpty())
			{
				AddWords(Detail.Title, DetailTitleFont.HasValidFont() ? DetailTitleFont : DetailFont, DetailTitleColor);
			}
			AddWords(Detail.Title.IsEmpty() ? Detail.Text : FText::Format(LOCTEXT("DetailText", " - {0}"), Detail.Text), DetailFont, DetailColor);
			if (UHorizontalBoxSlot* TextSlot = Line->AddChildToHorizontalBox(Words))
			{
				TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				TextSlot->SetVerticalAlignment(VAlign_Center);
			}
			Line->SetVisibility(ESlateVisibility::HitTestInvisible);
			DetailList->AddChild(Line);
		}
		DetailList->SetVisibility(View.Details.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	SetRenderOpacity(View.bEnabled || View.bSelected ? 1.f : DisabledOpacity);
	if (View.bSelected)
	{
		PlayJuice(DwJuice::Highlight);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Run screen helpers

void UDwRunScreen::FillChoices(UPanelWidget* Panel, TConstArrayView<TObjectPtr<UDwChoiceViewModel>> Choices, EDwChoiceStyle Style)
{
	TSubclassOf<UDwChoiceButtonWidget> EntryClass = ChoiceButtonClass;
	switch (Style)
	{
	case EDwChoiceStyle::Card:   EntryClass = CardButtonClass ? CardButtonClass : ChoiceButtonClass; break;
	case EDwChoiceStyle::Ledger: EntryClass = LedgerRowClass ? LedgerRowClass : ChoiceButtonClass; break;
	case EDwChoiceStyle::Poster: EntryClass = PosterCardClass ? PosterCardClass : ChoiceButtonClass; break;
	case EDwChoiceStyle::Chip:   EntryClass = ChipClass ? ChipClass : ChoiceButtonClass; break;
	case EDwChoiceStyle::Offer:  EntryClass = OfferRowClass ? OfferRowClass : ChoiceButtonClass; break;
	default: break;
	}
	if (!Panel || !EntryClass)
	{
		return;
	}
	const bool bSameKind = Panel->GetChildrenCount() == 0 || (Panel->GetChildAt(0) && Panel->GetChildAt(0)->GetClass() == EntryClass.Get());
	if (Panel->GetChildrenCount() != Choices.Num() || !bSameKind)
	{
		Panel->ClearChildren();
		for (int32 Index = 0; Index < Choices.Num(); ++Index)
		{
			Panel->AddChild(CreateWidget<UDwChoiceButtonWidget>(this, EntryClass));
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
		FSlateFontInfo Font = LineFont.HasValidFont() ? LineFont : Text->GetFont();
		Font.Size = LineFontSize > 0 ? LineFontSize : FontSize;
		Text->SetFont(Font);
		Text->SetJustification(LineJustification);
		const FLinearColor Ink = LineColor.A > 0.f ? LineColor : Color;
		Text->SetColorAndOpacity(FSlateColor(Ink));
		// Light text over the sea gets a shadow; ink on parchment none.
		const bool bLight = Ink.GetLuminance() > 0.25f;
		Text->SetShadowOffset(bLight ? FVector2D(1.f, 1.f) : FVector2D::ZeroVector);
		Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, bLight ? 0.85f : 0.f));
		Text->SetAutoWrapText(true);
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
	if (Ring)
	{
		// A rounded-box outline keeps its own colour (the image tint only reaches the fill), so the ring is recoloured on the brush.
		FSlateBrush RingBrush = Ring->GetBrush();
		RingBrush.OutlineSettings.Color = FSlateColor(Node.State == EDwMapNodeState::Choosable ? ChoosableRingColor : RingColor);
		Ring->SetBrush(RingBrush);
	}
	LabelText->SetColorAndOpacity(FSlateColor(Node.State == EDwMapNodeState::Missed ? MissedLabelColor : LabelColor));
	if (NodeIcon)
	{
		UTexture2D* Glyph = FightIcon;
		switch (Node.Type)
		{
		case EDwNodeType::Shop:  Glyph = ShopIcon; break;
		case EDwNodeType::Event: Glyph = EventIcon; break;
		case EDwNodeType::Boss:  Glyph = BossIcon; break;
		default: break;
		}
		NodeIcon->SetBrushFromTexture(Glyph);
		const float GlyphSize = Node.Type == EDwNodeType::Boss ? BossIconSize : IconSize;
		NodeIcon->SetDesiredSizeOverride(FVector2D(GlyphSize, GlyphSize));
		NodeIcon->SetColorAndOpacity(FLinearColor(IconColor.R, IconColor.G, IconColor.B, Node.State == EDwMapNodeState::Missed ? 0.35f : IconColor.A));
		NodeIcon->SetVisibility(Glyph ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		LabelText->SetVisibility(Glyph ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); // the glyph says it; the tooltip has the words
	}
	if (SailedMark)
	{
		SailedMark->SetVisibility(Node.State == EDwMapNodeState::Sailed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (HereTag)
	{
		HereTag->SetVisibility(Node.State == EDwMapNodeState::Current ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	SetRenderOpacity(Node.State == EDwMapNodeState::Missed ? 0.45f : 1.f);
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

FBox2D UDwMapScreen::GetNodeBox(const FDwMapNodeView& Node) const
{
	const FVector2D Centre = GetNodePosition(Node.Layer, Node.Lane) + NodeSize * 0.5f;
	const FVector2D Size = Node.Type == EDwNodeType::Boss ? NodeSize * BossScale : NodeSize;
	return FBox2D(Centre - Size * 0.5f, Centre + Size * 0.5f);
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
	GoldText->SetText(FText::AsNumber(Map->GetGold())); // beside the coin
	ShipText->SetText(Map->GetShipText());
	TArray<FText> Crew;
	for (const FDwCrewLineView& Line : Map->GetCrew())
	{
		Crew.Add(FText::Format(LOCTEXT("CrewLine", "{0}{1}   {2}"), Line.Name, Line.bIsCaptain ? LOCTEXT("Captain", " (captain)") : FText::GetEmpty(), Line.Detail));
	}
	FillLines(CrewList, Crew, 16, FLinearColor(0.95f, 0.92f, 0.85f));
	if (CrewPortraits)
	{
		CrewPortraits->ClearChildren();
		for (const FDwCrewLineView& Line : Map->GetCrew())
		{
			UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
			Frame->SetBrush(CrewPortraitFrame);
			Frame->SetPadding(FMargin(3.f));
			USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
			Box->SetWidthOverride(CrewPortraitSize);
			Box->SetHeightOverride(CrewPortraitSize);
			UImage* Face = WidgetTree->ConstructWidget<UImage>();
			Face->SetBrushFromSoftTexture(Line.Portrait);
			Face->SetColorAndOpacity(Line.bAlive ? FLinearColor::White : FLinearColor(0.35f, 0.35f, 0.35f, 0.7f));
			Box->SetContent(Face);
			Frame->SetContent(Box);
			Frame->SetToolTipText(FText::Format(LOCTEXT("CrewTip", "{0}   {1}"), Line.Name, Line.Detail));
			if (UHorizontalBoxSlot* FaceSlot = Cast<UHorizontalBoxSlot>(CrewPortraits->AddChild(Frame)))
			{
				FaceSlot->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
			}
		}
	}

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
			NodeWidgets.Add(Widget);
		}
	}
	for (int32 Index = 0; Index < NodeWidgets.Num() && Index < Nodes.Num(); ++Index)
	{
		NodeWidgets[Index]->SetNode(Nodes[Index], Map);
		if (UCanvasPanelSlot* NodeSlot = Cast<UCanvasPanelSlot>(NodeWidgets[Index]->Slot))
		{
			const FBox2D Box = GetNodeBox(Nodes[Index]);
			NodeSlot->SetPosition(Box.Min);
			NodeSlot->SetSize(Box.GetSize());
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
		const FBox2D FromBox = GetNodeBox(From);
		Extent = FVector2D::Max(Extent, FromBox.Max);
		for (const int32 ToIndex : From.Next)
		{
			const FDwMapNodeView* To = Map.FindNode(ToIndex);
			if (!To)
			{
				continue;
			}
			const FBox2D ToBox = GetNodeBox(*To);
			const bool bOpen = From.State == EDwMapNodeState::Current && To->State == EDwMapNodeState::Choosable;
			const bool bSailed = (From.State == EDwMapNodeState::Sailed || From.State == EDwMapNodeState::Current)
				&& (To->State == EDwMapNodeState::Sailed || To->State == EDwMapNodeState::Current);
			FDwMapRoute& Line = Lines.AddDefaulted_GetRef();
			Line.Start = FVector2D(FromBox.Max.X, FromBox.GetCenter().Y);
			Line.End = FVector2D(ToBox.Min.X, ToBox.GetCenter().Y);
			Line.Color = bOpen ? OpenRouteColor : (bSailed ? SailedRouteColor : RouteColor);
			Line.Thickness = bOpen || bSailed ? RouteThickness : FarRouteThickness;
			Line.bDashed = bDashUnsailed && !bSailed;
			Line.Bow = RouteBow;
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

namespace DwInk
{
	/** Draws a polyline (in Geometry's space) solid, or as ink dashes: Dash units drawn, Gap units skipped (Dash <= 0 = solid). */
	static void DrawLine(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geometry, const TArray<FVector2D>& Points,
		const FLinearColor& Color, float Thickness, float Dash, float Gap)
	{
		if (Points.Num() < 2)
		{
			return;
		}
		if (Dash <= 0.f)
		{
			FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, /*bAntialias*/ true, Thickness);
			return;
		}
		float Phase = 0.f;
		bool bInk = true;
		TArray<FVector2D> Piece = { Points[0] };
		for (int32 Index = 1; Index < Points.Num(); ++Index)
		{
			FVector2D From = Points[Index - 1];
			const FVector2D To = Points[Index];
			float Left = FVector2D::Distance(From, To);
			while (Left > 0.f)
			{
				const float Run = FMath::Min(Left, (bInk ? Dash : Gap) - Phase);
				const FVector2D Next = From + (To - From).GetSafeNormal() * Run;
				if (bInk)
				{
					Piece.Add(Next);
				}
				Phase += Run;
				Left -= Run;
				From = Next;
				if (Phase >= (bInk ? Dash : Gap) - KINDA_SMALL_NUMBER)
				{
					if (bInk && Piece.Num() > 1)
					{
						FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry.ToPaintGeometry(), Piece, ESlateDrawEffect::None, Color, /*bAntialias*/ true, Thickness);
					}
					bInk = !bInk;
					Phase = 0.f;
					Piece = { From };
				}
			}
		}
		if (bInk && Piece.Num() > 1)
		{
			FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry.ToPaintGeometry(), Piece, ESlateDrawEffect::None, Color, /*bAntialias*/ true, Thickness);
		}
	}
}

int32 UDwMapRoutesWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	constexpr int32 Steps = 24;
	for (const FDwMapRoute& Route : Routes)
	{
		// A quadratic curve bowing up by Route.Bow (already in this widget's space: it fills the chart from (0, 0)).
		const FVector2D Control = (Route.Start + Route.End) * 0.5f - FVector2D(0.f, Route.Bow * 2.f);
		TArray<FVector2D> Curve;
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			const float T = static_cast<float>(Step) / Steps;
			Curve.Add(FMath::Square(1.f - T) * Route.Start + 2.f * (1.f - T) * T * Control + FMath::Square(T) * Route.End);
		}
		DwInk::DrawLine(OutDrawElements, Layer, AllottedGeometry, Curve, Route.Color, Route.Thickness, Route.bDashed ? 10.f : 0.f, 8.f);
	}
	return Layer;
}

int32 UDwInkRuleWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const TArray<FVector2D> Points = { FVector2D(0.f, Size.Y * 0.5f), FVector2D(Size.X, Size.Y * 0.5f) };
	DwInk::DrawLine(OutDrawElements, Layer, AllottedGeometry, Points, Color, Thickness, Dash, Gap);
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
	FillChoices(OfferList, Rewards->GetOffers(), EDwChoiceStyle::Card);
	FillChoices(LearnerList, Rewards->GetLearners(), EDwChoiceStyle::Chip);
	FillChoices(ForgetList, Rewards->GetForget(), EDwChoiceStyle::Chip);
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
	FillChoices(CardList, LevelUp->GetCards(), EDwChoiceStyle::Card);
	if (!LevelUp->GetCards().IsEmpty())
	{
		PlayJuice(DwJuice::Highlight); // OnLevelUpShown hook (doc 14)
	}
}

void UDwShopScreen::FillSection(UWidget* Label, UPanelWidget* Panel, TConstArrayView<TObjectPtr<UDwChoiceViewModel>> Choices)
{
	FillChoices(Panel, Choices, EDwChoiceStyle::Ledger);
	if (Label)
	{
		Label->SetVisibility(Choices.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UDwShopScreen::NativeOnViewModelChanged()
{
	UDwShopViewModel* Shop = GetViewModel<UDwShopViewModel>();
	if (!Shop)
	{
		return;
	}
	GoldText->SetText(FText::AsNumber(Shop->GetGold())); // beside the coin
	ShipText->SetText(Shop->GetShipText());
	HoldText->SetText(Shop->GetHoldText());
	TArray<FText> Crew;
	for (const FDwCrewLineView& Line : Shop->GetCrew())
	{
		Crew.Add(FText::Format(LOCTEXT("CrewLine", "{0}{1}   {2}"), Line.Name, Line.bIsCaptain ? LOCTEXT("Captain", " (captain)") : FText::GetEmpty(), Line.Detail));
	}
	FillLines(CrewList, Crew, 16, FLinearColor(0.95f, 0.92f, 0.85f));

	FillSection(SupplyLabel, SupplyList, Shop->GetSupplies());
	MoveHintText->SetText(Shop->GetMoveHint());
	FillChoices(MoveList, Shop->GetMoves(), EDwChoiceStyle::Offer);
	FillChoices(LearnerList, Shop->GetLearners(), EDwChoiceStyle::Chip);
	FillChoices(ForgetList, Shop->GetForget(), EDwChoiceStyle::Chip);
	TArray<TObjectPtr<UDwChoiceViewModel>> Buy;
	if (UDwChoiceViewModel* BuyMove = Shop->GetBuyMove())
	{
		Buy.Add(BuyMove);
	}
	FillChoices(BuyMoveSlot, Buy);
	FillSection(RecruitLabel, RecruitList, Shop->GetRecruits());
	FillSection(UpgradeLabel, UpgradeList, Shop->GetUpgrades());
	FillSection(DisbandLabel, DisbandList, Shop->GetDisbands());
	const TArray<TObjectPtr<UDwChoiceViewModel>> Leave = { Shop->GetLeave() };
	FillChoices(LeaveSlot, Leave);

	// Juice hooks: the gold counter and the purchase (the buttons already played the purchase / error sound).
	if (ShownGold != INDEX_NONE && ShownGold != Shop->GetGold())
	{
		OnGoldChanged(ShownGold, Shop->GetGold());
	}
	ShownGold = Shop->GetGold();
	if (Shop->GetPurchaseCount() > ShownPurchases)
	{
		OnPurchase(Shop->GetLastResult());
	}
	ShownPurchases = Shop->GetPurchaseCount();
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
	FillChoices(ActionSlot, Shell->GetActions());
}

#undef LOCTEXT_NAMESPACE
