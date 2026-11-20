// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Looping.h"
#include "Presentation/DwAudioCueMap.h"
#include "DwGameplayCueNotify_Looping.generated.h"

class UAudioComponent;

/**
 * Status loop cue (frozen ice, fear smoke, guarded shield, enraged Kraken; Docs/Architecture/15-Audio-VFX.md "Classes").
 * The engine's looping notify spawns the VFX; this adds the cue tag's DA_AudioCueMap sounds: Start when the status is
 * applied, Loop while it lasts (faded out on removal or recycle), Stop when it ends. Like the Burst base, sounds never
 * live on the asset (its sound lists must stay empty), and nothing spawns or plays in instant mode.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Dw GCN Looping", ShortTooltip = "A status loop: VFX from the asset, sounds from DA_AudioCueMap."))
class DEADWATERRUN_API ADwGameplayCueNotify_Looping : public AGameplayCueNotify_Looping
{
	GENERATED_BODY()

public:
	/** Fade on the Loop sounds when the cue ends. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GCN Sounds", meta = (ClampMin = "0", Units = "s"))
	float LoopFadeOutSeconds = 0.3f;

	/** Loop sounds this instance is playing (tests; stays 0 without an audio device). */
	int32 GetNumLoopSounds() const { return LoopSounds.Num(); }

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool Recycle() override;

protected:
	virtual bool OnActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	/** Plays one section of this cue's sounds for Target. */
	void PlaySection(AActor* Target, const FGameplayCueParameters& Parameters, EDwCueSoundSection Section);
	void StopLoopSounds();

	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> LoopSounds;
	/** WhileActive can arrive more than once per activation (relevancy); the loop starts once. */
	bool bLoopStarted = false;
};
