// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "DwPrimaryLayoutWidget.generated.h"

class UCommonActivatableWidgetStack;

/**
 * The root of all Deadwater UI (WBP_PrimaryLayout, Docs/Architecture/14-UI.md D1): four CommonUI stacks, bottom to top
 * UI.Layer.Game (HUD, map, shop...), UI.Layer.GameMenu (pause, in-game settings), UI.Layer.Menu (main menu, summary),
 * UI.Layer.Modal (confirm dialogs). Created once per local player by UDwUIManagerSubsystem.
 */
UCLASS(Abstract)
class DEADWATERRUN_API UDwPrimaryLayoutWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** The stack for a UI.Layer.* tag, or nullptr for an unknown tag. */
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI")
	UCommonActivatableWidgetStack* GetLayer(FGameplayTag LayerTag) const;

protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonActivatableWidgetStack> GameLayer;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonActivatableWidgetStack> GameMenuLayer;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonActivatableWidgetStack> MenuLayer;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonActivatableWidgetStack> ModalLayer;
};
