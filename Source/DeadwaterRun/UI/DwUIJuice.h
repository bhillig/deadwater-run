// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UUserWidget;
class UWidgetAnimation;

/**
 * The juice contract with ui-juice-engineer (Docs/Architecture/14-UI.md "Juice hooks"). Widgets may define these named
 * widget animations; C++ plays them when present. They are never required for logic and never block input
 * (rule: juice < 0.2 s, interruptible). UI sounds go through UDwUISoundMap by UI.Sound.* tag.
 */
namespace DwJuice
{
	DEADWATERRUN_API extern const FName Intro;      // Anim_Intro: screen activated
	DEADWATERRUN_API extern const FName Outro;      // Anim_Outro: screen deactivated
	DEADWATERRUN_API extern const FName Hover;      // Anim_Hover: pointer entered an interactive widget
	DEADWATERRUN_API extern const FName Press;      // Anim_Press: interactive widget clicked
	DEADWATERRUN_API extern const FName Disabled;   // Anim_Disabled: interactive widget became unavailable
	DEADWATERRUN_API extern const FName Highlight;  // Anim_Highlight: selected / armed / needs attention

	/** The widget animation property named Name on Widget's class, or nullptr. */
	DEADWATERRUN_API UWidgetAnimation* FindAnimation(const UUserWidget& Widget, FName Name);

	/** Plays the named animation if the widget has it. Returns false (and does nothing) if it doesn't. */
	DEADWATERRUN_API bool Play(UUserWidget& Widget, FName Name);

	/** Plays the UI sound mapped to Tag in DA_UISoundMap (via the UI manager), if any. */
	DEADWATERRUN_API void PlaySound(const UUserWidget& Widget, const FGameplayTag& Tag);
}
