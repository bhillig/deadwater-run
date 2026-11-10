// Copyright Brandon Hillig 2026

#include "Presentation/DwSeaEnvironment.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

ADwSeaEnvironment::ADwSeaEnvironment()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	auto MakeMesh = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(GetRootComponent());
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		return Mesh;
	};
	Water = MakeMesh(TEXT("Water"));
	SkyDome = MakeMesh(TEXT("SkyDome"));

	// Everything is movable: moods change at runtime and nothing needs a lighting build.
	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(GetRootComponent());
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetAtmosphereSunLight(true);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(GetRootComponent());
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(GetRootComponent());

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(GetRootComponent());

	Weather = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Weather"));
	Weather->SetupAttachment(GetRootComponent());
	Weather->SetAutoActivate(false);
	Weather->SetRelativeLocation(FVector(600.f, 200.f, 1200.f)); // above the middle of a two-ship board

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(GetRootComponent());
	PostProcess->bUnbound = true;
}

void ADwSeaEnvironment::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMood(CurrentMood ? CurrentMood.Get() : DefaultMood.Get());
}

void ADwSeaEnvironment::ApplyMood(const UDwSeaMood* Mood)
{
	Water->SetRelativeLocation(FVector(0.f, 0.f, WaterHeight));
	if (!Mood)
	{
		return;
	}
	CurrentMood = Mood;

	SkyDome->SetVisibility(Mood->SkyMaterial != nullptr);
	if (Mood->SkyMaterial)
	{
		SkyDome->SetMaterial(0, Mood->SkyMaterial);
	}
	if (Mood->WaterMaterial)
	{
		Water->SetMaterial(0, Mood->WaterMaterial);
	}

	Sun->SetRelativeRotation(Mood->SunRotation);
	Sun->SetIntensity(Mood->SunIntensity);
	Sun->SetLightColor(Mood->SunColor);
	SkyLight->SetIntensity(Mood->SkyLightIntensity);
	SkyLight->SetLightColor(Mood->SkyLightColor);

	Fog->SetFogDensity(Mood->FogDensity);
	Fog->SetFogInscatteringColor(Mood->FogColor);

	PostProcess->Settings.bOverride_AutoExposureBias = true;
	PostProcess->Settings.AutoExposureBias = Mood->ExposureBias;

	if (Weather->GetAsset() != Mood->Weather)
	{
		Weather->SetAsset(Mood->Weather);
	}
	if (Mood->Weather)
	{
		Weather->Activate(/*bReset*/ true);
	}
	else
	{
		Weather->DeactivateImmediate();
	}
}

const UDwSeaMood* ADwSeaEnvironment::PickMood(TConstArrayView<FDwSeaMoodChance> Moods, int32 Seed, const FString& Forced)
{
	if (!Forced.IsEmpty())
	{
		for (const FDwSeaMoodChance& Entry : Moods)
		{
			// "Storm" matches DA_SeaMood_Storm and a mood displayed as "Storm".
			if (Entry.Mood && (Entry.Mood->GetName().Equals(Forced, ESearchCase::IgnoreCase)
				|| Entry.Mood->GetName().EndsWith(TEXT("_") + Forced, ESearchCase::IgnoreCase)
				|| Entry.Mood->DisplayName.ToString().Equals(Forced, ESearchCase::IgnoreCase)))
			{
				return Entry.Mood;
			}
		}
	}

	float Total = 0.f;
	for (const FDwSeaMoodChance& Entry : Moods)
	{
		Total += Entry.Mood ? FMath::Max(Entry.Weight, 0.f) : 0.f;
	}
	if (Total <= 0.f)
	{
		return nullptr;
	}
	// Its own stream, salted: independent of the combat stream that Seed also starts.
	float Roll = FRandomStream(HashCombine(GetTypeHash(Seed), 0x5EA5EA5Eu)).FRand() * Total;
	const UDwSeaMood* Last = nullptr;
	for (const FDwSeaMoodChance& Entry : Moods)
	{
		const float Weight = Entry.Mood ? FMath::Max(Entry.Weight, 0.f) : 0.f;
		if (Weight <= 0.f)
		{
			continue;
		}
		Last = Entry.Mood;
		if (Roll < Weight)
		{
			return Entry.Mood;
		}
		Roll -= Weight;
	}
	return Last;
}
