// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DwViewModelBase.generated.h"

class UDwViewModelBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDwOnViewModelChanged, UDwViewModelBase*, ViewModel);

/**
 * Base of every UI view model (Docs/Architecture/14-UI.md D2): a plain C++ object that subscribes to game delegates,
 * exposes getters and broadcasts OnChanged. Widgets never read game state; they read a view model and refresh in
 * NativeOnViewModelChanged. View models hold no rules: they copy what the game says and forward player intents to
 * UDwCombatInteraction (or UDwRun), nothing else. Testable without any widget.
 */
UCLASS(Abstract, BlueprintType)
class DEADWATERRUN_API UDwViewModelBase : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Deadwater|UI")
	FDwOnViewModelChanged OnChanged;

	/** Times NotifyChanged has broadcast (tests, debugging). */
	int32 GetChangeCount() const { return ChangeCount; }

protected:
	void NotifyChanged();

private:
	int32 ChangeCount = 0;
};
