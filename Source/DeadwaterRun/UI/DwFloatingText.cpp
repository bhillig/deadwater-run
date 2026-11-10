// Copyright Brandon Hillig 2026

#include "UI/DwFloatingText.h"

#include "Blueprint/UserWidget.h"
#include "Combat/DwCombatSubsystem.h"
#include "Combat/DwCombatUnit.h"
#include "Combat/DwShip.h"
#include "Components/TextBlock.h"
#include "Core/DwGameplayTags.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UI/DwUIJuice.h"

#define LOCTEXT_NAMESPACE "DwFloatingText"

namespace DwFloatingText
{
	/** Height above a unit's tile / a ship's origin the label is anchored at (world units): chest height, so numbers sit
	 *  on the unit instead of drifting up into the HUD's card rows for units in the back row. */
	constexpr float UnitHeight = 120.f;
	constexpr float ShipHeight = 160.f;
	/** Vertical gap between labels stacked over the same target (Slate units). */
	constexpr float StackSpacing = 34.f;
}

// ---------------------------------------------------------------------------------------------------------------------
// Widget

UDwFloatingTextWidget::UDwFloatingTextWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Placeholder palette (U6 art direction pending): red damage, green heal, gold status, violet fear.
	Colors.Add(EDwFloatingTextKind::Damage, FLinearColor(1.f, 0.32f, 0.25f));
	Colors.Add(EDwFloatingTextKind::Heal, FLinearColor(0.4f, 1.f, 0.45f));
	Colors.Add(EDwFloatingTextKind::Status, FLinearColor(1.f, 0.85f, 0.35f));
	Colors.Add(EDwFloatingTextKind::Fear, FLinearColor(0.8f, 0.55f, 1.f));
}

void UDwFloatingTextWidget::Show(const FDwFloatingTextRequest& Request, const FVector& WorldAnchor, int32 StackIndex)
{
	Current = Request;
	Anchor = WorldAnchor;
	Stack = StackIndex;
	Age = 0.f;
	bShowing = true;
	if (ValueText)
	{
		ValueText->SetText(Request.Text);
		if (const FLinearColor* Color = Colors.Find(Request.Kind))
		{
			ValueText->SetColorAndOpacity(FSlateColor(*Color));
		}
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UpdatePosition();
	PlayJuice(DwJuice::Intro);
	OnShown(Request.Kind);
}

void UDwFloatingTextWidget::Hide()
{
	bShowing = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UDwFloatingTextWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bShowing)
	{
		return;
	}
	Age += InDeltaTime;
	if (Age >= Lifetime)
	{
		Hide();
		return;
	}
	UpdatePosition();
}

void UDwFloatingTextWidget::UpdatePosition()
{
	const APlayerController* PC = GetOwningPlayer();
	FVector2D Screen;
	if (PC && PC->ProjectWorldLocationToScreen(Anchor, Screen, /*bPlayerViewportRelative*/ true))
	{
		SetAlignmentInViewport(FVector2D(0.5f, 1.f));
		SetPositionInViewport(Screen, /*bRemoveDPIScale*/ true);
		if (Stack > 0)
		{
			// Lift in Slate units (DPI-scaled), so stacked labels never overlap at any resolution.
			SetRenderTranslation(FVector2D(0.f, -DwFloatingText::StackSpacing * Stack));
		}
		else
		{
			SetRenderTranslation(FVector2D::ZeroVector);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Manager

void UDwFloatingTextManager::Initialize(UDwCombatSubsystem& InCombat, APlayerController* InOwner, TSubclassOf<UDwFloatingTextWidget> InWidgetClass)
{
	Shutdown();
	Combat = &InCombat;
	Owner = InOwner;
	WidgetClass = InWidgetClass;
	InCombat.OnCombatEvent.AddDynamic(this, &ThisClass::HandleCombatEvent);
}

void UDwFloatingTextManager::Shutdown()
{
	if (UDwCombatSubsystem* Fight = Combat.Get())
	{
		Fight->OnCombatEvent.RemoveAll(this);
	}
	Combat = nullptr;
	for (UDwFloatingTextWidget* Widget : Pool)
	{
		if (Widget)
		{
			Widget->RemoveFromParent();
		}
	}
	Pool.Reset();
	NextRecycle = 0;
}

int32 UDwFloatingTextManager::GetShowingCount() const
{
	int32 Count = 0;
	for (const UDwFloatingTextWidget* Widget : Pool)
	{
		Count += Widget && Widget->IsShowing() ? 1 : 0;
	}
	return Count;
}

bool UDwFloatingTextManager::Describe(const FDwCombatEvent& Event, FDwFloatingTextRequest& OutRequest)
{
	OutRequest = FDwFloatingTextRequest();
	OutRequest.Unit = Event.Target;
	const int32 Amount = FMath::RoundToInt(Event.Magnitude);
	switch (Event.Type)
	{
	case EDwCombatEventType::UnitDamaged:
		OutRequest.Kind = EDwFloatingTextKind::Damage;
		OutRequest.Text = FText::Format(LOCTEXT("Damage", "-{0}"), Amount);
		return Amount > 0;
	case EDwCombatEventType::UnitHealed:
		OutRequest.Kind = EDwFloatingTextKind::Heal;
		OutRequest.Text = FText::Format(LOCTEXT("Heal", "+{0}"), Amount);
		return Amount > 0;
	case EDwCombatEventType::ShipDamaged:
		OutRequest.Kind = EDwFloatingTextKind::Damage;
		OutRequest.Unit = FDwUnitHandle();
		OutRequest.ShipSide = Event.Side;
		OutRequest.Text = FText::Format(LOCTEXT("ShipDamage", "-{0}"), Amount);
		return Amount > 0;
	case EDwCombatEventType::FearChanged:
		// Fear going down (cower reset) is not news; going up is.
		OutRequest.Kind = EDwFloatingTextKind::Fear;
		OutRequest.Text = FText::Format(LOCTEXT("Fear", "+{0} Fear"), Amount);
		return Amount > 0;
	case EDwCombatEventType::StatusApplied:
	{
		OutRequest.Kind = EDwFloatingTextKind::Status;
		// Doc 14 names "Frozen!"; the others follow the same pattern (placeholder copy).
		if (Event.Tag.MatchesTagExact(DwTags::Status_Incapacitated_Frozen))        { OutRequest.Text = LOCTEXT("Frozen", "Frozen!"); }
		else if (Event.Tag.MatchesTagExact(DwTags::Status_Incapacitated_Cowering)) { OutRequest.Text = LOCTEXT("Cowering", "Cowering!"); }
		else if (Event.Tag.MatchesTagExact(DwTags::Status_Rooted))                 { OutRequest.Text = LOCTEXT("Rooted", "Rooted!"); }
		else if (Event.Tag.MatchesTagExact(DwTags::Status_Afraid))                 { OutRequest.Text = LOCTEXT("Afraid", "Afraid!"); }
		else if (Event.Tag.MatchesTagExact(DwTags::Status_Guarded))                { OutRequest.Text = LOCTEXT("Guarded", "Guarded"); }
		else
		{
			FString Leaf = Event.Tag.ToString();
			Leaf.Split(TEXT("."), nullptr, &Leaf, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			OutRequest.Text = FText::Format(LOCTEXT("OtherStatus", "{0}!"), FText::FromString(Leaf));
		}
		return Event.Tag.IsValid();
	}
	default:
		return false;
	}
}

TOptional<FVector> UDwFloatingTextManager::AnchorFor(const FDwFloatingTextRequest& Request) const
{
	const UDwCombatSubsystem* Fight = Combat.Get();
	if (!Fight)
	{
		return TOptional<FVector>();
	}
	if (Request.Unit.IsValid())
	{
		if (const ADwCombatUnit* Unit = Fight->FindUnit(Request.Unit))
		{
			return Unit->GetActorLocation() + FVector(0.f, 0.f, DwFloatingText::UnitHeight);
		}
	}
	else if (const ADwShip* Ship = Fight->GetShip(Request.ShipSide))
	{
		return Ship->GetActorLocation() + FVector(0.f, 0.f, DwFloatingText::ShipHeight);
	}
	return TOptional<FVector>();
}

UDwFloatingTextWidget* UDwFloatingTextManager::Acquire()
{
	for (UDwFloatingTextWidget* Widget : Pool)
	{
		if (Widget && !Widget->IsShowing())
		{
			return Widget;
		}
	}
	if (Pool.Num() < PoolSize)
	{
		APlayerController* PC = Owner.Get();
		UDwFloatingTextWidget* Widget = PC ? CreateWidget<UDwFloatingTextWidget>(PC, WidgetClass)
			: CreateWidget<UDwFloatingTextWidget>(Combat->GetWorld(), WidgetClass);
		if (Widget)
		{
			// Above the primary layout (z 1000): units in the back row float their numbers up where the HUD's card row
			// is, and feedback must stay readable. Labels never take input and last ~1 s.
			if (PC && PC->GetLocalPlayer() && Combat->GetWorld()->GetGameViewport())
			{
				Widget->AddToPlayerScreen(1100);
			}
			Widget->Hide();
			Pool.Add(Widget);
		}
		return Widget;
	}
	// Pool full: recycle the oldest label.
	UDwFloatingTextWidget* Oldest = Pool[NextRecycle];
	NextRecycle = (NextRecycle + 1) % Pool.Num();
	return Oldest;
}

void UDwFloatingTextManager::HandleCombatEvent(const FDwCombatEvent& Event)
{
	FDwFloatingTextRequest Request;
	if (!WidgetClass || !Describe(Event, Request))
	{
		return;
	}
	const TOptional<FVector> Anchor = AnchorFor(Request);
	if (!Anchor.IsSet())
	{
		return;
	}
	int32 Stack = 0;
	for (const UDwFloatingTextWidget* Other : Pool)
	{
		const FDwFloatingTextRequest& Shown = Other->GetRequest();
		Stack += Other->IsShowing() && Shown.Unit == Request.Unit && Shown.ShipSide == Request.ShipSide ? 1 : 0;
	}
	if (UDwFloatingTextWidget* Widget = Acquire())
	{
		Widget->Show(Request, Anchor.GetValue(), Widget->IsShowing() ? 0 : Stack);
		++ShownTotal;
	}
}

#undef LOCTEXT_NAMESPACE
