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
	Camera->SetFieldOfView(60.f);
	// Placeholder art is mostly dark wood and sea; stop auto exposure from washing it out.
	Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
	Camera->PostProcessSettings.AutoExposureBias = -1.f;
}

void ADwCameraRig::FrameBoard(const ADwBoard* Board)
{
	if (!Board)
	{
		return;
	}
	const FDwGridModel& Grid = Board->GetGrid();
	// Board corners in world space: tile centres of the two extreme tiles, padded by half a tile.
	const FVector Min = Board->TileToWorld(FIntPoint(0, 0));
	const FVector Max = Board->TileToWorld(FIntPoint(FMath::Max(Grid.Size.X - 1, 0), FMath::Max(Grid.Size.Y - 1, 0)));
	const float TileSize = Grid.Size.X > 1 ? (Max.X - Min.X) / (Grid.Size.X - 1) : 100.f;
	const FVector2D Size(FMath::Abs(Max.X - Min.X) + TileSize, FMath::Abs(Max.Y - Min.Y) + TileSize);
	BoardCentre = (Min + Max) * 0.5f;
	PanLimit = Size * 0.5f;

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
