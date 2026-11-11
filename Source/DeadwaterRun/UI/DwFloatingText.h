// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Combat/DwCombatTypes.h"
#include "UI/DwUserWidget.h"
#include "DwFloatingText.generated.h"

class APlayerController;
class UDwCombatSubsystem;
class UTextBlock;

/** What a combat event shows above its target. */
UENUM(BlueprintType)
enum class EDwFloatingTextKind : uint8
{
	Damage,
	Heal,
	Status,
	Fear
};

USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwFloatingTextRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FloatingText") EDwFloatingTextKind Kind = EDwFloatingTextKind::Damage;
	UPROPERTY(BlueprintReadOnly, Category = "FloatingText") FText Text;
	/** The unit it floats over; invalid for ship events. */
	UPROPERTY(BlueprintReadOnly, Category = "FloatingText") FDwUnitHandle Unit;
	/** Ship events: the ship it floats over. */
	UPROPERTY(BlueprintReadOnly, Category = "FloatingText") EDwSide ShipSide = EDwSide::Neutral;
};

/**
 * WBP_FloatingText: one pooled world-anchored label ("-25", "+30", "Frozen!", "+1 Fear"). Positioned every frame from its
 * world anchor; lives Lifetime seconds, then returns to the pool. Motion and fades are juice (DW-402): Anim_Intro plays
 * on Show when present, plus OnShown(Kind) for Blueprint styling.
 */
UCLASS(Abstract)
class DEADWATERRUN_API UDwFloatingTextWidget : public UDwUserWidget
{
	GENERATED_BODY()

public:
	/** StackIndex > 0 lifts the label above others already showing over the same target (multi-hit attacks). */
	void Show(const FDwFloatingTextRequest& Request, const FVector& WorldAnchor, int32 StackIndex = 0);
	void Hide();
	bool IsShowing() const { return bShowing; }
	const FDwFloatingTextRequest& GetRequest() const { return Current; }

	/** Seconds on screen. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|FloatingText", meta = (ClampMin = "0.1"))
	float Lifetime = 1.2f;

	/** Placeholder colours per kind (art direction pending, U6). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadwater|FloatingText")
	TMap<EDwFloatingTextKind, FLinearColor> Colors;

protected:
	UDwFloatingTextWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Deadwater|UI|Juice")
	void OnShown(EDwFloatingTextKind Kind);

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ValueText;

private:
	void UpdatePosition();

	FDwFloatingTextRequest Current;
	FVector Anchor = FVector::ZeroVector;
	int32 Stack = 0;
	float Age = 0.f;
	bool bShowing = false;
};

/**
 * UDwFloatingTextManager (Docs/Architecture/14-UI.md): turns FDwCombatEvents into pooled floating text over the unit or
 * ship concerned. Bounded pool (32): when every label is in use, the oldest is recycled. Owned by UDwUIManagerSubsystem.
 */
UCLASS()
class DEADWATERRUN_API UDwFloatingTextManager : public UObject
{
	GENERATED_BODY()

public:
	static constexpr int32 PoolSize = 32;

	/** Start listening to a fight. WidgetClass null = describe only (tests, no viewport). */
	void Initialize(UDwCombatSubsystem& InCombat, APlayerController* InOwner, TSubclassOf<UDwFloatingTextWidget> InWidgetClass);
	void Shutdown();

	/** The text an event shows, or false if it shows none (pure: tests and the combat log can reuse it). */
	static bool Describe(const FDwCombatEvent& Event, FDwFloatingTextRequest& OutRequest);

	/** Labels created so far (never more than PoolSize). */
	int32 GetPoolCount() const { return Pool.Num(); }
	int32 GetShowingCount() const;
	/** Requests shown so far (tests). */
	int32 GetShownTotal() const { return ShownTotal; }

private:
	UFUNCTION() void HandleCombatEvent(const FDwCombatEvent& Event);

	UDwFloatingTextWidget* Acquire();
	TOptional<FVector> AnchorFor(const FDwFloatingTextRequest& Request) const;

	TWeakObjectPtr<UDwCombatSubsystem> Combat;
	TWeakObjectPtr<APlayerController> Owner;
	/** Reflected: the class is soft-loaded per fight and nothing else holds it until the first label exists (GC). */
	UPROPERTY(Transient) TSubclassOf<UDwFloatingTextWidget> WidgetClass;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwFloatingTextWidget>> Pool;
	int32 NextRecycle = 0;
	int32 ShownTotal = 0;
};
