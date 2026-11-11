// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DwUIContentLibrary.generated.h"

/**
 * Creates or updates the DW-107 UI content headless, so it is reproducible and reviewable instead of hand-clicked
 * (Docs/Architecture/14-UI.md): CommonUI input data (DT_UIInputActions, B_UIInputData), the widget blueprints
 * (WBP_PrimaryLayout, WBP_CombatHud, WBP_UnitCard, WBP_ActionButton, WBP_FloatingText; DW-211: WBP_Map, WBP_MapNode,
 * WBP_Rewards, WBP_LevelUp, WBP_NodeShell, WBP_ChoiceButton), DA_UIScreenSet, an empty
 * DA_UISoundMap (created once, then owned by ui-juice-engineer) and the DA_GameData.UIScreenSet slot.
 *
 * Widget trees: structure and logic are C++ (BindWidget); this library is the layout. Every run rebuilds each WBP's
 * widget tree with the same names, so the layout is regenerated from code; widget animations (DW-402) are kept, and
 * since they bind by widget name they keep working. Run with Tools/CreateUIContent.ps1.
 */
UCLASS()
class DEADWATERRUNEDITOR_API UDwUIContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One line per asset touched or problem found; the last line is "CONTENT PASS" or "CONTENT FAIL". */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Content")
	static TArray<FString> CreateUIContent();
};
