// Copyright Brandon Hillig 2026

#include "Presentation/DwPresentationSubsystem.h"

#include "Engine/World.h"
#include "Misc/App.h"
#include "Shell/DwGameUserSettings.h"

void UDwPresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// -nullrhi (automation, simulator, servers): nothing to watch, so resolve everything immediately.
	bInstant = !FApp::CanEverRender();
	// Players' game speed setting (U1). Only rendered game worlds: instant worlds (tests, simulator) and editor worlds keep 1x.
	const UWorld* World = GetWorld();
	const UDwGameUserSettings* Settings = UDwGameUserSettings::Get();
	if (!bInstant && Settings && World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE))
	{
		PlaybackRate = Settings->GetPlaybackRate();
	}
}

bool UDwPresentationSubsystem::IsInstant(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UDwPresentationSubsystem* Presentation = World ? World->GetSubsystem<UDwPresentationSubsystem>() : nullptr;
	return !Presentation || Presentation->IsInstant();
}

float UDwPresentationSubsystem::GetPlaybackRate(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UDwPresentationSubsystem* Presentation = World ? World->GetSubsystem<UDwPresentationSubsystem>() : nullptr;
	return Presentation ? Presentation->GetPlaybackRate() : 1.f;
}
