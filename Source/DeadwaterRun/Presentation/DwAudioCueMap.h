// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DwAudioCueMap.generated.h"

class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/** Which sounds of a cue entry play (Docs/Architecture/15-Audio-VFX.md "Classes"). */
UENUM(BlueprintType)
enum class EDwCueSoundSection : uint8
{
	/** A burst cue executing, or a looping (status) cue starting. */
	Start,
	/** A looping cue while it is active; stopped with a short fade when the cue is removed. */
	Loop,
	/** A looping cue being removed. */
	Stop
};

/**
 * One sound in a cue. Every layer of a section plays at the same time (Ship.Sink: crash + hull creak + splash); within a
 * layer one of the variations is picked at random, never the same one twice in a row.
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwCueSoundLayer
{
	GENERATED_BODY()

	/** Variations of the same sound (SW_MeleeHit_01..03). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound")
	TArray<TObjectPtr<USoundBase>> Sounds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0"))
	float Volume = 1.f;

	/** Pitch multiplier, picked uniformly between Min and Max on each play. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.1"))
	float PitchMin = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.1"))
	float PitchMax = 1.f;

	/** Replaces the sound's own concurrency (SCON_Hits, SCON_Cannon). Empty = the sound asset's settings. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundConcurrency> Concurrency;
};

/** Everything a cue tag plays. A burst cue uses Start only; a looping (status) cue uses all three. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwCueSound
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound")
	TArray<FDwCueSoundLayer> Start;

	/** Looping cues only: started while the cue is active, faded out on removal. The sounds must loop (wave Looping on). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound")
	TArray<FDwCueSoundLayer> Loop;

	/** Looping cues only: played when the cue is removed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound")
	TArray<FDwCueSoundLayer> Stop;

	/**
	 * Off (default): 2D. The sources are stereo, which UE does not spatialise, and the camera is top-down.
	 * On: played at the cue's location with Attenuation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound")
	bool bSpatialized = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound", meta = (EditCondition = "bSpatialized"))
	TObjectPtr<USoundAttenuation> Attenuation;

	const TArray<FDwCueSoundLayer>& GetSection(EDwCueSoundSection Section) const;
};

/**
 * GameplayCue tag -> combat sounds (Docs/Architecture/15-Audio-VFX.md D1). The class is gameplay-engineer's; the
 * contents of DA_AudioCueMap are sound-wrangler's. Read by UDwPresentationSubsystem::PlayCueSounds, which the Dw cue
 * notify bases call: combat sounds only ever play from GameplayCues, never from gameplay code.
 * Sounds are hard references so a hit never waits on a load; the whole map loads with DA_GameData.
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwAudioCueMap : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound", meta = (Categories = "GameplayCue"))
	TMap<FGameplayTag, FDwCueSound> Cues;

	/** CueTag's entry, else its nearest parent's (GameplayCue.Ability.SixPistols.Hit -> ...SixPistols); null if none. */
	const FDwCueSound* FindCue(const FGameplayTag& CueTag, FGameplayTag* OutMatchedTag = nullptr) const;

	/**
	 * A variation index in [0, NumVariations) that is never Previous when there are two or more; uniform over the rest.
	 * INDEX_NONE when there are no variations. Pure, so the no-repeat rule is testable without audio.
	 */
	static int32 PickVariation(int32 NumVariations, int32 Previous, FRandomStream& Random);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
