// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "DwUIManagerSubsystem.generated.h"

class UCommonActivatableWidget;
class UDwActivatableScreen;
class UDwCombatHudViewModel;
class UDwCombatSubsystem;
class UDwFloatingTextManager;
class UDwPrimaryLayoutWidget;
class UDwUIScreenSet;
class UDwUISoundMap;
struct FDwCombatResult;

/**
 * Owns the local player's UI (Docs/Architecture/14-UI.md): creates WBP_PrimaryLayout when a player controller arrives,
 * pushes screens onto UI.Layer.* stacks, and owns the long-lived view models.
 * Screen selection: doc 14 pushes the screen DA_UIScreenSet maps to UDwRun::OnPhaseChanged. UDwRun is DW-201, so until
 * then the Combat phase is entered/left on UDwCombatSubsystem::OnCombatStarted (the only fight source in L_Sea today);
 * DW-201 replaces HandleCombatStarted with HandleRunPhaseChanged and the rest stays.
 */
UCLASS()
class DEADWATERRUN_API UDwUIManagerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static UDwUIManagerSubsystem* Get(const ULocalPlayer* LocalPlayer);

	/** Pushes a screen onto a UI.Layer.* stack. Null if there is no layout yet or the layer is unknown. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|UI", meta = (GameplayTagFilter = "UI.Layer"))
	UDwActivatableScreen* PushScreen(FGameplayTag LayerTag, TSubclassOf<UDwActivatableScreen> ScreenClass);

	/** Removes a screen from whichever stack holds it. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|UI")
	void PopScreen(UCommonActivatableWidget* Screen);

	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") UDwPrimaryLayoutWidget* GetLayout() const { return Layout; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") UDwCombatHudViewModel* GetCombatHudViewModel() const { return CombatHud; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") UDwActivatableScreen* GetCombatHudScreen() const { return CombatHudScreen; }
	UDwFloatingTextManager* GetFloatingText() const { return FloatingText; }
	const UDwUIScreenSet* GetScreenSet() const;
	const UDwUISoundMap* GetSoundMap() const;

	// ULocalPlayerSubsystem
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;
	virtual void Deinitialize() override;

private:
	UFUNCTION() void HandleCombatStarted();

	void CreateLayout(APlayerController& PlayerController);
	void DestroyLayout();
	void BindCombat(UWorld* World);
	void UnbindCombat();
	void ShowCombatHud();

	UPROPERTY(Transient) TObjectPtr<UDwPrimaryLayoutWidget> Layout;
	UPROPERTY(Transient) TObjectPtr<UDwCombatHudViewModel> CombatHud;
	UPROPERTY(Transient) TObjectPtr<UDwActivatableScreen> CombatHudScreen;
	UPROPERTY(Transient) TObjectPtr<UDwFloatingTextManager> FloatingText;
	TWeakObjectPtr<UDwCombatSubsystem> Combat;
	TWeakObjectPtr<APlayerController> PlayerController;
};
