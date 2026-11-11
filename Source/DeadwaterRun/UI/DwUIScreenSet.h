// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Run/DwRunTypes.h"
#include "DwUIScreenSet.generated.h"

class UDwActivatableScreen;
class UDwFloatingTextWidget;
class UDwPrimaryLayoutWidget;
class UDwUISoundMap;

/**
 * DA_UIScreenSet (Docs/Architecture/14-UI.md): which widget class UDwUIManagerSubsystem creates for each screen, so
 * C++ never hard-codes widget asset paths. Referenced from DA_GameData.UIScreenSet.
 * Doc 14 keys screens by run phase: one named slot per phase (readable in the editor), GetPhaseScreen maps them.
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

	/** Phase Map: WBP_Map on UI.Layer.Game (DW-211). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phases")
	TSoftClassPtr<UDwActivatableScreen> Map;

	/** Phase Rewards: WBP_Rewards (DW-211). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phases")
	TSoftClassPtr<UDwActivatableScreen> Rewards;

	/** Phase LevelUp: WBP_LevelUp (DW-211). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phases")
	TSoftClassPtr<UDwActivatableScreen> LevelUp;

	/** Phase Shop: WBP_NodeShell until DW-204 brings WBP_Shop. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phases")
	TSoftClassPtr<UDwActivatableScreen> Shop;

	/** Phase Event: WBP_NodeShell until DW-205 brings WBP_Event. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phases")
	TSoftClassPtr<UDwActivatableScreen> Event;

	/** WBP_Menu (DW-210): main menu, pause, confirm and run summary - a title, lines and buttons. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shell")
	TSoftClassPtr<UDwActivatableScreen> Menu;

	/** WBP_CaptainSelect (DW-210). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shell")
	TSoftClassPtr<UDwActivatableScreen> CaptainSelect;

	/** WBP_Settings (DW-210). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shell")
	TSoftClassPtr<UDwActivatableScreen> Settings;

	/** The screen class for a run phase (Combat: the HUD); null for phases without one (Victory/Defeat: DW-210). */
	TSoftClassPtr<UDwActivatableScreen> GetPhaseScreen(EDwRunPhase Phase) const
	{
		switch (Phase)
		{
		case EDwRunPhase::Map:     return Map;
		case EDwRunPhase::Combat:  return CombatHud;
		case EDwRunPhase::Rewards: return Rewards;
		case EDwRunPhase::LevelUp: return LevelUp;
		case EDwRunPhase::Shop:    return Shop;
		case EDwRunPhase::Event:   return Event;
		default:                   return nullptr;
		}
	}

	/** WBP_FloatingText, pooled by UDwFloatingTextManager. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	TSoftClassPtr<UDwFloatingTextWidget> FloatingText;

	/** DA_UISoundMap (contents: ui-juice-engineer, DW-402). Optional. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Juice")
	TSoftObjectPtr<UDwUISoundMap> SoundMap;
};
