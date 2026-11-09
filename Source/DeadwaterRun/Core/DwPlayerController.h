// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Combat/DwCombatSideController.h"
#include "DwPlayerController.generated.h"

class ADwCameraRig;
class UDwCombatInteraction;
struct FInputActionValue;

/**
 * The single local player's controller (mouse + keyboard), and the player side's IDwCombatSideController
 * (Docs/Architecture/03 "Player input"). Enhanced Input from DA_GameData.Input (IMC_Combat); the cursor is traced to the
 * board every frame and forwarded to UDwCombatInteraction, which turns clicks into combat commands. Owns the camera rig.
 */
UCLASS()
class DEADWATERRUN_API ADwPlayerController : public APlayerController, public IDwCombatSideController
{
	GENERATED_BODY()

public:
	ADwPlayerController();

	UFUNCTION(BlueprintPure, Category = "Deadwater|Input")
	UDwCombatInteraction* GetInteraction() const { return Interaction; }

	UFUNCTION(BlueprintPure, Category = "Deadwater|Camera")
	ADwCameraRig* GetCameraRig() const { return CameraRig; }

	// IDwCombatSideController
	virtual void BeginSideTurn(UDwCombatSubsystem& Combat, EDwSide Side) override;
	virtual void OnCommandResolved(UDwCombatSubsystem& Combat) override;

	virtual void PlayerTick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void SetupInputComponent() override;

private:
	UFUNCTION() void HandleCombatStarted();

	void OnSelect(const FInputActionValue& Value);
	void OnCancel(const FInputActionValue& Value);
	void OnEndTurn(const FInputActionValue& Value);
	void OnNextUnit(const FInputActionValue& Value);
	void OnPause(const FInputActionValue& Value);
	void OnCameraPan(const FInputActionValue& Value);
	void OnCameraZoom(const FInputActionValue& Value);
	void OnAbilitySlot(const FInputActionValue& Value, int32 Slot);

	void UpdateHover();

	UPROPERTY(Transient) TObjectPtr<UDwCombatInteraction> Interaction;
	UPROPERTY(Transient) TObjectPtr<ADwCameraRig> CameraRig;

	/** Camera rig class (BP subclass for tweaks); defaults to ADwCameraRig. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Camera")
	TSubclassOf<ADwCameraRig> CameraRigClass;

	FVector2D PendingPan = FVector2D::ZeroVector;
	/** Cursor and camera at the last hover trace: the hover only changes when one of them moves. */
	FVector2D LastCursor = FVector2D(-1.f, -1.f);
	FVector LastCameraLocation = FVector::ZeroVector;
};
