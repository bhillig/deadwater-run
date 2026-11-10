// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "DwUserWidget.generated.h"

class UDwViewModelBase;

/**
 * Base of every non-screen Deadwater widget (Docs/Architecture/14-UI.md). Structure and logic live in C++ subclasses
 * (BindWidget); WBP_* only lay out and style. A widget shows one view model and refreshes in NativeOnViewModelChanged
 * whenever the view model broadcasts OnChanged - it never reads game state itself and never ticks to poll.
 * Juice: interactive subclasses call PlayJuice(DwJuice::Hover/Press/Disabled/Highlight) and PlayUISound(UI.Sound.*).
 */
UCLASS(Abstract)
class DEADWATERRUN_API UDwUserWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Deadwater|UI")
	void SetViewModel(UDwViewModelBase* InViewModel);

	UFUNCTION(BlueprintPure, Category = "Deadwater|UI")
	UDwViewModelBase* GetViewModel() const { return ViewModel; }

	template <typename T>
	T* GetViewModel() const { return Cast<T>(ViewModel); }

	/** Plays a named juice animation (Anim_Hover, ...) if this widget has it. Never required for logic. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|UI|Juice")
	bool PlayJuice(FName AnimationName);

	/** Plays the UI sound mapped to a UI.Sound.* tag, if DA_UISoundMap has one. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|UI|Juice", meta = (GameplayTagFilter = "UI.Sound"))
	void PlayUISound(FGameplayTag SoundTag);

protected:
	/** The view model changed or broadcast OnChanged: refresh every bound widget from it. */
	virtual void NativeOnViewModelChanged() {}

	/** Blueprint styling hook after NativeOnViewModelChanged. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Deadwater|UI")
	void OnViewModelChanged();

	virtual void NativeDestruct() override;

private:
	UFUNCTION() void HandleViewModelChanged(UDwViewModelBase* Changed);

	UPROPERTY(Transient) TObjectPtr<UDwViewModelBase> ViewModel;
};
