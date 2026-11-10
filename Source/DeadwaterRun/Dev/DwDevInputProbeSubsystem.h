// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "DwDevInputProbeSubsystem.generated.h"

class ADwPlayerController;
class UDwCombatInteraction;
class UDwCombatSubsystem;

/**
 * Dev-only input regression probe (DW-107, Tools/InputProbe.ps1). With -DwInputProbe on the command line it plays L_Sea,
 * injects real Slate key and mouse events (so they go through the game viewport, the CommonUI action router and Enhanced
 * Input exactly as hardware input does) and checks the combat reacted: Tab, 1-6, Esc, right-click, left-click select and
 * move on the board, Space, and a click on the HUD's End Turn button. Logs "DWINPUT PASS/FAIL <step>" lines and
 * "DWINPUT RESULT <passed>/<total>", then quits. Moves the OS cursor while it runs. Never created in Shipping.
 */
UCLASS()
class DEADWATERRUN_API UDwDevInputProbeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FStep
	{
		FString Name;
		/** Injects the input (may be empty: a pure wait). */
		TFunction<void()> Act;
		/** Seconds to let the game process it. */
		float Wait = 0.4f;
		/** True when the game reacted as expected; Detail explains a failure. Null: no check (a setup step). */
		TFunction<bool(FString& Detail)> Check;
		/** Times the check is polled again (every Wait seconds) before it counts as failed. */
		int32 Retries = 0;
	};

	void Next();
	void Finish();

	void PressKey(const FKey& Key);
	/** Moves the cursor to a viewport pixel (both the OS cursor and a Slate mouse-move event). */
	void MoveMouse(const FVector2D& ViewportPixel);
	void ClickMouse(const FKey& Button);
	/** Viewport pixel of a board tile's centre, or unset if it is off screen. */
	TOptional<FVector2D> TileOnScreen(FIntPoint Tile) const;
	/** Viewport pixel of a named widget's centre inside the combat HUD, or unset. */
	TOptional<FVector2D> HudWidgetOnScreen(FName WidgetName) const;

	ADwPlayerController* GetPlayer() const;
	UDwCombatInteraction* GetInteraction() const;
	UDwCombatSubsystem* GetCombat() const;

	TArray<FStep> Steps;
	int32 Current = INDEX_NONE;
	int32 RetriesLeft = 0;
	int32 Passed = 0;
	FVector2D LastScreen = FVector2D::ZeroVector;
	FTimerHandle Timer;

	// State captured between steps.
	int32 StartRound = 0;
	int32 Commands = 0;
	FIntPoint MoveTarget = FIntPoint(-1, -1);
	int32 ArmSlot = INDEX_NONE;
};
