// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DwUIScreenSet.generated.h"

class UDwActivatableScreen;
class UDwFloatingTextWidget;
class UDwPrimaryLayoutWidget;
class UDwUISoundMap;

/**
 * DA_UIScreenSet (Docs/Architecture/14-UI.md): which widget class UDwUIManagerSubsystem creates for each screen, so
 * C++ never hard-codes widget asset paths. Referenced from DA_GameData.UIScreenSet.
 * Doc 14 keys screens by run phase (EDwRunPhase -> soft class). UDwRun and EDwRunPhase arrive with DW-201, so for now
 * each phase is a named slot; DW-201/DW-211 turn the slots into the phase map without changing the widgets.
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwUIScreenSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** WBP_PrimaryLayout: the four UI.Layer.* stacks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout")
	TSoftClassPtr<UDwPrimaryLayoutWidget> PrimaryLayout;

	/** Phase Combat: WBP_CombatHud on UI.Layer.Game. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phases")
	TSoftClassPtr<UDwActivatableScreen> CombatHud;

	/** WBP_FloatingText, pooled by UDwFloatingTextManager. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	TSoftClassPtr<UDwFloatingTextWidget> FloatingText;

	/** DA_UISoundMap (contents: ui-juice-engineer, DW-402). Optional. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Juice")
	TSoftObjectPtr<UDwUISoundMap> SoundMap;
};
