// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DwAudioContentLibrary.generated.h"

/**
 * Creates or updates the DW-114 audio and cue content headless (Docs/Architecture/15-Audio-VFX.md):
 * - DA_AudioCueMap and DA_MusicSet in Audio/: created empty; their contents are sound-wrangler's (DW-311), so an
 *   existing map is never touched and the music set only gets its run-phase states when it has none
 * - GC_Hit_Melee, GC_Cannon_Fire, GC_Cannon_Impact, GC_Unit_Death in VFX/Cues: UDwGameplayCueNotify_Burst blueprints
 *   with placeholder Synty Niagara systems until DW-307
 * - DA_GameData.Audio: the two assets, SM_Base / SM_Pause and the four sound classes (DW-113, sound-wrangler)
 * Idempotent. Run by Tools/CreateCombatContent.ps1 after the art content.
 */
UCLASS()
class DEADWATERRUNEDITOR_API UDwAudioContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One line per asset touched or problem found; the last line is "CONTENT PASS" or "CONTENT FAIL". */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Content")
	static TArray<FString> CreateAudioContent();
};
