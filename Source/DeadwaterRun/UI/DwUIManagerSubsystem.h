// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Run/DwRunTypes.h"
#include "DwUIManagerSubsystem.generated.h"

class UCommonActivatableWidget;
class UDwActivatableScreen;
class UDwCombatHudViewModel;
class UDwLevelUpViewModel;
class UDwMapViewModel;
class UDwNodeShellViewModel;
class UDwRewardsViewModel;
class UDwCombatSubsystem;
class UDwRun;
class UDwRunSubsystem;
class UDwFloatingTextManager;
class UDwPrimaryLayoutWidget;
class UDwUIScreenSet;
class UDwUISoundMap;
struct FDwCombatResult;

/**
 * Owns the local player's UI (Docs/Architecture/14-UI.md): creates WBP_PrimaryLayout when a player controller arrives,
 * pushes screens onto UI.Layer.* stacks, and owns the long-lived view models.
 * Screen selection (doc 14): the combat HUD goes up on UDwCombatSubsystem::OnCombatStarted (run fights and the dev
 * fight alike) and comes down when the active UDwRun leaves the Combat phase. Every other run phase gets its screen from
 * DA_UIScreenSet (DW-211: map, rewards, level-up, shop/event shells) with its view model bound to the run; the summary
 * after Victory/Defeat is DW-210.
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
	/** The map, rewards, level-up or shop/event screen on top, if any (DW-211). */
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") UDwActivatableScreen* GetRunScreen() const { return RunScreen; }
	UDwFloatingTextManager* GetFloatingText() const { return FloatingText; }
	const UDwUIScreenSet* GetScreenSet() const;
	const UDwUISoundMap* GetSoundMap() const;

	// ULocalPlayerSubsystem
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;
	virtual void Deinitialize() override;

private:
	UFUNCTION() void HandleCombatStarted();
	UFUNCTION() void HandleRunStarted(UDwRun* Run);
	UFUNCTION() void HandleRunCleared();
	UFUNCTION() void HandleRunPhaseChanged(EDwRunPhase NewPhase, EDwRunPhase OldPhase);

	void CreateLayout(APlayerController& PlayerController);
	void DestroyLayout();
	void BindCombat(UWorld* World);
	void UnbindCombat();
	void BindRuns(UWorld* World);
	void UnbindRuns();
	void BindRun(UDwRun* Run);
	void ShowCombatHud();
	void HideCombatHud();
	/** Replaces the run screen with the one for Phase (none for Combat, Victory, Defeat). */
	void ShowRunScreen(EDwRunPhase Phase);

	UPROPERTY(Transient) TObjectPtr<UDwPrimaryLayoutWidget> Layout;
	UPROPERTY(Transient) TObjectPtr<UDwCombatHudViewModel> CombatHud;
	UPROPERTY(Transient) TObjectPtr<UDwActivatableScreen> CombatHudScreen;
	UPROPERTY(Transient) TObjectPtr<UDwActivatableScreen> RunScreen;
	UPROPERTY(Transient) TObjectPtr<UDwMapViewModel> MapViewModel;
	UPROPERTY(Transient) TObjectPtr<UDwRewardsViewModel> RewardsViewModel;
	UPROPERTY(Transient) TObjectPtr<UDwLevelUpViewModel> LevelUpViewModel;
	UPROPERTY(Transient) TObjectPtr<UDwNodeShellViewModel> ShellViewModel;
	UPROPERTY(Transient) TObjectPtr<UDwFloatingTextManager> FloatingText;
	TWeakObjectPtr<UDwCombatSubsystem> Combat;
	TWeakObjectPtr<UDwRunSubsystem> Runs;
	TWeakObjectPtr<UDwRun> BoundRun;
	TWeakObjectPtr<APlayerController> PlayerController;
};
