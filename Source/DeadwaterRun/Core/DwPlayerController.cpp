// Copyright Brandon Hillig 2026

#include "Core/DwPlayerController.h"

#include "Board/DwBoard.h"
#include "Combat/DwCombatInteraction.h"
#include "Combat/DwCombatSubsystem.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Core/DwLog.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Presentation/DwCameraRig.h"

ADwPlayerController::ADwPlayerController()
{
	// Turn-based, mouse-driven game: cursor always visible, click events on for tile picking.
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	CameraRigClass = ADwCameraRig::StaticClass();
}

void ADwPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);

	const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
	if (UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (GameData && GameData->Input.CombatContext)
		{
			Input->AddMappingContext(GameData->Input.CombatContext, 0);
		}
		else
		{
			UE_LOG(LogDwUI, Error, TEXT("DA_GameData.Input.CombatContext is not set: no combat input."));
		}
	}

	Interaction = NewObject<UDwCombatInteraction>(this);
	if (UDwCombatSubsystem* Combat = GetWorld()->GetSubsystem<UDwCombatSubsystem>())
	{
		Combat->SetSideController(EDwSide::Player, this);
		Combat->OnCombatStarted.AddDynamic(this, &ThisClass::HandleCombatStarted);
		if (Combat->IsActive())
		{
			HandleCombatStarted();
		}
	}
}

void ADwPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDwCombatSubsystem* Combat = GetWorld() ? GetWorld()->GetSubsystem<UDwCombatSubsystem>() : nullptr)
	{
		Combat->OnCombatStarted.RemoveAll(this);
		if (Combat->GetSideController(EDwSide::Player) == this)
		{
			Combat->SetSideController(EDwSide::Player, nullptr);
		}
	}
	if (Interaction)
	{
		Interaction->Shutdown();
	}
	Super::EndPlay(Reason);
}

void ADwPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
	if (!Input || !GameData)
	{
		UE_LOG(LogDwUI, Error, TEXT("SetupInputComponent: needs UEnhancedInputComponent (DefaultInput.ini) and DA_GameData."));
		return;
	}
	const FDwInputConfig& Config = GameData->Input;
	auto Bind = [Input](const UInputAction* Action, ETriggerEvent Event, ADwPlayerController* Self, void (ADwPlayerController::*Handler)(const FInputActionValue&))
	{
		if (Action)
		{
			Input->BindAction(Action, Event, Self, Handler);
		}
	};
	Bind(Config.Select, ETriggerEvent::Started, this, &ThisClass::OnSelect);
	Bind(Config.Cancel, ETriggerEvent::Started, this, &ThisClass::OnCancel);
	Bind(Config.EndTurn, ETriggerEvent::Started, this, &ThisClass::OnEndTurn);
	Bind(Config.NextUnit, ETriggerEvent::Started, this, &ThisClass::OnNextUnit);
	Bind(Config.Pause, ETriggerEvent::Started, this, &ThisClass::OnPause);
	Bind(Config.CameraPan, ETriggerEvent::Triggered, this, &ThisClass::OnCameraPan);
	Bind(Config.CameraZoom, ETriggerEvent::Triggered, this, &ThisClass::OnCameraZoom);
	for (int32 Slot = 0; Slot < Config.Abilities.Num(); ++Slot)
	{
		if (Config.Abilities[Slot])
		{
			Input->BindAction(Config.Abilities[Slot], ETriggerEvent::Started, this, &ThisClass::OnAbilitySlot, Slot);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Combat

void ADwPlayerController::HandleCombatStarted()
{
	UDwCombatSubsystem* Combat = GetWorld()->GetSubsystem<UDwCombatSubsystem>();
	if (!Combat || !Interaction)
	{
		return;
	}
	Interaction->Initialize(*Combat);
	if (!CameraRig)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		CameraRig = GetWorld()->SpawnActor<ADwCameraRig>(CameraRigClass ? CameraRigClass.Get() : ADwCameraRig::StaticClass(), Params);
	}
	CameraRig->FrameBoard(Combat->GetBoard());
	SetViewTarget(CameraRig);
	if (Combat->GetActiveSide() == EDwSide::Player)
	{
		Interaction->BeginPlayerTurn(); // the fight may have started before this controller listened
	}
}

void ADwPlayerController::BeginSideTurn(UDwCombatSubsystem& /*Combat*/, EDwSide Side)
{
	if (Side == EDwSide::Player && Interaction)
	{
		Interaction->BeginPlayerTurn();
	}
}

void ADwPlayerController::OnCommandResolved(UDwCombatSubsystem& /*Combat*/)
{
	if (Interaction)
	{
		Interaction->Refresh();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Input

void ADwPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateHover();
	if (CameraRig && !PendingPan.IsNearlyZero())
	{
		CameraRig->Pan(PendingPan, DeltaTime);
	}
	PendingPan = FVector2D::ZeroVector;
}

void ADwPlayerController::UpdateHover()
{
	if (!Interaction)
	{
		return;
	}
	float MouseX = 0.f;
	float MouseY = 0.f;
	const FVector CameraLocation = CameraRig ? CameraRig->GetActorLocation() : FVector::ZeroVector;
	const bool bHasMouse = GetMousePosition(MouseX, MouseY);
	const FVector2D Cursor(MouseX, MouseY);
	if (!bHasMouse || (Cursor.Equals(LastCursor) && CameraLocation.Equals(LastCameraLocation, 0.1f)))
	{
		return; // nothing moved: keep the current hover (also lets scripted demos and tests set it)
	}
	LastCursor = Cursor;
	LastCameraLocation = CameraLocation;
	const UDwCombatSubsystem* Combat = GetWorld()->GetSubsystem<UDwCombatSubsystem>();
	const ADwBoard* Board = Combat ? Combat->GetBoard() : nullptr;
	FVector Origin;
	FVector Direction;
	FIntPoint Tile;
	if (Board && DeprojectMousePositionToWorld(Origin, Direction) && Board->RayToTile(Origin, Direction, Tile))
	{
		Interaction->HoverTile(Tile);
	}
	else
	{
		Interaction->HoverTile(TOptional<FIntPoint>());
	}
}

void ADwPlayerController::OnSelect(const FInputActionValue& /*Value*/)
{
	if (Interaction && Interaction->GetHoveredTile().IsSet())
	{
		Interaction->ClickTile(Interaction->GetHoveredTile().GetValue());
	}
}

void ADwPlayerController::OnCancel(const FInputActionValue& /*Value*/)
{
	if (Interaction)
	{
		Interaction->Cancel();
	}
}

void ADwPlayerController::OnEndTurn(const FInputActionValue& /*Value*/)
{
	if (Interaction)
	{
		Interaction->RequestEndTurn();
	}
}

void ADwPlayerController::OnNextUnit(const FInputActionValue& /*Value*/)
{
	if (Interaction)
	{
		Interaction->SelectNextUnit();
	}
}

void ADwPlayerController::OnPause(const FInputActionValue& /*Value*/)
{
	// Esc cancels targeting first; with nothing to cancel it will open the pause menu (DW-210).
	if (!Interaction || !Interaction->Cancel())
	{
		UE_LOG(LogDwUI, Log, TEXT("Pause requested (pause menu arrives with DW-210)."));
	}
}

void ADwPlayerController::OnCameraPan(const FInputActionValue& Value)
{
	PendingPan += Value.Get<FVector2D>();
}

void ADwPlayerController::OnCameraZoom(const FInputActionValue& Value)
{
	if (CameraRig)
	{
		CameraRig->Zoom(Value.Get<float>());
	}
}

void ADwPlayerController::OnAbilitySlot(const FInputActionValue& /*Value*/, int32 Slot)
{
	if (Interaction)
	{
		Interaction->SelectSlot(Slot);
	}
}
