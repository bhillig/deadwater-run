// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DwCameraRig.generated.h"

class ADwBoard;
class UCameraComponent;

/**
 * The combat camera (Docs/Architecture/03 "ADwCameraRig", U2 decided): a fixed angle looking across both ships from the
 * player's side, framing the whole board, with limited pan and zoom and no rotation. Presentation only; it ticks to ease
 * toward the requested framing.
 */
UCLASS(Blueprintable)
class DEADWATERRUN_API ADwCameraRig : public AActor
{
	GENERATED_BODY()

public:
	ADwCameraRig();

	/** Centres on the board and picks a distance that shows every tile. Snaps immediately. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Camera")
	void FrameBoard(const ADwBoard* Board);

	/** Pans by Input (-1..1 per axis, board X/Y) scaled by PanSpeed and DeltaTime; clamped to the board. */
	void Pan(const FVector2D& Input, float DeltaTime);

	/** Positive zooms in. Clamped to [MinZoom, MaxZoom] of the framed distance. */
	void Zoom(float Steps);

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Deadwater|Camera") FVector GetFocus() const { return TargetFocus; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|Camera") float GetZoom() const { return TargetZoom; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** Degrees below the horizon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "20", ClampMax = "85"))
	float Pitch = 55.f;

	/** Extra space around the board when framing (1 = edge to edge). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "1"))
	float FramingMargin = 1.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera") float PanSpeed = 900.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera") float ZoomStep = 0.1f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera") float MinZoom = 0.55f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera") float MaxZoom = 1.3f;
	/** How fast the view eases to the target (higher = snappier). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera") float Smoothing = 10.f;

private:
	void ApplyView(const FVector& Focus, float Zoom);

	FVector BoardCentre = FVector::ZeroVector;
	FVector2D PanLimit = FVector2D::ZeroVector;
	float BaseDistance = 1500.f;
	FVector TargetFocus = FVector::ZeroVector;
	FVector CurrentFocus = FVector::ZeroVector;
	float TargetZoom = 1.f;
	float CurrentZoom = 1.f;
};
