// Copyright Brandon Hillig 2026

#include "Presentation/DwCameraRig.h"

#include "Board/DwBoard.h"
#include "Camera/CameraComponent.h"

ADwCameraRig::ADwCameraRig()
{
	PrimaryActorTick.bCanEverTick = true; // presentation easing only
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetRootComponent());
	Camera->SetFieldOfView(FieldOfView);
	// Exposure belongs to the sea mood (ADwSeaEnvironment), not the camera.
}

void ADwCameraRig::SnapTo(const FVector& Focus, float InZoom)
{
	TargetFocus = CurrentFocus = Focus;
	TargetZoom = CurrentZoom = FMath::Max(InZoom, 0.05f);
	ApplyView(CurrentFocus, CurrentZoom);
}

void ADwCameraRig::FrameBoard(const ADwBoard* Board)
{
	if (!Board)
	{
		return;
	}
	// The whole board on the deck plane, including the gap between the two ships.
	const FBox2D Bounds = Board->GetLocalBounds();
	const FVector2D Size = Bounds.GetSize();
	BoardCentre = Board->GetActorTransform().TransformPosition(FVector(Bounds.GetCenter() + FramingOffset, 0.f));
	PanLimit = Size * 0.5f;

	Camera->SetFieldOfView(FieldOfView); // Blueprint subclasses may change the default
	// Fit the width (the camera looks along -Y, so board X is screen width).
	const float HalfFov = FMath::DegreesToRadians(Camera->FieldOfView * 0.5f);
	const float WidthDistance = Size.X * 0.5f * FramingMargin / FMath::Tan(HalfFov);
	const float DepthDistance = Size.Y * FramingMargin / FMath::Tan(HalfFov); // vertical FOV is narrower: be generous
	BaseDistance = FMath::Max(WidthDistance, DepthDistance);

	TargetFocus = CurrentFocus = BoardCentre;
	TargetZoom = CurrentZoom = 1.f;
	ApplyView(CurrentFocus, CurrentZoom);
}

void ADwCameraRig::Pan(const FVector2D& Input, float DeltaTime)
{
	const float Speed = PanSpeed * CurrentZoom * DeltaTime;
	TargetFocus.X = FMath::Clamp(TargetFocus.X + Input.X * Speed, BoardCentre.X - PanLimit.X, BoardCentre.X + PanLimit.X);
	// Screen up = away from the camera = -Y.
	TargetFocus.Y = FMath::Clamp(TargetFocus.Y - Input.Y * Speed, BoardCentre.Y - PanLimit.Y, BoardCentre.Y + PanLimit.Y);
}

void ADwCameraRig::Zoom(float Steps)
{
	TargetZoom = FMath::Clamp(TargetZoom - Steps * ZoomStep, MinZoom, MaxZoom);
}

void ADwCameraRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Alpha = FMath::Clamp(DeltaSeconds * Smoothing, 0.f, 1.f);
	CurrentFocus = FMath::Lerp(CurrentFocus, TargetFocus, Alpha);
	CurrentZoom = FMath::Lerp(CurrentZoom, TargetZoom, Alpha);
	ApplyView(CurrentFocus, CurrentZoom);
}

void ADwCameraRig::ApplyView(const FVector& Focus, float InZoom)
{
	// Fixed angle from the +Y side looking along -Y: screen right = +X, so the player ship (low X) is on the left, the
	// enemy on the right, and deck row 0 (the top row of the ASCII layouts) is the far side. No rotation input.
	const FRotator View(-Pitch, -90.f, 0.f);
	SetActorLocationAndRotation(Focus - View.Vector() * BaseDistance * InZoom, View);
}
