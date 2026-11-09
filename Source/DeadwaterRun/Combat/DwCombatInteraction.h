// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Abilities/DwTargeting.h"
#include "Combat/DwCombatSnapshot.h"
#include "Combat/DwCombatTypes.h"
#include "DwCombatInteraction.generated.h"

class ADwBoard;
class UDwAbilityDefinition;
class UDwCombatSubsystem;

UENUM(BlueprintType)
enum class EDwInteractionMode : uint8
{
	/** Nothing selected, or not the player's turn. */
	Idle,
	/** A unit is selected: reachable tiles shown, hover shows the path, a click moves (or attacks an enemy in reach). */
	Move,
	/** An ability is armed: valid targets shown, hover previews, a click confirms. */
	Targeting
};

/** One slot of the selected unit's action bar (keys 1-6). Items expand to one slot per item kind in stock. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwActionSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Action") TObjectPtr<const UDwAbilityDefinition> Ability;
	UPROPERTY(BlueprintReadOnly, Category = "Action") FName ItemId;
	UPROPERTY(BlueprintReadOnly, Category = "Action") FText Label;
	UPROPERTY(BlueprintReadOnly, Category = "Action") int32 Count = 0;
	/** Usable now with at least one valid target. */
	UPROPERTY(BlueprintReadOnly, Category = "Action") bool bAvailable = false;
	/** Why not ("Not enough MP", "No action left this turn", "Stand on or next to one of your loaded cannons"). */
	UPROPERTY(BlueprintReadOnly, Category = "Action") FText Reason;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDwOnInteractionChanged);

/**
 * The player's combat interaction state (Docs/Architecture/03 "Player input"): selected unit, armed ability, hovered tile,
 * previews and board highlights. Holds no rules - every check is FDwCombatQueries or an ability CDO query on the live
 * snapshot, and every action is a command to UDwCombatSubsystem, so the UI can never do what the rules forbid.
 * No undo (C13): a confirmed command is final; damage is previewed on hover before the click.
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwCombatInteraction : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UDwCombatSubsystem& InCombat);
	void Shutdown();

	/** The player's side turn began: unlock and select the first unit that can act. */
	void BeginPlayerTurn();

	UFUNCTION(BlueprintCallable, Category = "Deadwater|Interaction") void SelectUnit(FDwUnitHandle Unit);
	/** Arms an ability of the selected unit. False (with LastMessage) if it can't be used now. */
	bool SelectAbility(const UDwAbilityDefinition* Ability, FName ItemId = NAME_None);
	/** Arms action bar slot Index (0-based, keys 1-6). */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Interaction") bool SelectSlot(int32 Index);
	void HoverTile(TOptional<FIntPoint> Tile);
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Interaction") void ClickTile(FIntPoint Tile);
	/** Disarms the ability, else deselects. False if there was nothing to cancel. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Interaction") bool Cancel();
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Interaction") void SelectNextUnit();
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Interaction") void RequestEndTurn();

	/** Recomputes highlights and previews from the live fight (after anything resolves). */
	void Refresh();

	EDwInteractionMode GetMode() const { return Mode; }
	FDwUnitHandle GetSelectedUnit() const { return SelectedUnit; }
	const UDwAbilityDefinition* GetArmedAbility() const { return ArmedAbility; }
	FName GetArmedItem() const { return ArmedItem; }
	TOptional<FIntPoint> GetHoveredTile() const { return HoveredTile; }
	/** Preview of the hovered target while targeting (or of the basic attack on a hovered enemy while moving). */
	const FDwAbilityPreview& GetPreview() const { return Preview; }
	const TArray<FIntPoint>& GetPath() const { return Path; }
	const TArray<FIntPoint>& GetReachable() const { return Reachable; }
	const TArray<FDwAbilityTarget>& GetValidTargets() const { return ValidTargets; }
	/** Last rejection or hint, for the HUD. */
	const FText& GetMessage() const { return Message; }
	bool IsEndTurnSuggested() const { return bEndTurnSuggested; }
	/** Player turn, nothing resolving, no outcome. */
	bool CanAct() const;
	TArray<FDwActionSlot> GetActionBar() const;

	UPROPERTY(BlueprintAssignable, Category = "Deadwater|Interaction") FDwOnInteractionChanged OnChanged;

private:
	UFUNCTION() void HandleCommandResolved(const FDwCombatCommand& Command);
	UFUNCTION() void HandleCombatEnded(const FDwCombatResult& Result);
	UFUNCTION() void HandleEndTurnSuggested();

	EDwCommandResult Submit(const FDwCombatCommand& Command);
	/** The target a click on Tile means for the armed ability (unit on it, the ship it belongs to, or the tile). */
	TOptional<FDwAbilityTarget> TargetAt(const FDwCombatSnapshot& Snapshot, FIntPoint Tile, const UDwAbilityDefinition* Ability,
		const TArray<FDwAbilityTarget>& Valid) const;
	FDwTargetingContext MakeContext(const FDwCombatSnapshot& Snapshot, const UDwAbilityDefinition* Ability, FName ItemId) const;
	void Disarm();
	void UpdateHover(const FDwCombatSnapshot& Snapshot);
	void DrawHighlights(const FDwCombatSnapshot& Snapshot);
	void ClearHighlights();
	ADwBoard* GetBoard() const;

	UPROPERTY() TObjectPtr<UDwCombatSubsystem> Combat;
	UPROPERTY() TObjectPtr<const UDwAbilityDefinition> ArmedAbility;

	EDwInteractionMode Mode = EDwInteractionMode::Idle;
	FDwUnitHandle SelectedUnit;
	FName ArmedItem;
	TOptional<FIntPoint> HoveredTile;
	TArray<FIntPoint> Reachable;
	TArray<FIntPoint> Path;
	TArray<FDwAbilityTarget> ValidTargets;
	FDwAbilityPreview Preview;
	FText Message;
	bool bEndTurnSuggested = false;
};
