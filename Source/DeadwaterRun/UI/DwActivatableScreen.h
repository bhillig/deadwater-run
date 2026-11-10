// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "GameplayTagContainer.h"
#include "Input/UIActionBindingHandle.h"
#include "DwActivatableScreen.generated.h"

class UDwViewModelBase;

/**
 * Base of every Deadwater screen (Docs/Architecture/14-UI.md D1): a CommonUI activatable widget pushed onto one of the
 * primary layout's UI.Layer.* stacks by UDwUIManagerSubsystem. Same view model contract as UDwUserWidget.
 * Input: each screen declares how game input behaves while it is on top (menus block the game; the combat HUD lets
 * Enhanced Input through so board clicks, hotkeys and Space keep working).
 * Juice: Anim_Intro / Anim_Outro play on activate / deactivate when present, plus OnJuiceIntro / OnJuiceOutro.
 */
UCLASS(Abstract)
class DEADWATERRUN_API UDwActivatableScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Deadwater|UI")
	void SetViewModel(UDwViewModelBase* InViewModel);

	UFUNCTION(BlueprintPure, Category = "Deadwater|UI")
	UDwViewModelBase* GetViewModel() const { return ViewModel; }

	template <typename T>
	T* GetViewModel() const { return Cast<T>(ViewModel); }

	UFUNCTION(BlueprintCallable, Category = "Deadwater|UI|Juice")
	bool PlayJuice(FName AnimationName);

	UFUNCTION(BlueprintCallable, Category = "Deadwater|UI|Juice", meta = (GameplayTagFilter = "UI.Sound"))
	void PlayUISound(FGameplayTag SoundTag);

	// UCommonActivatableWidget
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	virtual void NativeOnViewModelChanged() {}

	UFUNCTION(BlueprintImplementableEvent, Category = "Deadwater|UI")
	void OnViewModelChanged();

	/** Juice hook: the screen was activated (after Anim_Intro started, if any). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Deadwater|UI|Juice")
	void OnJuiceIntro();

	/** Juice hook: the screen was deactivated (after Anim_Outro started, if any). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Deadwater|UI|Juice")
	void OnJuiceOutro();

	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;

	/** Menu = the game gets no input while this screen is on top; All = game and UI both get it (HUDs). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Input")
	ECommonInputMode InputMode = ECommonInputMode::Menu;

	/**
	 * Menus: NoCapture (the cursor stays free). Screens that let the game through (InputMode All) must use
	 * CaptureDuringMouseDown, or the scene viewport drops mouse buttons before they reach Enhanced Input.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|Input")
	EMouseCaptureMode MouseCaptureMode = EMouseCaptureMode::NoCapture;

private:
	UFUNCTION() void HandleViewModelChanged(UDwViewModelBase* Changed);

	UPROPERTY(Transient) TObjectPtr<UDwViewModelBase> ViewModel;
};
