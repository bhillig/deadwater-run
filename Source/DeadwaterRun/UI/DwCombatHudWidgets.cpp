// Copyright Brandon Hillig 2026

#include "UI/DwCombatHudWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/DwGameplayTags.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UI/DwUIJuice.h"

#define LOCTEXT_NAMESPACE "DwCombatHud"

namespace DwHudWidgets
{
	FText Fraction(float Value, float Max)
	{
		return FText::Format(LOCTEXT("Fraction", "{0} / {1}"), FMath::RoundToInt(Value), FMath::RoundToInt(Max));
	}

	float Percent(float Value, float Max)
	{
		return Max > 0.f ? FMath::Clamp(Value / Max, 0.f, 1.f) : 0.f;
	}

	void SetFontSize(UTextBlock& Text, int32 Size)
	{
		FSlateFontInfo Font = Text.GetFont();
		Font.Size = Size;
		Text.SetFont(Font);
	}

	/** A small label in a coloured box (status chips). Created at runtime: their number changes with the fight. */
	UWidget* MakeChip(UWidgetTree& Tree, const FText& Label, int32 FontSize, const FLinearColor& Background)
	{
		UBorder* Chip = Tree.ConstructWidget<UBorder>();
		Chip->SetBrushColor(Background);
		Chip->SetPadding(FMargin(6.f, 1.f));
		Chip->SetVisibility(ESlateVisibility::HitTestInvisible);
		UTextBlock* Text = Tree.ConstructWidget<UTextBlock>();
		Text->SetText(Label);
		SetFontSize(*Text, FontSize);
		Chip->SetContent(Text);
		return Chip;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Unit card

void UDwUnitCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (CardButton)
	{
		CardButton->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
		CardButton->OnHovered.AddDynamic(this, &ThisClass::HandleHovered);
	}
}

void UDwUnitCardWidget::HandleClicked()
{
	UDwUnitViewModel* Unit = GetViewModel<UDwUnitViewModel>();
	if (Unit && Unit->GetView().Side == EDwSide::Player && Unit->GetView().bAlive)
	{
		PlayJuice(DwJuice::Press);
		PlayUISound(DwTags::UI_Sound_Click);
		Unit->Select();
	}
}

void UDwUnitCardWidget::HandleHovered()
{
	PlayJuice(DwJuice::Hover);
	PlayUISound(DwTags::UI_Sound_Hover);
}

void UDwUnitCardWidget::NativeOnViewModelChanged()
{
	using namespace DwHudWidgets;
	const UDwUnitViewModel* Unit = GetViewModel<UDwUnitViewModel>();
	if (!Unit)
	{
		return;
	}
	const FDwUnitView& View = Unit->GetView();
	const bool bAlly = View.Side == EDwSide::Player;

	NameText->SetText(View.Name);
	NameText->SetColorAndOpacity(FSlateColor(bAlly ? AllyColor : EnemyColor));
	if (!View.bAlive)
	{
		RoleText->SetText(LOCTEXT("Down", "Down"));
	}
	else if (View.bActing)
	{
		RoleText->SetText(LOCTEXT("Acting", "Acting"));
	}
	else
	{
		RoleText->SetText(View.bIsCaptain ? LOCTEXT("Captain", "Captain") : FText::GetEmpty());
	}
	RoleText->SetVisibility(RoleText->GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	// What this pirate can still do this turn (crew only: enemies act on their own turn).
	if (bAlly && View.bAlive)
	{
		// Only what is left: "Move  Action", "Move", "Action" or "Done".
		FText Budget = LOCTEXT("BudgetDone", "Done");
		if (View.bHasMove && View.bHasAction) { Budget = LOCTEXT("BudgetBoth", "Move  Action"); }
		else if (View.bHasMove)               { Budget = LOCTEXT("BudgetMove", "Move"); }
		else if (View.bHasAction)             { Budget = LOCTEXT("BudgetAction", "Action"); }
		BudgetText->SetText(Budget);
		BudgetText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		BudgetText->SetVisibility(ESlateVisibility::Collapsed);
	}

	HealthBar->SetPercent(Percent(View.Health, View.MaxHealth));
	HealthBar->SetFillColorAndOpacity(bAlly ? AllyColor : EnemyColor);
	HealthText->SetText(Fraction(View.Health, View.MaxHealth));
	ManaBar->SetPercent(Percent(View.Mana, View.MaxMana));
	ManaBar->SetFillColorAndOpacity(ManaColor);
	ManaText->SetText(Fraction(View.Mana, View.MaxMana));

	// Enemy Fear as pips out of MaxFear (doc 14 "Readability rules").
	const int32 MaxPips = bAlly ? 0 : FMath::RoundToInt(View.MaxFear);
	FearRow->SetVisibility(MaxPips > 0 && View.bAlive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (FearPips->GetChildrenCount() != MaxPips)
	{
		FearPips->ClearChildren();
		for (int32 Pip = 0; Pip < MaxPips; ++Pip)
		{
			USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
			Box->SetWidthOverride(16.f);
			Box->SetHeightOverride(16.f);
			Box->SetContent(WidgetTree->ConstructWidget<UBorder>());
			if (UHorizontalBoxSlot* PipSlot = FearPips->AddChildToHorizontalBox(Box))
			{
				PipSlot->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
				PipSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}
	for (int32 Pip = 0; Pip < FearPips->GetChildrenCount(); ++Pip)
	{
		const USizeBox* Box = Cast<USizeBox>(FearPips->GetChildAt(Pip));
		if (UBorder* Fill = Box ? Cast<UBorder>(Box->GetContent()) : nullptr)
		{
			Fill->SetBrushColor(Pip < FMath::RoundToInt(View.Fear) ? FearOnColor : FearOffColor);
		}
	}

	// Statuses with turns left ("Frozen 1"). Placeholder text chips until status icons exist (art pass).
	StatusList->ClearChildren();
	for (const FDwStatusView& Status : View.Statuses)
	{
		const FText Label = Status.TurnsRemaining > 0
			? FText::Format(LOCTEXT("StatusTurns", "{0} {1}"), Status.Label, Status.TurnsRemaining) : Status.Label;
		StatusList->AddChild(MakeChip(*WidgetTree, Label, StatusFontSize, FLinearColor(0.3f, 0.22f, 0.05f, 0.95f)));
	}
	StatusList->SetVisibility(View.Statuses.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	FLinearColor FrameTint = FrameColor;
	if (View.bActing)        { FrameTint = ActingFrameColor; }
	else if (View.bSelected) { FrameTint = SelectedFrameColor; }
	else if (View.bHovered)  { FrameTint = HoveredFrameColor; }
	Frame->SetBrushColor(FrameTint);
	SetRenderOpacity(View.bAlive ? 1.f : 0.45f);

	if (View.bSelected && !bWasSelected)
	{
		PlayJuice(DwJuice::Highlight);
	}
	bWasSelected = View.bSelected;
	if (LastHealth >= 0.f && !FMath::IsNearlyEqual(LastHealth, View.Health))
	{
		OnHealthChanged(LastHealth, View.Health);
	}
	LastHealth = View.Health;
}

// ---------------------------------------------------------------------------------------------------------------------
// Action button

void UDwActionButtonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (ActionButton)
	{
		ActionButton->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
		ActionButton->OnHovered.AddDynamic(this, &ThisClass::HandleHovered);
	}
}

void UDwActionButtonWidget::HandleClicked()
{
	if (UDwActionViewModel* Action = GetViewModel<UDwActionViewModel>())
	{
		PlayJuice(DwJuice::Press);
		PlayUISound(Action->Select() ? DwTags::UI_Sound_Click : DwTags::UI_Sound_Error);
	}
}

void UDwActionButtonWidget::HandleHovered()
{
	PlayJuice(DwJuice::Hover);
	PlayUISound(DwTags::UI_Sound_Hover);
}

void UDwActionButtonWidget::NativeOnViewModelChanged()
{
	const UDwActionViewModel* Action = GetViewModel<UDwActionViewModel>();
	if (!Action)
	{
		return;
	}
	const FDwActionView& View = Action->GetView();
	HotkeyText->SetText(View.Hotkey);
	NameText->SetText(View.Count > 0 ? FText::Format(LOCTEXT("ActionCount", "{0} x{1}"), View.Name, View.Count) : View.Name);
	CostText->SetText(View.CostText);
	CostText->SetVisibility(View.CostText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	ReasonText->SetText(View.DisabledReason);
	ReasonText->SetVisibility(View.DisabledReason.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	// Unavailable slots stay clickable (the click reports the same reason on the HUD) and keep their reason text at
	// full contrast; only the background and the name dim. Nothing here decides availability: the view model does.
	ActionButton->SetBackgroundColor(View.bArmed ? ArmedColor : (View.bEnabled ? ReadyColor : DisabledColor));
	NameText->SetColorAndOpacity(FSlateColor(View.bEnabled || View.bArmed ? NameColor : DisabledNameColor));
	ActionButton->SetToolTipText(View.DisabledReason.IsEmpty() ? View.Description
		: FText::Format(LOCTEXT("TooltipReason", "{0}\n{1}"), View.Description, View.DisabledReason));
	if (bWasEnabled && !View.bEnabled)
	{
		PlayJuice(DwJuice::Disabled);
	}
	if (View.bArmed && !bWasArmed)
	{
		PlayJuice(DwJuice::Highlight);
	}
	bWasEnabled = View.bEnabled;
	bWasArmed = View.bArmed;
}

// ---------------------------------------------------------------------------------------------------------------------
// Combat HUD

UDwCombatHudScreen::UDwCombatHudScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Game and UI both get input: board clicks, 1-6, Tab, Esc and Space stay with Enhanced Input (ADwPlayerController).
	// CaptureDuringMouseDown, like the FInputModeGameAndUI the controller sets: with NoCapture the scene viewport never
	// forwards mouse buttons to the game, so board clicks (IA_Select / IA_Cancel) would be lost (Tools/InputProbe.ps1).
	InputMode = ECommonInputMode::All;
	MouseCaptureMode = EMouseCaptureMode::CaptureDuringMouseDown;
}

void UDwCombatHudScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (EndTurnButton)
	{
		EndTurnButton->OnClicked.AddDynamic(this, &ThisClass::HandleEndTurnClicked);
	}
	if (TurnBanner)
	{
		TurnBanner->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UDwCombatHudScreen::NativeDestruct()
{
	if (UDwCombatHudViewModel* Hud = BoundHud.Get())
	{
		Hud->OnTurnBanner.RemoveAll(this);
	}
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BannerTimer);
	}
	Super::NativeDestruct();
}

void UDwCombatHudScreen::HandleEndTurnClicked()
{
	if (UDwCombatHudViewModel* Hud = GetViewModel<UDwCombatHudViewModel>())
	{
		PlayUISound(DwTags::UI_Sound_Confirm);
		Hud->EndTurn();
	}
}

bool UDwCombatHudScreen::IsTurnBannerVisible() const
{
	return TurnBanner && TurnBanner->GetVisibility() != ESlateVisibility::Collapsed;
}

void UDwCombatHudScreen::HandleTurnBanner(EDwSide Side, int32 Round)
{
	TurnBannerText->SetText(Side == EDwSide::Player
		? FText::Format(LOCTEXT("BannerPlayer", "Round {0} - Your turn"), Round)
		: FText::Format(LOCTEXT("BannerEnemy", "Round {0} - Enemy turn"), Round));
	TurnBannerText->SetColorAndOpacity(FSlateColor(Side == EDwSide::Player ? AllyColor : EnemyColor));
	TurnBanner->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(BannerTimer, FTimerDelegate::CreateUObject(this, &ThisClass::HideTurnBanner), TurnBannerSeconds, false);
	}
	if (Side == EDwSide::Player)
	{
		PlayUISound(DwTags::UI_Sound_TurnStart);
	}
	OnTurnBannerShown(Side);
}

void UDwCombatHudScreen::HideTurnBanner()
{
	TurnBanner->SetVisibility(ESlateVisibility::Collapsed);
}

template <typename TWidget, typename TViewModel>
void UDwCombatHudScreen::SyncList(UPanelWidget& Panel, TSubclassOf<TWidget> EntryClass, const TArray<TObjectPtr<TViewModel>>& ViewModels)
{
	if (!EntryClass)
	{
		return;
	}
	while (Panel.GetChildrenCount() > ViewModels.Num())
	{
		Panel.RemoveChildAt(Panel.GetChildrenCount() - 1);
	}
	while (Panel.GetChildrenCount() < ViewModels.Num())
	{
		UPanelSlot* EntrySlot = Panel.AddChild(CreateWidget<TWidget>(this, EntryClass));
		if (UVerticalBoxSlot* Row = Cast<UVerticalBoxSlot>(EntrySlot))
		{
			Row->SetPadding(FMargin(0.f, 0.f, 0.f, EntrySpacing));
		}
		else if (UHorizontalBoxSlot* Column = Cast<UHorizontalBoxSlot>(EntrySlot))
		{
			Column->SetPadding(FMargin(0.f, 0.f, EntrySpacing, 0.f));
			Column->SetVerticalAlignment(VAlign_Fill); // every card / action button as tall as the tallest
		}
	}
	for (int32 Index = 0; Index < ViewModels.Num(); ++Index)
	{
		TWidget* Entry = Cast<TWidget>(Panel.GetChildAt(Index));
		if (Entry && Entry->GetViewModel() != ViewModels[Index])
		{
			Entry->SetViewModel(ViewModels[Index]);
		}
	}
}

void UDwCombatHudScreen::NativeOnViewModelChanged()
{
	UDwCombatHudViewModel* Hud = GetViewModel<UDwCombatHudViewModel>();
	if (BoundHud.Get() != Hud)
	{
		if (UDwCombatHudViewModel* Old = BoundHud.Get())
		{
			Old->OnTurnBanner.RemoveAll(this);
		}
		BoundHud = Hud;
		if (Hud)
		{
			Hud->OnTurnBanner.AddDynamic(this, &ThisClass::HandleTurnBanner);
		}
	}
	if (!Hud)
	{
		return;
	}

	TurnText->SetText(Hud->GetTurnText());
	TurnText->SetColorAndOpacity(FSlateColor(Hud->IsPlayerTurn() ? AllyColor : EnemyColor));
	RefreshShips(*Hud);
	SyncList<UDwUnitCardWidget, UDwUnitViewModel>(*CrewList, UnitCardClass, Hud->GetCrew());
	SyncList<UDwUnitCardWidget, UDwUnitViewModel>(*EnemyList, UnitCardClass, Hud->GetEnemies());
	SyncList<UDwActionButtonWidget, UDwActionViewModel>(*ActionBar, ActionButtonClass, Hud->GetActions());
	ActionBar->SetVisibility(Hud->IsPlayerTurn() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	// End Turn: enabled when the player may act; the "End turn?" prompt is non-modal and never ends the turn itself.
	const FDwHudKeys& Keys = Hud->GetKeys();
	EndTurnButton->SetIsEnabled(Hud->CanAct());
	EndTurnButton->SetBackgroundColor(Hud->IsEndTurnSuggested() ? EndTurnSuggestedColor : EndTurnColor);
	EndTurnKeyText->SetText(Keys.EndTurn);
	EndTurnKeyText->SetVisibility(Keys.EndTurn.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	EndTurnPrompt->SetVisibility(Hud->IsEndTurnSuggested() && Hud->CanAct() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	RefreshPrompt(*Hud);

	OutcomeText->SetText(Hud->GetOutcomeText());
	OutcomeBanner->SetVisibility(Hud->GetOutcome() == EDwCombatOutcome::None ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	ControlsText->SetText(FText::Format(
		LOCTEXT("Controls", "{0} select / move / attack     1-6 actions     {1} cancel     {2} next pirate     {3} end turn     WASD pan     Wheel zoom"),
		Keys.Select, Keys.Cancel, Keys.NextUnit, Keys.EndTurn));
}

void UDwCombatHudScreen::RefreshShips(const UDwCombatHudViewModel& Hud)
{
	using namespace DwHudWidgets;
	const FDwShipView& Ours = Hud.GetPlayerShip();
	const FDwShipView& Theirs = Hud.GetEnemyShip();
	PlayerShipBar->SetPercent(Percent(Ours.Health, Ours.MaxHealth));
	PlayerShipBar->SetFillColorAndOpacity(AllyColor);
	PlayerShipText->SetText(Fraction(Ours.Health, Ours.MaxHealth));
	EnemyShipPanel->SetVisibility(Theirs.bPresent ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	EnemyShipBar->SetPercent(Percent(Theirs.Health, Theirs.MaxHealth));
	EnemyShipBar->SetFillColorAndOpacity(EnemyColor);
	EnemyShipText->SetText(Fraction(Theirs.Health, Theirs.MaxHealth));

	for (const FDwShipView* Ship : { &Ours, &Theirs })
	{
		float& Last = LastShipHealth[Ship->Side == EDwSide::Player ? 0 : 1];
		if (Last >= 0.f && !FMath::IsNearlyEqual(Last, Ship->Health))
		{
			OnShipHealthChanged(Ship->Side, Last, Ship->Health);
		}
		Last = Ship->Health;
	}
}

void UDwCombatHudScreen::RefreshPrompt(const UDwCombatHudViewModel& Hud)
{
	HintText->SetText(Hud.GetHint());
	HintText->SetVisibility(Hud.GetHint().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	MessageText->SetText(Hud.GetMessage());
	MessageText->SetVisibility(Hud.GetMessage().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	// Damage preview before every confirm (C13): one line per unit or ship the action would hit.
	PreviewList->ClearChildren();
	for (const FDwPreviewLine& Line : Hud.GetPreview())
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
		const FText Lethal = Line.bLethal ? LOCTEXT("Lethal", "  LETHAL") : FText::GetEmpty();
		Text->SetText(FText::Format(LOCTEXT("PreviewLine", "{0}: {1}{2}"), Line.Target, Line.Effect, Lethal));
		Text->SetColorAndOpacity(FSlateColor(Line.bLethal ? LethalColor : PreviewColor));
		Text->SetShadowOffset(FVector2D(1.f, 1.f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
		DwHudWidgets::SetFontSize(*Text, PreviewFontSize);
		PreviewList->AddChildToVerticalBox(Text);
	}
	PreviewList->SetVisibility(Hud.GetPreview().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

#undef LOCTEXT_NAMESPACE
