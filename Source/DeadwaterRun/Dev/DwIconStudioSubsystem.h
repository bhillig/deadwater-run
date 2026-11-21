// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DwIconStudioSubsystem.generated.h"

class ADirectionalLight;
class ASceneCapture2D;
class AStaticMeshActor;
class UTextureRenderTarget2D;

/** One icon to render, from Tools/Content/ui_icon_renders.json. */
struct FDwIconRenderJob
{
	FString Name;
	/** Exactly one of MeshPath (a static mesh) or ActorPath (a Blueprint actor, e.g. a prop assembled from parts). */
	FString MeshPath;
	FString ActorPath;
	/** "side": look along the mesh's thinnest axis, longest axis turned by Roll (the Synty weapon-icon pose). "three": three-quarter view from Direction. */
	FString View = TEXT("side");
	float Roll = 45.f;
	/** Mirror the side view (swaps which end of the mesh points up). */
	bool bFlip = false;
	FVector Direction = FVector(-0.62, 0.55, 0.56);
	/** Output size in px (0 = the manifest's size). */
	int32 Size = 0;
	/** Fraction of the frame the mesh fills (1 = edge to edge). */
	float Fill = 0.9f;
	/** Portraits: frame this bone of the subject's skeletal mesh (e.g. "head"), FocusRadius cm around it, lifted FocusLift cm. */
	FName FocusBone;
	float FocusRadius = 30.f;
	float FocusLift = 0.f;
};

/**
 * Dev-only icon renderer (never created in Shipping). With -DwIconStudio=<manifest.json> -DwIconStudioOut=<folder> the game
 * renders every listed static mesh to <folder>/T_ICON_<Name>.png - lit, orthographic, transparent background, 2x
 * supersampled - and quits. Colour comes from a FinalColorLDR capture (the game's tonemapper), alpha from a
 * SceneColorHDR capture (inverse opacity in A). Runs in the game loop because commandlet scene captures read back
 * nothing. Driven by Tools/RenderIcons.ps1; the PNGs are imported as UI textures by the UI content script.
 */
UCLASS()
class DEADWATERRUN_API UDwIconStudioSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Parses the manifest; false (with a reason in OutError) on bad JSON or a job without a name or mesh. */
	static bool ParseManifest(const FString& Json, int32& OutSize, float& OutKeyLight, float& OutFillLight, TArray<FDwIconRenderJob>& OutJobs, FString& OutError);

private:
	void SetUpStage();
	void Step();
	bool RenderJob(const FDwIconRenderJob& Job);
	void Finish();

	FString OutDir;
	int32 Size = 512;
	float KeyLight = 6.f;
	float FillLight = 2.f;
	TArray<FDwIconRenderJob> Jobs;
	int32 JobIndex = 0;
	int32 WaitFrames = 0;
	int32 Rendered = 0;
	TArray<FString> Failures;
	FTimerHandle Timer;

	UPROPERTY() TObjectPtr<AStaticMeshActor> MeshActor;
	/** What the current job shows: MeshActor, or a Blueprint actor spawned for the job (destroyed after it). */
	UPROPERTY() TObjectPtr<AActor> Subject;
	UPROPERTY() TObjectPtr<ASceneCapture2D> CaptureActor;
	UPROPERTY() TObjectPtr<ADirectionalLight> Key;
	UPROPERTY() TObjectPtr<ADirectionalLight> Fill;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> ColorTarget;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> AlphaTarget;
};
