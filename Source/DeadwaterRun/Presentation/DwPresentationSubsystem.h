// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Math/RandomStream.h"
#include "Presentation/DwAudioCueMap.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "DwPresentationSubsystem.generated.h"

class UAudioComponent;
class UNiagaraComponent;
class USceneComponent;
class USoundBase;

/** A cue sound about to play: the cue tag it came from, the section, and the variation picked. Tests and dev logs. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FDwOnCueSoundPlayed, const FGameplayTag& /*CueTag*/, EDwCueSoundSection /*Section*/, USoundBase* /*Sound*/);

/**
 * How abilities present themselves (Docs/Architecture/02 "Presentation tasks and instant mode").
 * Instant mode: every presentation task completes synchronously and gameplay cues are suppressed, so tests and the
 * headless simulator run whole fights in one call stack. Defaults to instant when the process can't render.
 * Presentation never decides an outcome: abilities change state only at commit, impact and end.
 *
 * Also plays GameplayCue sounds (Docs/Architecture/15-Audio-VFX.md D1): the Dw cue notify bases call PlayCueSounds with
 * their cue tag, which looks the sounds up in DA_AudioCueMap. It lives here, per world, because it needs the instant
 * flag and keeps per-world state (the last variation of each layer, for the no-repeat rule).
 */
UCLASS()
class DEADWATERRUN_API UDwPresentationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Deadwater|Presentation")
	bool IsInstant() const { return bInstant; }

	UFUNCTION(BlueprintCallable, Category = "Deadwater|Presentation")
	void SetInstant(bool bInInstant) { bInstant = bInInstant; }

	/** Game-speed setting (UDwGameUserSettings, U1): 1 = normal, 2 = twice as fast. Set at world start and on Apply. */
	UFUNCTION(BlueprintPure, Category = "Deadwater|Presentation")
	float GetPlaybackRate() const { return PlaybackRate; }

	UFUNCTION(BlueprintCallable, Category = "Deadwater|Presentation")
	void SetPlaybackRate(float Rate) { PlaybackRate = FMath::Clamp(Rate, 0.25f, 4.f); }

	/** Instant if there is no subsystem (e.g. a world without one): the safe default for logic. */
	static bool IsInstant(const UObject* WorldContext);
	static float GetPlaybackRate(const UObject* WorldContext);

	/**
	 * Presentation (VFX, cue sounds) should play in World: false in instant mode. Unlike IsInstant, a world without the
	 * subsystem (an editor preview) presents.
	 */
	static bool ShouldPresent(const UWorld* World);

	/**
	 * Plays one section of CueTag's DA_AudioCueMap entry (every layer, one variation each). Nothing in instant mode or
	 * without an entry. 2D unless the entry is spatialized, then at Location (attached to AttachTo when given).
	 * Loop sounds are added to OutLoops so the caller can stop them. Returns the number of sounds started.
	 * Only the cue notify bases call this: combat sounds never play from gameplay code.
	 */
	int32 PlayCueSounds(const FGameplayTag& CueTag, EDwCueSoundSection Section, const FVector& Location, USceneComponent* AttachTo = nullptr,
		TArray<TObjectPtr<UAudioComponent>>* OutLoops = nullptr);

	/** DA_GameData.Audio.CueMap, unless an override is set. */
	const UDwAudioCueMap* GetCueMap() const;

	/** Replaces DA_AudioCueMap for this world (tests, Dw.Audio.PlaceholderCue). Null restores it. */
	void SetCueMapOverride(UDwAudioCueMap* Map) { CueMapOverride = Map; }

	/** Cue sounds started in this world (a sound counts even when the process has no audio device). */
	int32 GetCueSoundsPlayed() const { return CueSoundsPlayed; }

	/**
	 * Cue events that spawned their effects in this world (a Burst executing, a loop starting). Counted by the cue bases
	 * before the engine spawns the Niagara systems, which a headless world then culls (no view), so tests read this.
	 */
	int32 GetCuesPresented() const { return CuesPresented; }
	void NoteCuePresented() { ++CuesPresented; }

	FDwOnCueSoundPlayed OnCueSoundPlayed;

	/**
	 * Deactivates Effect after Seconds unless it finishes first (cue VFX stay bounded, 15-Audio-VFX "Classes").
	 * Safe with the Niagara pool: the cap is dropped when the system finishes, before the pool can reuse the component.
	 */
	void CapEffectLifetime(UNiagaraComponent* Effect, float Seconds);

	/** Cue VFX currently under a lifetime cap (tests). */
	int32 GetNumCappedEffects() const { return CappedEffects.Num(); }

private:
	UFUNCTION() void HandleCappedEffectFinished(UNiagaraComponent* Effect);
	void ExpireCappedEffect(TObjectKey<UNiagaraComponent> Effect);

	bool bInstant = false;
	float PlaybackRate = 1.f;

	UPROPERTY(Transient) TObjectPtr<UDwAudioCueMap> CueMapOverride;
	/** (matched cue tag, section, layer) -> last variation played. */
	TMap<TTuple<FGameplayTag, EDwCueSoundSection, int32>, int32> LastVariations;
	/** Presentation only, so not the fight's seeded stream. */
	FRandomStream SoundRandom;
	int32 CueSoundsPlayed = 0;
	int32 CuesPresented = 0;
	TMap<TObjectKey<UNiagaraComponent>, FTimerHandle> CappedEffects;
};
