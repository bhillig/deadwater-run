// Copyright Brandon Hillig 2026

#include "Presentation/DwPresentationSubsystem.h"

#include "Components/AudioComponent.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Core/DwLog.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "NiagaraComponent.h"
#include "Shell/DwGameUserSettings.h"
#include "Sound/SoundBase.h"

#if !UE_BUILD_SHIPPING
namespace DwPresentationDev
{
	/**
	 * Dw.Audio.PlaceholderCue <CueTag> <SoundPath> [<SoundPath>...]: this world's cue map becomes a copy of DA_AudioCueMap
	 * with CueTag's Start set to those sounds (one layer, variations). For auditioning and demos before DW-311.
	 */
	FAutoConsoleCommandWithWorldAndArgs PlaceholderCue(TEXT("Dw.Audio.PlaceholderCue"),
		TEXT("Dw.Audio.PlaceholderCue <CueTag> <SoundPath> [<SoundPath>...]: plays these sounds for the cue in this world (dev)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDwPresentationSubsystem* Presentation = World ? World->GetSubsystem<UDwPresentationSubsystem>() : nullptr;
			const FGameplayTag Tag = Args.IsEmpty() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(*Args[0], /*ErrorIfNotFound*/ false);
			if (!Presentation || !Tag.IsValid() || Args.Num() < 2)
			{
				UE_LOG(LogDwPresentation, Error, TEXT("Dw.Audio.PlaceholderCue: needs a world, a known cue tag and at least one sound path."));
				return;
			}
			const UDwAudioCueMap* Source = Presentation->GetCueMap();
			UDwAudioCueMap* Map = Source ? DuplicateObject<UDwAudioCueMap>(Source, Presentation) : NewObject<UDwAudioCueMap>(Presentation);
			FDwCueSoundLayer Layer;
			for (int32 Index = 1; Index < Args.Num(); ++Index)
			{
				if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Args[Index]))
				{
					Layer.Sounds.Add(Sound);
				}
				else
				{
					UE_LOG(LogDwPresentation, Error, TEXT("Dw.Audio.PlaceholderCue: no sound at %s."), *Args[Index]);
				}
			}
			FDwCueSound& Cue = Map->Cues.FindOrAdd(Tag);
			Cue.Start = { Layer };
			Presentation->SetCueMapOverride(Map);
			UE_LOG(LogDwPresentation, Log, TEXT("Dw.Audio.PlaceholderCue: %s plays %d placeholder sound(s) in %s."), *Tag.ToString(),
				Layer.Sounds.Num(), *World->GetName());
		}));
}
#endif

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
	SoundRandom.Initialize(static_cast<int32>(FPlatformTime::Cycles()));
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

bool UDwPresentationSubsystem::ShouldPresent(const UWorld* World)
{
	const UDwPresentationSubsystem* Presentation = World ? World->GetSubsystem<UDwPresentationSubsystem>() : nullptr;
	return World && (!Presentation || !Presentation->IsInstant());
}

const UDwAudioCueMap* UDwPresentationSubsystem::GetCueMap() const
{
	if (CueMapOverride)
	{
		return CueMapOverride;
	}
	const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
	return GameData ? GameData->Audio.CueMap.Get() : nullptr;
}

int32 UDwPresentationSubsystem::PlayCueSounds(const FGameplayTag& CueTag, EDwCueSoundSection Section, const FVector& Location,
	USceneComponent* AttachTo, TArray<TObjectPtr<UAudioComponent>>* OutLoops)
{
	UWorld* World = GetWorld();
	if (bInstant || !World)
	{
		return 0;
	}
	const UDwAudioCueMap* Map = GetCueMap();
	FGameplayTag Matched;
	const FDwCueSound* Cue = Map ? Map->FindCue(CueTag, &Matched) : nullptr;
	if (!Cue)
	{
		UE_LOG(LogDwPresentation, Verbose, TEXT("Cue sound: %s has no DA_AudioCueMap entry."), *CueTag.ToString());
		return 0;
	}

	const bool bLoop = Section == EDwCueSoundSection::Loop;
	const TArray<FDwCueSoundLayer>& Layers = Cue->GetSection(Section);
	int32 Started = 0;
	for (int32 LayerIndex = 0; LayerIndex < Layers.Num(); ++LayerIndex)
	{
		const FDwCueSoundLayer& Layer = Layers[LayerIndex];
		int32& Last = LastVariations.FindOrAdd(MakeTuple(Matched, Section, LayerIndex), INDEX_NONE);
		const int32 Pick = UDwAudioCueMap::PickVariation(Layer.Sounds.Num(), Last, SoundRandom);
		USoundBase* Sound = Layer.Sounds.IsValidIndex(Pick) ? Layer.Sounds[Pick].Get() : nullptr;
		if (!Sound)
		{
			continue;
		}
		Last = Pick;
		const float Pitch = SoundRandom.FRandRange(Layer.PitchMin, FMath::Max(Layer.PitchMin, Layer.PitchMax));

		// Combat sounds are not UI sounds: they pause with the world (15-Audio-VFX "Settings and mix").
		UAudioComponent* Component = nullptr;
		if (!Cue->bSpatialized)
		{
			if (bLoop)
			{
				Component = UGameplayStatics::CreateSound2D(World, Sound, Layer.Volume, Pitch, 0.f, Layer.Concurrency, false, /*bAutoDestroy*/ true);
				if (Component)
				{
					Component->bIsUISound = false;
					Component->Play();
				}
			}
			else
			{
				UGameplayStatics::PlaySound2D(World, Sound, Layer.Volume, Pitch, 0.f, Layer.Concurrency, nullptr, /*bIsUISound*/ false);
			}
		}
		else if (bLoop && AttachTo)
		{
			Component = UGameplayStatics::SpawnSoundAttached(Sound, AttachTo, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset, /*bStopWhenAttachedToDestroyed*/ true, Layer.Volume, Pitch, 0.f, Cue->Attenuation, Layer.Concurrency);
		}
		else if (bLoop)
		{
			Component = UGameplayStatics::SpawnSoundAtLocation(World, Sound, Location, FRotator::ZeroRotator, Layer.Volume, Pitch, 0.f,
				Cue->Attenuation, Layer.Concurrency);
		}
		else
		{
			UGameplayStatics::PlaySoundAtLocation(World, Sound, Location, FRotator::ZeroRotator, Layer.Volume, Pitch, 0.f, Cue->Attenuation,
				Layer.Concurrency);
		}
		if (Component && OutLoops)
		{
			OutLoops->Add(Component);
		}
		++Started;
		++CueSoundsPlayed;
		UE_LOG(LogDwPresentation, Verbose, TEXT("Cue sound: %s %s layer %d -> %s (variation %d, pitch %.2f)"), *CueTag.ToString(),
			*UEnum::GetValueAsString(Section), LayerIndex, *Sound->GetName(), Pick, Pitch);
		OnCueSoundPlayed.Broadcast(CueTag, Section, Sound);
	}
	return Started;
}

void UDwPresentationSubsystem::CapEffectLifetime(UNiagaraComponent* Effect, float Seconds)
{
	UWorld* World = GetWorld();
	if (!Effect || !World || Seconds <= 0.f)
	{
		return;
	}
	FTimerHandle& Timer = CappedEffects.FindOrAdd(Effect);
	World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::ExpireCappedEffect,
		TObjectKey<UNiagaraComponent>(Effect)), Seconds, false);
	Effect->OnSystemFinished.AddUniqueDynamic(this, &ThisClass::HandleCappedEffectFinished);
}

void UDwPresentationSubsystem::HandleCappedEffectFinished(UNiagaraComponent* Effect)
{
	if (!Effect)
	{
		return;
	}
	Effect->OnSystemFinished.RemoveDynamic(this, &ThisClass::HandleCappedEffectFinished);
	FTimerHandle Timer;
	if (CappedEffects.RemoveAndCopyValue(Effect, Timer) && GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(Timer);
	}
}

void UDwPresentationSubsystem::ExpireCappedEffect(TObjectKey<UNiagaraComponent> Effect)
{
	CappedEffects.Remove(Effect);
	if (UNiagaraComponent* Component = Effect.ResolveObjectPtr())
	{
		Component->OnSystemFinished.RemoveDynamic(this, &ThisClass::HandleCappedEffectFinished);
		Component->Deactivate(); // AutoRelease: back to the pool once the particles die out
	}
}
