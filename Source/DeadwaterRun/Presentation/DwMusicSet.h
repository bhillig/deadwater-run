// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Run/DwRunTypes.h"
#include "DwMusicSet.generated.h"

class USoundBase;

/** The music of one state (Music.Map, Music.Combat, ...). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwMusicTrack
{
	GENERATED_BODY()

	/** Optional lead-in, played once before Sound (the music pack's INTRO cut). Ignored for stingers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Music")
	TSoftObjectPtr<USoundBase> Intro;

	/** The state's music (the LOOP cut, or the whole stinger). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Music")
	TSoftObjectPtr<USoundBase> Sound;

	/** On: Sound repeats until the state changes. Off: a one-shot stinger (Victory, Defeat) - plays once, then silence. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Music")
	bool bLoop = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0"))
	float Volume = 1.f;
};

/**
 * Music state tag -> track (Docs/Architecture/15-Audio-VFX.md D2). The class is gameplay-engineer's; the contents of
 * DA_MusicSet are sound-wrangler's. Played by UDwMusicSubsystem. Tracks are soft references: a state's music loads when
 * the state is entered (run phase changes and fight starts, never mid-turn).
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwMusicSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** A state without a track is silence. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Music", meta = (Categories = "Music"))
	TMap<FGameplayTag, FDwMusicTrack> Tracks;

	/**
	 * The state each run phase plays. Combat is not listed: a fight picks Music.Combat, or Music.Boss when it is the
	 * Kraken, as it starts, and Kraken phases switch on from there. A phase missing here keeps the current music.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Music", meta = (Categories = "Music"))
	TMap<EDwRunPhase, FGameplayTag> PhaseStates;

	/** Seconds the old track fades out while the new one fades in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0", Units = "s"))
	float CrossfadeSeconds = 1.5f;

	/** Seconds the music fades out under a stinger, which itself starts at full volume. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0", Units = "s"))
	float StingerFadeOutSeconds = 0.5f;

	const FDwMusicTrack* FindTrack(const FGameplayTag& State) const { return Tracks.Find(State); }

	/** The state Phase plays, or an empty tag to keep the current music. */
	FGameplayTag GetPhaseState(EDwRunPhase Phase) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
