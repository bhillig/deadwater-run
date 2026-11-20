// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Run/DwRunTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DwMusicSubsystem.generated.h"

class UAudioComponent;
class UDwCombatSubsystem;
class UDwMusicSet;
class UDwRun;
class USoundBase;
class UWorld;
struct FActorsInitializedParams;
struct FDwCombatEvent;
struct FDwMusicTrack;

/** What SetMusicState decided (pure, so the state rules are testable without audio). */
struct DEADWATERRUN_API FDwMusicTransition
{
	/** False: the state is already playing, nothing changes. */
	bool bChanged = false;
	/** The new state's track; null = silence. */
	const FDwMusicTrack* Track = nullptr;
	/** Fade-out of the old music, and fade-in of the new (0 for a stinger, which hits at once). */
	float FadeOutSeconds = 0.f;
	float FadeInSeconds = 0.f;
};

/**
 * The game's music and mix (Docs/Architecture/15-Audio-VFX.md D2). A game-instance subsystem so music survives the
 * menu -> L_Sea travel. Music follows state tags from DA_MusicSet:
 * - the run's phases (UDwMusicSet::PhaseStates: Map, Shop, Victory, Defeat, ...), bound when a run starts;
 * - fights: Music.Combat when a fight starts, Music.Boss for the Kraken, then each Kraken phase's MusicState
 *   (FDwCombatEvent PhaseChanged, B7);
 * - ADwMenuGameMode asks for Music.Menu.
 * It also owns the session mix: SM_Base is pushed in every game world (the settings volumes are class overrides on it,
 * UDwGameUserSettings::ApplyAudio) and SM_Pause while the game is paused (ADwPlayerController::SetPause).
 * Without an audio device (-nosound: tests, simulator) the state machine still runs; nothing plays.
 */
UCLASS()
class DEADWATERRUN_API UDwMusicSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UDwMusicSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Switches the music to State's track: a crossfade, or a stinger over a quick fade-out. Same state = no change. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Audio")
	bool SetMusicState(FGameplayTag State);

	UFUNCTION(BlueprintPure, Category = "Deadwater|Audio")
	FGameplayTag GetMusicState() const { return State; }

	/** The sound the current state plays now (its Intro, then its Sound); null for silence. Set without an audio device too. */
	USoundBase* GetCurrentSound() const { return CurrentSound; }

	/** Pushes or pops SM_Pause. */
	void SetPaused(bool bPaused);
	bool IsPaused() const { return bPaused; }

	/** DA_GameData.Audio.MusicSet, unless an override is set (tests). */
	const UDwMusicSet* GetMusicSet() const;
	void SetMusicSetOverride(UDwMusicSet* Set) { MusicSetOverride = Set; }

	/** The transition from Current to Requested under Set. */
	static FDwMusicTransition DecideTransition(const UDwMusicSet* Set, const FGameplayTag& Current, const FGameplayTag& Requested);

	/** The state a fight starts with: Music.Boss for the Kraken, else Music.Combat. */
	static FGameplayTag GetCombatState(bool bKrakenFight);

	/** Follows Run's phases (and nothing else). Called when a run starts; public for tests. */
	void BindRun(UDwRun* Run);
	/** Follows Combat's fights. Called for every game world of this game instance; public for tests. */
	void BindCombat(UDwCombatSubsystem* Combat);

private:
	UFUNCTION() void HandleRunStarted(UDwRun* Run);
	UFUNCTION() void HandleRunCleared();
	UFUNCTION() void HandleRunPhaseChanged(EDwRunPhase NewPhase, EDwRunPhase OldPhase);
	UFUNCTION() void HandleCombatStarted();
	UFUNCTION() void HandleCombatEvent(const FDwCombatEvent& Event);

	void HandleWorldInitializedActors(const FActorsInitializedParams& Params);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	bool IsOurGameWorld(const UWorld* World) const;

	/** Starts Sound on a new component that survives level travel; fades it in over FadeIn. */
	void StartSound(USoundBase* Sound, float Volume, float FadeIn);
	void FadeOutCurrent(float Seconds);
	void HandleSoundFinished(UAudioComponent* Component);
	UWorld* GetAudioWorld() const;

	FGameplayTag State;
	bool bPaused = false;
	/** Playing the current track's Intro; its Sound follows when the Intro finishes. */
	bool bInIntro = false;

	UPROPERTY(Transient) TObjectPtr<UDwMusicSet> MusicSetOverride;
	UPROPERTY(Transient) TObjectPtr<USoundBase> CurrentSound;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Current;
	/** Old tracks still fading out; held so they aren't collected mid-fade. */
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Fading;
	UPROPERTY(Transient) TObjectPtr<UDwRun> BoundRun;
	TWeakObjectPtr<UDwCombatSubsystem> BoundCombat;
	/** Worlds SM_Base was pushed in (popped on cleanup). */
	TArray<TWeakObjectPtr<UWorld>> MixWorlds;

	FDelegateHandle WorldInitHandle;
	FDelegateHandle WorldCleanupHandle;
};
