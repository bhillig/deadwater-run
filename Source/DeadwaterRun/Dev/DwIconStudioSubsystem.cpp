// Copyright Brandon Hillig 2026

#include "Dev/DwIconStudioSubsystem.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/DwLog.h"
#include "Dom/JsonObject.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace DwIconStudio
{
	/** Far from anything a level places, so the show-only capture never fights the level's own geometry for depth. */
	const FVector StageOrigin(0.0, 0.0, 200000.0);
	/** Frames to wait after loading a mesh, for texture streaming and shader maps. */
	constexpr int32 SettleFrames = 6;
}

bool UDwIconStudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return Super::ShouldCreateSubsystem(Outer) && FCString::Strifind(FCommandLine::Get(), TEXT("-DwIconStudio=")) != nullptr;
#endif
}

bool UDwIconStudioSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UDwIconStudioSubsystem::ParseManifest(const FString& Json, int32& OutSize, float& OutKeyLight, float& OutFillLight, TArray<FDwIconRenderJob>& OutJobs, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		OutError = TEXT("manifest is not valid JSON");
		return false;
	}
	double Number = 0.0;
	if (Root->TryGetNumberField(TEXT("size"), Number)) { OutSize = FMath::Clamp(static_cast<int32>(Number), 32, 1024); }
	if (Root->TryGetNumberField(TEXT("keyLight"), Number)) { OutKeyLight = static_cast<float>(Number); }
	if (Root->TryGetNumberField(TEXT("fillLight"), Number)) { OutFillLight = static_cast<float>(Number); }
	const TArray<TSharedPtr<FJsonValue>>* Icons = nullptr;
	if (!Root->TryGetArrayField(TEXT("icons"), Icons) || Icons->IsEmpty())
	{
		OutError = TEXT("manifest has no \"icons\" array");
		return false;
	}
	OutJobs.Reset();
	for (int32 Index = 0; Index < Icons->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> Item = (*Icons)[Index]->AsObject();
		FDwIconRenderJob Job;
		if (Item.IsValid())
		{
			Item->TryGetStringField(TEXT("name"), Job.Name);
			Item->TryGetStringField(TEXT("mesh"), Job.MeshPath);
			Item->TryGetStringField(TEXT("actor"), Job.ActorPath);
		}
		if (Job.Name.IsEmpty() || Job.MeshPath.IsEmpty() == Job.ActorPath.IsEmpty())
		{
			OutError = FString::Printf(TEXT("icon %d needs a \"name\" and exactly one of \"mesh\" or \"actor\""), Index);
			return false;
		}
		Item->TryGetStringField(TEXT("view"), Job.View);
		if (Item->TryGetNumberField(TEXT("roll"), Number)) { Job.Roll = static_cast<float>(Number); }
		if (Item->TryGetNumberField(TEXT("fill"), Number)) { Job.Fill = FMath::Clamp(static_cast<float>(Number), 0.2f, 1.f); }
		Item->TryGetBoolField(TEXT("flip"), Job.bFlip);
		FString Bone;
		if (Item->TryGetStringField(TEXT("focusBone"), Bone) && !Bone.IsEmpty())
		{
			Job.FocusBone = FName(*Bone);
		}
		if (Item->TryGetNumberField(TEXT("focusRadius"), Number)) { Job.FocusRadius = static_cast<float>(Number); }
		if (Item->TryGetNumberField(TEXT("focusLift"), Number)) { Job.FocusLift = static_cast<float>(Number); }
		const TArray<TSharedPtr<FJsonValue>>* Dir = nullptr;
		if (Item->TryGetArrayField(TEXT("dir"), Dir) && Dir->Num() == 3)
		{
			Job.Direction = FVector((*Dir)[0]->AsNumber(), (*Dir)[1]->AsNumber(), (*Dir)[2]->AsNumber());
		}
		if (Job.View != TEXT("side") && Job.View != TEXT("three"))
		{
			OutError = FString::Printf(TEXT("icon %s: view must be \"side\" or \"three\""), *Job.Name);
			return false;
		}
		OutJobs.Add(MoveTemp(Job));
	}
	return true;
}

void UDwIconStudioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FString Manifest;
	if (!FParse::Value(FCommandLine::Get(), TEXT("DwIconStudio="), Manifest) || !FParse::Value(FCommandLine::Get(), TEXT("DwIconStudioOut="), OutDir))
	{
		UE_LOG(LogDw, Error, TEXT("ICONSTUDIO FAIL needs -DwIconStudio=<manifest.json> and -DwIconStudioOut=<folder>"));
		FPlatformMisc::RequestExit(false);
		return;
	}
	FString Json;
	FString Error;
	if (!FFileHelper::LoadFileToString(Json, *Manifest) || !ParseManifest(Json, Size, KeyLight, FillLight, Jobs, Error))
	{
		UE_LOG(LogDw, Error, TEXT("ICONSTUDIO FAIL %s: %s"), *Manifest, Error.IsEmpty() ? TEXT("cannot read the file") : *Error);
		FPlatformMisc::RequestExit(false);
		return;
	}
	IFileManager::Get().MakeDirectory(*OutDir, /*Tree*/ true);
	SetUpStage();
	UE_LOG(LogDw, Log, TEXT("Icon studio: %d icons at %d px -> %s"), Jobs.Num(), Size, *OutDir);
	InWorld.GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::Step), 0.02f, /*bLoop*/ true, /*FirstDelay*/ 1.f);
}

void UDwIconStudioSubsystem::SetUpStage()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	MeshActor = World->SpawnActor<AStaticMeshActor>(DwIconStudio::StageOrigin, FRotator::ZeroRotator, Params);
	MeshActor->SetMobility(EComponentMobility::Movable);
	MeshActor->GetStaticMeshComponent()->SetCastShadow(false);

	auto MakeLight = [&](float Intensity)
	{
		ADirectionalLight* Light = World->SpawnActor<ADirectionalLight>(DwIconStudio::StageOrigin, FRotator::ZeroRotator, Params);
		UDirectionalLightComponent* Component = Cast<UDirectionalLightComponent>(Light->GetLightComponent());
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetIntensity(Intensity);
		Component->SetCastShadows(false);
		Component->SetAtmosphereSunLight(false);
		return Light;
	};
	Key = MakeLight(KeyLight);
	Fill = MakeLight(FillLight);

	auto MakeTarget = [&](ETextureRenderTargetFormat Format)
	{
		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(this);
		Target->RenderTargetFormat = Format;
		Target->ClearColor = FLinearColor::Transparent;
		Target->bAutoGenerateMips = false;
		Target->InitAutoFormat(Size * 2, Size * 2);
		Target->UpdateResourceImmediate(true);
		return Target;
	};
	ColorTarget = MakeTarget(RTF_RGBA8);
	AlphaTarget = MakeTarget(RTF_RGBA16f);

	CaptureActor = World->SpawnActor<ASceneCapture2D>(DwIconStudio::StageOrigin, FRotator::ZeroRotator, Params);
	USceneCaptureComponent2D* Capture = CaptureActor->GetCaptureComponent2D();
	Capture->ProjectionType = ECameraProjectionMode::Orthographic;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyActors = { MeshActor };
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetBloom(false);
	Capture->ShowFlags.SetMotionBlur(false);
	Capture->ShowFlags.SetTemporalAA(false);
	Capture->ShowFlags.SetAntiAliasing(false);
	Capture->ShowFlags.SetEyeAdaptation(false);
	FPostProcessSettings& Post = Capture->PostProcessSettings;
	Post.bOverride_AutoExposureMethod = true;
	Post.AutoExposureMethod = AEM_Manual;
	Post.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Post.AutoExposureApplyPhysicalCameraExposure = false;
	Post.bOverride_AutoExposureBias = true;
	Post.AutoExposureBias = 0.f;
	Post.bOverride_VignetteIntensity = true;
	Post.VignetteIntensity = 0.f;
	Post.bOverride_AmbientOcclusionIntensity = true;
	Post.AmbientOcclusionIntensity = 0.f;
	Capture->PostProcessBlendWeight = 1.f;
}

void UDwIconStudioSubsystem::Step()
{
	if (!Jobs.IsValidIndex(JobIndex))
	{
		Finish();
		return;
	}
	const FDwIconRenderJob& Job = Jobs[JobIndex];
	if (WaitFrames == 0)
	{
		if (!Job.ActorPath.IsEmpty())
		{
			// A Blueprint prop (e.g. BP_Wep_Cannon_01: carriage, barrel and wheels as separate meshes).
			UClass* ActorClass = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("%s.%s_C"), *Job.ActorPath, *FPackageName::GetShortName(Job.ActorPath)));
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Subject = ActorClass ? GetWorld()->SpawnActor<AActor>(ActorClass, DwIconStudio::StageOrigin, FRotator::ZeroRotator, Params) : nullptr;
			if (!Subject)
			{
				Failures.Add(FString::Printf(TEXT("%s (no actor %s)"), *Job.Name, *Job.ActorPath));
				++JobIndex;
				return;
			}
			MeshActor->SetActorHiddenInGame(true);
			// Characters would fall or run AI at the stage: freeze them in their reference pose.
			Subject->SetActorTickEnabled(false);
			Subject->ForEachComponent<UActorComponent>(/*bIncludeFromChildActors*/ true, [](UActorComponent* Component)
			{
				if (!Component->IsA<USkeletalMeshComponent>())
				{
					Component->SetComponentTickEnabled(false);
				}
			});
		}
		else
		{
			UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Job.MeshPath);
			if (!Mesh)
			{
				Failures.Add(FString::Printf(TEXT("%s (no mesh %s)"), *Job.Name, *Job.MeshPath));
				++JobIndex;
				return;
			}
			MeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
			MeshActor->SetActorHiddenInGame(false);
			Subject = MeshActor;
		}
		CaptureActor->GetCaptureComponent2D()->ShowOnlyActors = { Subject };
		Subject->ForEachComponent<UPrimitiveComponent>(/*bIncludeFromChildActors*/ true, [](UPrimitiveComponent* Primitive)
		{
			Primitive->SetCastShadow(false);
			TArray<UMaterialInterface*> Materials;
			Primitive->GetUsedMaterials(Materials);
			for (UMaterialInterface* Material : Materials)
			{
				TArray<UTexture*> Textures;
				if (Material)
				{
					Material->GetUsedTextures(Textures);
				}
				for (UTexture* Texture : Textures)
				{
					Texture->SetForceMipLevelsToBeResident(30.f);
				}
			}
		});
		WaitFrames = DwIconStudio::SettleFrames;
		return;
	}
	if (--WaitFrames > 0)
	{
		return;
	}
#if WITH_EDITOR
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
#endif
	if (RenderJob(Job))
	{
		++Rendered;
	}
	else
	{
		Failures.Add(Job.Name);
	}
	if (Subject && Subject != MeshActor)
	{
		Subject->Destroy();
	}
	Subject = nullptr;
	++JobIndex;
}

bool UDwIconStudioSubsystem::RenderJob(const FDwIconRenderJob& Job)
{
	// Centre the subject's bounds on the stage origin.
	Subject->SetActorLocationAndRotation(DwIconStudio::StageOrigin, FRotator::ZeroRotator);
	FVector Center;
	FVector Extent;
	Subject->GetActorBounds(/*bOnlyCollidingComponents*/ false, Center, Extent, /*bIncludeFromChildActors*/ true);
	Subject->SetActorLocation(DwIconStudio::StageOrigin * 2.0 - Center);
	const double Radius = Extent.Size();

	FVector Forward;
	FVector Up;
	if (Job.View == TEXT("side"))
	{
		const FVector Axes[3] = { FVector::XAxisVector, FVector::YAxisVector, FVector::ZAxisVector };
		const double E[3] = { Extent.X, Extent.Y, Extent.Z };
		const int32 Thin = E[0] <= E[1] && E[0] <= E[2] ? 0 : (E[1] <= E[2] ? 1 : 2);
		const int32 A = (Thin + 1) % 3;
		const int32 B = (Thin + 2) % 3;
		const int32 Long = E[A] >= E[B] ? A : B;
		Forward = Axes[Thin];
		const FVector E1 = Axes[Long];
		FVector E2 = FVector::CrossProduct(Forward, E1).GetSafeNormal();
		if (Job.bFlip)
		{
			E2 = -E2;
		}
		const double R = FMath::DegreesToRadians(Job.Roll);
		Up = FMath::Sin(R) * E1 + FMath::Cos(R) * E2;
	}
	else
	{
		Forward = -Job.Direction.GetSafeNormal();
		Up = FVector::ZAxisVector.RotateAngleAxis(Job.Roll, Forward);
	}
	const FRotator ViewRotation = FRotationMatrix::MakeFromXZ(Forward, Up).Rotator();
	const FVector Right = FRotationMatrix(ViewRotation).GetScaledAxis(EAxis::Y);
	const FVector ScreenUp = FRotationMatrix(ViewRotation).GetScaledAxis(EAxis::Z);
	double Half = 1.0;
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector C((Corner & 1) ? Extent.X : -Extent.X, (Corner & 2) ? Extent.Y : -Extent.Y, (Corner & 4) ? Extent.Z : -Extent.Z);
		Half = FMath::Max3(Half, FMath::Abs(FVector::DotProduct(C, Right)), FMath::Abs(FVector::DotProduct(C, ScreenUp)));
	}

	// Portraits: frame a bone (e.g. the head) instead of the whole subject.
	FVector Target = DwIconStudio::StageOrigin;
	if (!Job.FocusBone.IsNone())
	{
		const USkeletalMeshComponent* Skeletal = Subject->FindComponentByClass<USkeletalMeshComponent>();
		if (!Skeletal || Skeletal->GetBoneIndex(Job.FocusBone) == INDEX_NONE)
		{
			UE_LOG(LogDw, Warning, TEXT("Icon studio: %s has no bone %s"), *Job.Name, *Job.FocusBone.ToString());
			return false;
		}
		Target = Skeletal->GetBoneLocation(Job.FocusBone) + FVector(0.0, 0.0, Job.FocusLift);
		Half = Job.FocusRadius;
	}

	USceneCaptureComponent2D* Capture = CaptureActor->GetCaptureComponent2D();
	CaptureActor->SetActorLocationAndRotation(Target - Forward * (Radius * 4.0 + 100.0), ViewRotation);
	Capture->OrthoWidth = static_cast<float>(2.0 * Half / Job.Fill);
	// Key light from the upper left, in front; a weaker fill from the lower right.
	Key->SetActorRotation((Forward - ScreenUp * 0.9 + Right * 0.7).Rotation());
	Fill->SetActorRotation((Forward + ScreenUp * 0.3 - Right * 0.8).Rotation());

	Capture->TextureTarget = ColorTarget;
	Capture->CaptureSource = SCS_FinalColorLDR;
	Capture->CaptureScene();
	Capture->TextureTarget = AlphaTarget;
	Capture->CaptureSource = SCS_SceneColorHDR;
	Capture->CaptureScene();

	TArray<FColor> Color;
	TArray<FLinearColor> Alpha;
	if (!ColorTarget->GameThread_GetRenderTargetResource()->ReadPixels(Color) || !AlphaTarget->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Alpha))
	{
		return false;
	}
	const int32 Big = Size * 2;
	if (Color.Num() != Big * Big || Alpha.Num() != Big * Big)
	{
		return false;
	}
	// 2x2 box down-sample. The LDR colour is the mesh over black (premultiplied), so sum colour / sum coverage un-premultiplies it.
	TArray<FColor> Out;
	Out.SetNumZeroed(Size * Size);
	int32 Covered = 0;
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			double R = 0.0, G = 0.0, B = 0.0, A = 0.0;
			for (int32 S = 0; S < 4; ++S)
			{
				const int32 I = (Y * 2 + (S >> 1)) * Big + X * 2 + (S & 1);
				const double Coverage = FMath::Clamp(1.0 - Alpha[I].A, 0.0, 1.0);
				R += Color[I].R;
				G += Color[I].G;
				B += Color[I].B;
				A += Coverage;
			}
			if (A <= 0.0)
			{
				continue;
			}
			++Covered;
			FColor& Pixel = Out[Y * Size + X];
			Pixel.R = static_cast<uint8>(FMath::Clamp(R / A, 0.0, 255.0));
			Pixel.G = static_cast<uint8>(FMath::Clamp(G / A, 0.0, 255.0));
			Pixel.B = static_cast<uint8>(FMath::Clamp(B / A, 0.0, 255.0));
			Pixel.A = static_cast<uint8>(FMath::Clamp(A / 4.0 * 255.0, 0.0, 255.0));
		}
	}
	if (Covered == 0)
	{
		UE_LOG(LogDw, Warning, TEXT("Icon studio: %s rendered nothing"), *Job.Name);
		return false;
	}
	const FString File = FPaths::Combine(OutDir, FString::Printf(TEXT("T_ICON_%s.png"), *Job.Name));
	const FImageView View(Out.GetData(), Size, Size, EGammaSpace::sRGB);
	if (!FImageUtils::SaveImageByExtension(*File, View))
	{
		return false;
	}
	UE_LOG(LogDw, Log, TEXT("Icon studio: %s (%d%% covered)"), *File, Covered * 100 / (Size * Size));
	return true;
}

void UDwIconStudioSubsystem::Finish()
{
	GetWorld()->GetTimerManager().ClearTimer(Timer);
	if (Failures.IsEmpty())
	{
		UE_LOG(LogDw, Log, TEXT("ICONSTUDIO PASS %d icons -> %s"), Rendered, *OutDir);
	}
	else
	{
		UE_LOG(LogDw, Error, TEXT("ICONSTUDIO FAIL %d rendered, failed: %s"), Rendered, *FString::Join(Failures, TEXT(", ")));
	}
	FPlatformMisc::RequestExit(false);
}
