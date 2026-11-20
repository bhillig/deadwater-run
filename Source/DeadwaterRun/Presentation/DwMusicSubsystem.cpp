// Copyright Brandon Hillig 2026

#include "Presentation/DwMusicSubsystem.h"

#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "AudioThread.h"
#include "Combat/DwCombatSubsystem.h"
#include "Components/AudioComponent.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Core/DwLog.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Presentation/DwMusicSet.h"
#include "Run/DwRun.h"
#include "Run/DwRunSubsystem.h"
#include "Shell/DwGameUserSettings.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace DwMusic
{
	const FDwAudioConfig* GetAudio()
	{
		const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
		return GameData ? &GameData->Audio : nullptr;
	}
}

#if !UE_BUILD_SHIPPING
namespace DwMusicDev
{
	/**
	 * Dw.Audio.Dump: the music state and each sound class's resolved volume on every audio device (after the class tree
	 * and the mixes), read on the audio thread. Proves the settings volumes reach the classes.
	 */
	FAutoConsoleCommandWithWorld Dump(TEXT("Dw.Audio.Dump"), TEXT("Dw.Audio.Dump: music state and resolved sound class volumes (dev)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (const UDwMusicSubsystem* Music = UDwMusicSubsystem::Get(World))
			{
				UE_LOG(LogDwPresentation, Display, TEXT("Dw.Audio.Dump: music state %s, sound %s, paused %d"), *Music->GetMusicState().ToString(),
					*GetNameSafe(Music->GetCurrentSound()), Music->IsPaused() ? 1 : 0);
			}
			const FDwAudioConfig* Audio = DwMusic::GetAudio();
			FAudioDeviceManager* Devices = GEngine ? GEngine->GetAudioDeviceManager() : nullptr;
			if (!Audio || !Devices)
			{
				UE_LOG(LogDwPresentation, Display, TEXT("Dw.Audio.Dump: no audio device or no DA_GameData.Audio."));
				return;
			}
			const TArray<USoundClass*> Classes = { Audio->MasterClass, Audio->MusicClass, Audio->SfxClass, Audio->UiClass };
			Devices->IterateOverAllDevices([Classes](Audio::FDeviceId Id, FAudioDevice* Device)
			{
				FAudioThread::RunCommandOnAudioThread([Device, Id, Classes]()
				{
					for (USoundClass* Class : Classes)
					{
						const FSoundClassProperties* Properties = Class ? Device->GetSoundClassCurrentProperties(Class) : nullptr;
						UE_LOG(LogDwPresentation, Display, TEXT("Dw.Audio.Dump: device %u %s volume %.3f"), Id, *GetNameSafe(Class),
							Properties ? Properties->Volume : -1.f);
					}
				});
			});
		}));
}
#endif

UDwMusicSubsystem* UDwMusicSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UDwMusicSubsystem>() : nullptr;
}

void UDwMusicSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UDwRunSubsystem* Runs = Collection.InitializeDependency<UDwRunSubsystem>())
	{
		Runs->OnRunStarted.AddUniqueDynamic(this, &ThisClass::HandleRunStarted);
		Runs->OnRunCleared.AddUniqueDynamic(this, &ThisClass::HandleRunCleared);
		BindRun(Runs->GetRun());
	}
	WorldInitHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &ThisClass::HandleWorldInitializedActors);
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
}

void UDwMusicSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	BindRun(nullptr);
	BindCombat(nullptr);
	FadeOutCurrent(0.f);
	for (UAudioComponent* Old : Fading)
	{
		if (IsValid(Old))
		{
			Old->Stop();
		}
	}
	Fading.Reset();
	Super::Deinitialize();
}

const UDwMusicSet* UDwMusicSubsystem::GetMusicSet() const
{
	if (MusicSetOverride)
	{
		return MusicSetOverride;
	}
	const FDwAudioConfig* Audio = DwMusic::GetAudio();
	return Audio ? Audio->MusicSet.Get() : nullptr;
}

FDwMusicTransition UDwMusicSubsystem::DecideTransition(const UDwMusicSet* Set, const FGameplayTag& Current, const FGameplayTag& Requested)
{
	FDwMusicTransition Transition;
	if (Requested == Current)
	{
		return Transition;
	}
	Transition.bChanged = true;
	Transition.Track = Set ? Set->FindTrack(Requested) : nullptr;
	if (!Set)
	{
		return Transition;
	}
	if (Transition.Track && !Transition.Track->bLoop)
	{
		Transition.FadeOutSeconds = Set->StingerFadeOutSeconds; // the stinger itself hits at full volume
	}
	else
	{
		Transition.FadeOutSeconds = Set->CrossfadeSeconds;
		Transition.FadeInSeconds = Transition.Track ? Set->CrossfadeSeconds : 0.f;
	}
	return Transition;
}

FGameplayTag UDwMusicSubsystem::GetCombatState(bool bKrakenFight)
{
	return bKrakenFight ? DwTags::Music_Boss : DwTags::Music_Combat;
}

bool UDwMusicSubsystem::SetMusicState(FGameplayTag Requested)
{
	const FDwMusicTransition Transition = DecideTransition(GetMusicSet(), State, Requested);
	if (!Transition.bChanged)
	{
		return false;
	}
	UE_LOG(LogDwPresentation, Log, TEXT("Music: %s -> %s%s"), *State.ToString(), *Requested.ToString(),
		Transition.Track ? TEXT("") : TEXT(" (no track: silence)"));
	State = Requested;
	FadeOutCurrent(Transition.FadeOutSeconds);
	CurrentSound = nullptr;
	bInIntro = false;
	if (const FDwMusicTrack* Track = Transition.Track)
	{
		USoundBase* Intro = Track->bLoop ? Track->Intro.LoadSynchronous() : nullptr;
		bInIntro = Intro != nullptr;
		CurrentSound = bInIntro ? Intro : Track->Sound.LoadSynchronous();
		if (CurrentSound)
		{
			StartSound(CurrentSound, Track->Volume, Transition.FadeInSeconds);
		}
	}
	return true;
}

void UDwMusicSubsystem::SetPaused(bool bInPaused)
{
	if (bPaused == bInPaused)
	{
		return;
	}
	bPaused = bInPaused;
	const FDwAudioConfig* Audio = DwMusic::GetAudio();
	UWorld* World = GetAudioWorld();
	if (!Audio || !Audio->PauseMix || !World)
	{
		return;
	}
	if (bPaused)
	{
		UGameplayStatics::PushSoundMixModifier(World, Audio->PauseMix);
	}
	else
	{
		UGameplayStatics::PopSoundMixModifier(World, Audio->PauseMix);
	}
}

void UDwMusicSubsystem::BindRun(UDwRun* Run)
{
	if (BoundRun)
	{
		BoundRun->OnPhaseChanged.RemoveAll(this);
	}
	BoundRun = Run;
	if (Run)
	{
		Run->OnPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandleRunPhaseChanged);
		HandleRunPhaseChanged(Run->GetPhase(), Run->GetPhase());
	}
}

void UDwMusicSubsystem::BindCombat(UDwCombatSubsystem* Combat)
{
	if (UDwCombatSubsystem* Old = BoundCombat.Get())
	{
		Old->OnCombatStarted.RemoveAll(this);
		Old->OnCombatEvent.RemoveAll(this);
	}
	BoundCombat = Combat;
	if (Combat)
	{
		Combat->OnCombatStarted.AddUniqueDynamic(this, &ThisClass::HandleCombatStarted);
		Combat->OnCombatEvent.AddUniqueDynamic(this, &ThisClass::HandleCombatEvent);
	}
}

void UDwMusicSubsystem::HandleRunStarted(UDwRun* Run)
{
	BindRun(Run);
}

void UDwMusicSubsystem::HandleRunCleared()
{
	BindRun(nullptr); // the music plays on until the next screen asks for another state
}

void UDwMusicSubsystem::HandleRunPhaseChanged(EDwRunPhase NewPhase, EDwRunPhase OldPhase)
{
	// Fights choose their own state when they start (Combat or Boss).
	const UDwMusicSet* Set = GetMusicSet();
	const FGameplayTag PhaseState = Set && NewPhase != EDwRunPhase::Combat ? Set->GetPhaseState(NewPhase) : FGameplayTag();
	if (PhaseState.IsValid())
	{
		SetMusicState(PhaseState);
	}
}

void UDwMusicSubsystem::HandleCombatStarted()
{
	if (const UDwCombatSubsystem* Combat = BoundCombat.Get())
	{
		SetMusicState(GetCombatState(Combat->GetSetup().Kraken != nullptr));
	}
}

void UDwMusicSubsystem::HandleCombatEvent(const FDwCombatEvent& Event)
{
	// B7: the Kraken's phase change carries the phase's music state.
	if (Event.Type == EDwCombatEventType::PhaseChanged && Event.Tag.IsValid())
	{
		SetMusicState(Event.Tag);
	}
}

bool UDwMusicSubsystem::IsOurGameWorld(const UWorld* World) const
{
	return World && World->IsGameWorld() && World->GetGameInstance() == GetGameInstance();
}

void UDwMusicSubsystem::HandleWorldInitializedActors(const FActorsInitializedParams& Params)
{
	UWorld* World = Params.World;
	if (!IsOurGameWorld(World))
	{
		return;
	}
	const FDwAudioConfig* Audio = DwMusic::GetAudio();
	if (Audio && Audio->BaseMix && !MixWorlds.Contains(World))
	{
		UGameplayStatics::PushSoundMixModifier(World, Audio->BaseMix);
		MixWorlds.Add(World);
	}
	if (const UDwGameUserSettings* Settings = UDwGameUserSettings::Get())
	{
		Settings->ApplyAudio();
	}
	BindCombat(World->GetSubsystem<UDwCombatSubsystem>());
	// A state set before any world existed (or during travel) starts now.
	if (!Current && CurrentSound)
	{
		const FDwMusicTrack* Track = GetMusicSet() ? GetMusicSet()->FindTrack(State) : nullptr;
		StartSound(CurrentSound, Track ? Track->Volume : 1.f, 0.f);
	}
}

void UDwMusicSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (!World || !MixWorlds.Contains(World))
	{
		return;
	}
	MixWorlds.Remove(World);
	if (const FDwAudioConfig* Audio = DwMusic::GetAudio())
	{
		if (Audio->BaseMix)
		{
			UGameplayStatics::PopSoundMixModifier(World, Audio->BaseMix);
		}
		if (bPaused && Audio->PauseMix)
		{
			UGameplayStatics::PopSoundMixModifier(World, Audio->PauseMix);
		}
	}
	bPaused = false; // the next world starts unpaused
	if (UDwCombatSubsystem* Combat = BoundCombat.Get(); Combat && Combat->GetWorld() == World)
	{
		BindCombat(nullptr);
	}
}

UWorld* UDwMusicSubsystem::GetAudioWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

void UDwMusicSubsystem::StartSound(USoundBase* Sound, float Volume, float FadeIn)
{
	UWorld* World = GetAudioWorld();
	if (!Sound || !World)
	{
		return; // no world yet: HandleWorldInitializedActors starts it
	}
	// 2D, a UI-class component (keeps playing under the pause menu, ducked by SM_Pause), survives level travel.
	UAudioComponent* Component = UGameplayStatics::CreateSound2D(World, Sound, Volume, 1.f, 0.f, nullptr,
		/*bPersistAcrossLevelTransition*/ true, /*bAutoDestroy*/ false);
	if (!Component)
	{
		return; // no audio device
	}
	Component->OnAudioFinishedNative.AddUObject(this, &ThisClass::HandleSoundFinished);
	if (FadeIn > 0.f)
	{
		Component->FadeIn(FadeIn, 1.f);
	}
	else
	{
		Component->Play();
	}
	Current = Component;
}

void UDwMusicSubsystem::FadeOutCurrent(float Seconds)
{
	Fading.RemoveAll([](const UAudioComponent* Old) { return !IsValid(Old) || !Old->IsPlaying(); });
	if (!Current)
	{
		return;
	}
	Current->OnAudioFinishedNative.RemoveAll(this);
	if (Seconds > 0.f && Current->IsPlaying())
	{
		Current->FadeOut(Seconds, 0.f);
		Fading.Add(Current);
	}
	else
	{
		Current->Stop();
	}
	Current = nullptr;
}

void UDwMusicSubsystem::HandleSoundFinished(UAudioComponent* Component)
{
	const UDwMusicSet* Set = GetMusicSet();
	const FDwMusicTrack* Track = Set ? Set->FindTrack(State) : nullptr;
	if (Component != Current || !Track)
	{
		return;
	}
	if (bInIntro)
	{
		// INTRO -> LOOP
		bInIntro = false;
		CurrentSound = Track->Sound.LoadSynchronous();
		Current->SetSound(CurrentSound);
		Current->Play();
	}
	else if (Track->bLoop)
	{
		Current->Play(); // a loop whose wave isn't flagged Looping
	}
	// A stinger is over: silence until the next state.
}
