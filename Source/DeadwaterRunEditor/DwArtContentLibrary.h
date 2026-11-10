// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DwArtContentLibrary.generated.h"

/**
 * Creates or updates the Synty art pass of DW-307 headless, so it is reproducible and reviewable instead of hand-clicked:
 * - M_TileOverlay / M_UnitRing and BP_Board (translucent tile highlights, grid outline, no grey-box decks)
 * - BP_Ship_Sloop_Player / _Enemy: hull pieces read from Synty's small-ship presets, masts and deck props as dressing
 * - BP_Unit_*: Synty pirates with their attachments, a weapon and looping idle / run animations
 * - DA_SeaMood_Day / _Storm and the ADwSeaEnvironment in L_Sea
 * - the slots that use them: unit definitions, the placeholder encounter, deck layout alignment and DA_GameData
 * Every number that places art (hull alignment, prop offsets, weapon grips, mood lighting) is in this file's tables.
 * Idempotent. Run by Tools/CreateCombatContent.ps1 after the combat and AI content.
 */
UCLASS()
class DEADWATERRUNEDITOR_API UDwArtContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One line per asset touched or problem found; the last line is "CONTENT PASS" or "CONTENT FAIL". */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Content")
	static TArray<FString> CreateArtContent();
};
