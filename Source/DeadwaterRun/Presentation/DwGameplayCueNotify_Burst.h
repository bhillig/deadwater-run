// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Burst.h"
#include "DwGameplayCueNotify_Burst.generated.h"

class UNiagaraSystem;
struct FGameplayCueNotify_SpawnContext;

/**
 * One-shot cue (hits, cannon fire and impact, death, heal; Docs/Architecture/15-Audio-VFX.md "Classes"). The GC_*
 * assets derive from it. On execute it spawns the Burst's VFX (pooled by the engine, AutoRelease), caps their lifetime,
 * then plays the cue tag's sounds from DA_AudioCueMap. Sounds never live on the cue asset itself (BurstSounds must stay
 * empty): the cue is gameplay-engineer's, the sounds are sound-wrangler's.
 * Nothing spawns or plays in instant mode (tests, simulator), even if a cue gets past bSuppressGameplayCues.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Dw GCN Burst", ShortTooltip = "A one-off cue: VFX from the asset, sounds from DA_AudioCueMap."))
class DEADWATERRUN_API UDwGameplayCueNotify_Burst : public UGameplayCueNotify_Burst
{
	GENERATED_BODY()

public:
	/**
	 * Longest any spawned Niagara system may run before it is deactivated (and returns to the pool), so a looping
	 * system dropped into a burst can't run forever. 0 = no cap (only for systems that end by themselves).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GCN Effects", meta = (ClampMin = "0", Units = "s"))
	float MaxEffectSeconds = 3.f;

	/** The Niagara systems the burst spawns, in order (tests, content checks). */
	TArray<UNiagaraSystem*> GetBurstNiagaraSystems() const;

	/** Sounds set on the asset's own effect lists; must be 0 (they come from DA_AudioCueMap). */
	int32 GetNumAssetSounds() const;

#if WITH_EDITOR
	/** Replaces the burst's particles with Systems (UDwAudioContentLibrary builds the GC_* assets with it). */
	void SetBurstNiagaraSystems(const TArray<UNiagaraSystem*>& Systems);
#endif

	/**
	 * Entries in a protected array of an engine cue effects struct (FGameplayCueNotify_BurstEffects::BurstSounds, ...),
	 * read through reflection since the engine exposes no accessor.
	 */
	static int32 GetEffectsArrayNum(const UScriptStruct& Struct, const void* Effects, FName Member);

	/** The tag the cue was fired with (a child of this notify's tag, or the tag itself): the DA_AudioCueMap key. */
	static FGameplayTag GetSoundTag(const FGameplayTag& NotifyTag, const FGameplayCueParameters& Parameters);

	/** Where a spatialized cue sound plays: the cue's spawn transform, else the target's location. */
	static FVector GetSoundLocation(const FGameplayCueNotify_SpawnContext& SpawnContext, const FGameplayCueNotify_PlacementInfo& Placement,
		const AActor& Target);

protected:
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
