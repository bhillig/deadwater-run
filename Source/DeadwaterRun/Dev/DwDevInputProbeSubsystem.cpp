// Copyright Brandon Hillig 2026

#include "Dev/DwDevInputProbeSubsystem.h"

#include "Blueprint/WidgetTree.h"
#include "Board/DwBoard.h"
#include "Combat/DwCombatInteraction.h"
#include "Combat/DwCombatSubsystem.h"
#include "Combat/DwCombatUnit.h"
#include "Core/DwLog.h"
#include "Core/DwPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "UI/DwActivatableScreen.h"
#include "UI/DwUIManagerSubsystem.h"
#include "UnrealClient.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

bool UDwDevInputProbeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return Super::ShouldCreateSubsystem(Outer) && FParse::Param(FCommandLine::Get(), TEXT("DwInputProbe"));
#endif
}

bool UDwDevInputProbeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game;
}

ADwPlayerController* UDwDevInputProbeSubsystem::GetPlayer() const
{
	return GetWorld() ? Cast<ADwPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
}

UDwCombatInteraction* UDwDevInputProbeSubsystem::GetInteraction() const
{
	const ADwPlayerController* PC = GetPlayer();
	return PC ? PC->GetInteraction() : nullptr;
}

UDwCombatSubsystem* UDwDevInputProbeSubsystem::GetCombat() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<UDwCombatSubsystem>() : nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
// Injection (the same Slate entry points the platform layer calls for hardware input)

void UDwDevInputProbeSubsystem::PressKey(const FKey& Key)
{
	FSlateApplication& Slate = FSlateApplication::Get();
	const FKeyEvent Event(Key, Slate.GetModifierKeys(), /*UserIndex*/ 0, /*bIsRepeat*/ false, /*CharacterCode*/ 0, /*KeyCode*/ 0);
	Slate.ProcessKeyDownEvent(Event);
	Slate.ProcessKeyUpEvent(Event);
}

void UDwDevInputProbeSubsystem::MoveMouse(const FVector2D& Screen)
{
	FSlateApplication& Slate = FSlateApplication::Get();
	Slate.SetCursorPos(Screen); // the scene viewport re-reads the OS cursor every frame
	const FPointerEvent Event(/*PointerIndex*/ 0, Screen, LastScreen, TSet<FKey>(), EKeys::Invalid, 0.f, Slate.GetModifierKeys());
	Slate.ProcessMouseMoveEvent(Event);
	LastScreen = Screen;

	// Which widget a click here lands on (the HUD must let board clicks through to the game viewport).
	const FWidgetPath Path = Slate.LocateWindowUnderMouse(Screen, Slate.GetInteractiveTopLevelWindows());
	UE_LOG(LogDw, Display, TEXT("DWINPUT cursor (%.0f, %.0f) over %s"), Screen.X, Screen.Y,
		Path.IsValid() ? *Path.Widgets.Last().Widget->GetTypeAsString() : TEXT("nothing"));
}

void UDwDevInputProbeSubsystem::ClickMouse(const FKey& Button)
{
	FSlateApplication& Slate = FSlateApplication::Get();
	const TSharedPtr<SWindow> Window = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
	const FPointerEvent Down(0, LastScreen, LastScreen, TSet<FKey>({ Button }), Button, 0.f, Slate.GetModifierKeys());
	const FPointerEvent Up(0, LastScreen, LastScreen, TSet<FKey>(), Button, 0.f, Slate.GetModifierKeys());
	Slate.ProcessMouseButtonDownEvent(Window ? Window->GetNativeWindow() : nullptr, Down);
	Slate.ProcessMouseButtonUpEvent(Up);
}

TOptional<FVector2D> UDwDevInputProbeSubsystem::TileOnScreen(FIntPoint Tile) const
{
	const ADwPlayerController* PC = GetPlayer();
	const UDwCombatSubsystem* Combat = GetCombat();
	const ADwBoard* Board = Combat ? Combat->GetBoard() : nullptr;
	const TSharedPtr<SViewport> Viewport = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetGameViewportWidget() : nullptr;
	FVector2D Pixel;
	if (!PC || !Board || !Viewport || !GEngine->GameViewport->Viewport
		|| !PC->ProjectWorldLocationToScreen(Board->TileToWorld(Tile), Pixel, /*bPlayerViewportRelative*/ true))
	{
		return TOptional<FVector2D>();
	}
	// Viewport pixels -> the viewport widget's local (Slate) space -> absolute desktop coordinates.
	const FGeometry& Geometry = Viewport->GetCachedGeometry();
	const FVector2D PixelSize(GEngine->GameViewport->Viewport->GetSizeXY());
	const FVector2D Local = Pixel * FVector2D(Geometry.GetLocalSize()) / PixelSize;
	return FVector2D(Geometry.LocalToAbsolute(Local));
}

TOptional<FVector2D> UDwDevInputProbeSubsystem::HudWidgetOnScreen(FName WidgetName) const
{
	const ADwPlayerController* PC = GetPlayer();
	const UDwUIManagerSubsystem* Manager = PC ? UDwUIManagerSubsystem::Get(PC->GetLocalPlayer()) : nullptr;
	const UDwActivatableScreen* Hud = Manager ? Manager->GetCombatHudScreen() : nullptr;
	const UWidget* Widget = Hud && Hud->WidgetTree ? Hud->WidgetTree->FindWidget(WidgetName) : nullptr;
	if (!Widget)
	{
		return TOptional<FVector2D>();
	}
	const FGeometry& Geometry = Widget->GetCachedGeometry();
	return FVector2D(Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f));
}

// ---------------------------------------------------------------------------------------------------------------------
// Steps

void UDwDevInputProbeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	UE_LOG(LogDw, Log, TEXT("DWINPUT probe armed"));

	auto PlayerTurn = [this](FString& Detail)
	{
		const UDwCombatSubsystem* Combat = GetCombat();
		const UDwCombatInteraction* Interaction = GetInteraction();
		Detail = TEXT("combat not running or not the player's turn");
		return Combat && Combat->IsActive() && Interaction && Interaction->CanAct();
	};
	auto Mode = [this](EDwInteractionMode Expected, FString& Detail)
	{
		const UDwCombatInteraction* Interaction = GetInteraction();
		Detail = FString::Printf(TEXT("interaction mode %s"), Interaction ? *StaticEnum<EDwInteractionMode>()->GetNameStringByValue(static_cast<int64>(Interaction->GetMode())) : TEXT("none"));
		return Interaction && Interaction->GetMode() == Expected;
	};
	auto EndTurnRecorded = [this](FString& Detail)
	{
		const UDwCombatSubsystem* Combat = GetCombat();
		const TArray<FDwCombatCommand>& Record = Combat->GetRecord().Commands;
		for (int32 Index = Commands; Index < Record.Num(); ++Index)
		{
			if (Record[Index].Type == EDwCommandType::EndTurn && Record[Index].Side == EDwSide::Player)
			{
				return true;
			}
		}
		Detail = TEXT("no player EndTurn command reached the combat subsystem");
		return false;
	};
	TSharedRef<FDwUnitHandle> Previous = MakeShared<FDwUnitHandle>();
	TSharedRef<FDwUnitHandle> Other = MakeShared<FDwUnitHandle>();

	Steps.Add({ TEXT("player turn ready"), nullptr, 0.5f, PlayerTurn, 20 });

	Steps.Add({ TEXT("Tab selects the next pirate (IA_NextUnit)"),
		[this, Previous]() { *Previous = GetInteraction()->GetSelectedUnit(); PressKey(EKeys::Tab); }, 0.4f,
		[this, Previous](FString& Detail)
		{
			Detail = FString::Printf(TEXT("selection stayed %d"), Previous->Id);
			return GetInteraction()->GetSelectedUnit().IsValid() && GetInteraction()->GetSelectedUnit() != *Previous;
		} });

	const FKey SlotKeys[6] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six };
	auto ArmKey = [this, SlotKeys]()
	{
		const TArray<FDwActionSlot> Bar = GetInteraction()->GetActionBar();
		ArmSlot = Bar.IndexOfByPredicate([](const FDwActionSlot& Slot) { return Slot.bAvailable; });
		if (ArmSlot != INDEX_NONE && ArmSlot < 6)
		{
			PressKey(SlotKeys[ArmSlot]);
		}
	};
	Steps.Add({ TEXT("number key arms an action (IA_AbilityN)"), ArmKey, 0.4f,
		[Mode](FString& Detail) { return Mode(EDwInteractionMode::Targeting, Detail); } });
	Steps.Add({ TEXT("Esc cancels targeting (IA_Pause -> Cancel)"), [this]() { PressKey(EKeys::Escape); }, 0.4f,
		[Mode](FString& Detail) { return Mode(EDwInteractionMode::Move, Detail); } });
	Steps.Add({ TEXT("number key re-arms the action"), ArmKey, 0.4f,
		[Mode](FString& Detail) { return Mode(EDwInteractionMode::Targeting, Detail); } });
	Steps.Add({ TEXT("right-click cancels targeting (IA_Cancel)"),
		[this]()
		{
			// Over the board's own deck (nothing on that tile reacts to a right-click).
			if (const TOptional<FVector2D> Point = TileOnScreen(GetInteraction()->GetReachable().IsEmpty() ? FIntPoint(2, 1) : GetInteraction()->GetReachable()[0]))
			{
				MoveMouse(Point.GetValue());
			}
			ClickMouse(EKeys::RightMouseButton);
		}, 0.4f,
		[Mode](FString& Detail) { return Mode(EDwInteractionMode::Move, Detail); } });

	Steps.Add({ TEXT("hovering a reachable tile previews the path"),
		[this]()
		{
			const TArray<FIntPoint>& Reach = GetInteraction()->GetReachable();
			MoveTarget = Reach.IsEmpty() ? FIntPoint(-1, -1) : Reach.Last();
			if (const TOptional<FVector2D> Point = TileOnScreen(MoveTarget))
			{
				MoveMouse(Point.GetValue());
			}
		}, 0.4f,
		[this](FString& Detail)
		{
			const TOptional<FIntPoint> Hovered = GetInteraction()->GetHoveredTile();
			Detail = FString::Printf(TEXT("hovered %s, wanted %s"), Hovered.IsSet() ? *Hovered.GetValue().ToString() : TEXT("nothing"), *MoveTarget.ToString());
			return Hovered.IsSet() && Hovered.GetValue() == MoveTarget && !GetInteraction()->GetPath().IsEmpty();
		} });
	Steps.Add({ TEXT("left-click moves the selected pirate there (IA_Select)"),
		[this, Previous]() { *Previous = GetInteraction()->GetSelectedUnit(); ClickMouse(EKeys::LeftMouseButton); }, 2.5f,
		[this, Previous](FString& Detail)
		{
			const ADwCombatUnit* Unit = GetCombat()->FindUnit(*Previous);
			Detail = FString::Printf(TEXT("unit at %s, wanted %s"), Unit ? *Unit->GetTile().ToString() : TEXT("?"), *MoveTarget.ToString());
			return Unit && Unit->GetTile() == MoveTarget;
		} });

	Steps.Add({ TEXT("cursor over another pirate"),
		[this, Other]()
		{
			const FDwCombatSnapshot Snapshot = GetCombat()->GetSnapshot();
			for (const FDwUnitSnapshot& Unit : Snapshot.Units)
			{
				if (Unit.Side == EDwSide::Player && Unit.bAlive && Unit.Handle != GetInteraction()->GetSelectedUnit())
				{
					*Other = Unit.Handle;
					if (const TOptional<FVector2D> Point = TileOnScreen(Unit.Tile))
					{
						MoveMouse(Point.GetValue());
					}
					break;
				}
			}
		}, 0.3f, nullptr });
	Steps.Add({ TEXT("left-click on another pirate selects it (IA_Select)"), [this]() { ClickMouse(EKeys::LeftMouseButton); }, 0.4f,
		[this, Other](FString& Detail)
		{
			Detail = FString::Printf(TEXT("selected %d, wanted %d"), GetInteraction()->GetSelectedUnit().Id, Other->Id);
			return GetInteraction()->GetSelectedUnit() == *Other;
		} });

	Steps.Add({ TEXT("Space ends the turn (IA_EndTurn)"),
		[this]() { Commands = GetCombat()->GetRecord().Commands.Num(); PressKey(EKeys::SpaceBar); }, 0.6f, EndTurnRecorded });
	// The enemy side plays (passive today, AI from DW-106): wait up to 15 s for the player's next turn.
	Steps.Add({ TEXT("player turn again"), nullptr, 0.5f, PlayerTurn, 30 });

	Steps.Add({ TEXT("cursor over the HUD End Turn button"),
		[this]()
		{
			Commands = GetCombat()->GetRecord().Commands.Num();
			if (const TOptional<FVector2D> Point = HudWidgetOnScreen(TEXT("EndTurnButton")))
			{
				MoveMouse(Point.GetValue());
			}
		}, 0.3f, nullptr });
	Steps.Add({ TEXT("clicking the HUD End Turn button ends the turn (CommonUI HUD click)"),
		[this]() { ClickMouse(EKeys::LeftMouseButton); }, 0.6f, EndTurnRecorded });

	InWorld.GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::Next), 5.f, false);
}

void UDwDevInputProbeSubsystem::Next()
{
	// Check the step that just ran (polling again while it has retries left), then run the next one.
	if (Steps.IsValidIndex(Current) && Steps[Current].Check)
	{
		const FStep& Step = Steps[Current];
		FString Detail;
		const bool bOk = Step.Check(Detail);
		if (!bOk && RetriesLeft > 0)
		{
			--RetriesLeft;
			GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::Next), FMath::Max(Step.Wait, 0.05f), false);
			return;
		}
		Passed += bOk ? 1 : 0;
		UE_LOG(LogDw, Display, TEXT("DWINPUT %s %s%s"), bOk ? TEXT("PASS") : TEXT("FAIL"), *Step.Name, bOk ? TEXT("") : *(TEXT(" - ") + Detail));
	}
	++Current;
	if (!Steps.IsValidIndex(Current))
	{
		Finish();
		return;
	}
	RetriesLeft = Steps[Current].Retries;
	if (Steps[Current].Act)
	{
		Steps[Current].Act();
	}
	GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::Next), FMath::Max(Steps[Current].Wait, 0.05f), false);
}

void UDwDevInputProbeSubsystem::Finish()
{
	int32 Total = 0;
	for (const FStep& Step : Steps)
	{
		Total += Step.Check ? 1 : 0;
	}
	UE_LOG(LogDw, Display, TEXT("DWINPUT RESULT %d/%d"), Passed, Total);
	FPlatformMisc::RequestExit(false);
}
