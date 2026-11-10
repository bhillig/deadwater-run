// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Combat/DwCombatTypes.h"
#include "Core/DwTypes.h"
#include "UI/DwViewModelBase.h"
#include "DwCombatViewModels.generated.h"

class UDwAbilityDefinition;
class UDwCombatInteraction;
class UDwCombatSubsystem;
class UInputAction;
class UInputMappingContext;
class UTexture2D;
struct FDwCombatSnapshot;
struct FDwInputConfig;
struct FDwUnitSnapshot;

/*
 * Combat HUD view models (Docs/Architecture/14-UI.md "Classes"). They read UDwCombatSubsystem (snapshot, events),
 * UDwCombatInteraction (selection, previews, action bar) and the units' ASCs (status turns left), and send intents only
 * through UDwCombatInteraction. No rule is evaluated here: availability and disabled reasons are the interaction's
 * action bar, which runs the same FDwCombatQueries checks SubmitCommand validates with.
 */

/** A status on a unit, with the turns it has left (INDEX_NONE = no turn duration). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwStatusView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Status") FGameplayTag Tag;
	UPROPERTY(BlueprintReadOnly, Category = "Status") FText Label;
	UPROPERTY(BlueprintReadOnly, Category = "Status") int32 TurnsRemaining = INDEX_NONE;

	bool operator==(const FDwStatusView& Other) const
	{
		return Tag == Other.Tag && TurnsRemaining == Other.TurnsRemaining && Label.EqualTo(Other.Label);
	}
};

/** Everything a crew card or enemy bar shows. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwUnitView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Unit") FDwUnitHandle Handle;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") EDwSide Side = EDwSide::Player;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") FText Name;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") bool bIsCaptain = false;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") bool bAlive = true;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float Health = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float MaxHealth = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float Mana = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float MaxMana = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float Fear = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float MaxFear = 0.f;
	/** Move still available this turn (MovePoints > 0). */
	UPROPERTY(BlueprintReadOnly, Category = "Turn") bool bHasMove = false;
	/** Action still available this turn (ActionPoints > 0). */
	UPROPERTY(BlueprintReadOnly, Category = "Turn") bool bHasAction = false;
	/** The player's selected pirate. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction") bool bSelected = false;
	/** Under the cursor on the board, or hit by the previewed action. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction") bool bHovered = false;
	/** The enemy whose turn it is in the fixed sequence (C14). */
	UPROPERTY(BlueprintReadOnly, Category = "Turn") bool bActing = false;
	UPROPERTY(BlueprintReadOnly, Category = "Status") TArray<FDwStatusView> Statuses;

	bool operator==(const FDwUnitView& Other) const;
};

UCLASS(BlueprintType)
class DEADWATERRUN_API UDwUnitViewModel : public UDwViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") const FDwUnitView& GetView() const { return View; }

	/** Intent: select this pirate (crew only; the interaction ignores anything else). */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|UI") void Select();

	/** Broadcasts OnChanged only when something the widget shows changed. */
	void Apply(const FDwUnitView& NewView);
	void SetInteraction(UDwCombatInteraction* InInteraction);

private:
	UPROPERTY(Transient) FDwUnitView View;
	TWeakObjectPtr<UDwCombatInteraction> Interaction;
};

/** One action bar slot (Docs/Architecture/14 "UDwActionViewModel"). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwActionView
{
	GENERATED_BODY()

	/** 0-based action bar index (keys 1-6). */
	UPROPERTY(BlueprintReadOnly, Category = "Action") int32 SlotIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Action") FGameplayTag AbilityTag;
	UPROPERTY(BlueprintReadOnly, Category = "Action") FName ItemId;
	UPROPERTY(BlueprintReadOnly, Category = "Action") FText Name;
	/** Definition description with {Damage} {Heal} {Fear} {Range} {ManaCost} {Turns} {Amount} filled from tuning. */
	UPROPERTY(BlueprintReadOnly, Category = "Action") FText Description;
	UPROPERTY(BlueprintReadOnly, Category = "Action") TSoftObjectPtr<UTexture2D> Icon;
	UPROPERTY(BlueprintReadOnly, Category = "Action") int32 ManaCost = 0;
	/** "2 MP", "Free", or empty for a plain 1-action ability. */
	UPROPERTY(BlueprintReadOnly, Category = "Action") FText CostText;
	/** Items left (items only). */
	UPROPERTY(BlueprintReadOnly, Category = "Action") int32 Count = 0;
	/** Usable now: the rules allow it, it has a target, and the player may act (their turn, nothing resolving). */
	UPROPERTY(BlueprintReadOnly, Category = "Action") bool bEnabled = false;
	/** Why the rules refuse it (the same text SubmitCommand would give). Empty when the rules allow it. */
	UPROPERTY(BlueprintReadOnly, Category = "Action") FText DisabledReason;
	/** The key bound to this slot in IMC_Combat ("1".."6"). */
	UPROPERTY(BlueprintReadOnly, Category = "Action") FText Hotkey;
	/** Armed: the player is choosing its target. */
	UPROPERTY(BlueprintReadOnly, Category = "Action") bool bArmed = false;

	bool operator==(const FDwActionView& Other) const;
};

UCLASS(BlueprintType)
class DEADWATERRUN_API UDwActionViewModel : public UDwViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") const FDwActionView& GetView() const { return View; }

	/** Intent: arm this slot (same as pressing its hotkey). False with a message on the interaction if refused. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|UI") bool Select();

	void Apply(const FDwActionView& NewView);
	void SetInteraction(UDwCombatInteraction* InInteraction);

	/** Fills {Damage} {Heal} {Fear} {Range} {ManaCost} {Turns} {Amount} tokens of an ability or item description. */
	static FText FormatDescription(const UDwAbilityDefinition* Ability, FName ItemId, const FDwCombatSnapshot& Snapshot);

private:
	UPROPERTY(Transient) FDwActionView View;
	TWeakObjectPtr<UDwCombatInteraction> Interaction;
};

/** A ship health bar. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwShipView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ship") bool bPresent = false;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") EDwSide Side = EDwSide::Player;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") float Health = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") float MaxHealth = 0.f;
};

/** One line of the damage preview shown before a confirming click (C13). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwPreviewLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Preview") FText Target;
	/** "-25 HP", "-30 to -50 HP", "+30 HP", "+1 Fear"... */
	UPROPERTY(BlueprintReadOnly, Category = "Preview") FText Effect;
	UPROPERTY(BlueprintReadOnly, Category = "Preview") bool bLethal = false;
};

/** The key labels the HUD shows, read from IMC_Combat so they follow the real bindings. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwHudKeys
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Keys") TArray<FText> Abilities;
	UPROPERTY(BlueprintReadOnly, Category = "Keys") FText EndTurn;
	UPROPERTY(BlueprintReadOnly, Category = "Keys") FText Select;
	UPROPERTY(BlueprintReadOnly, Category = "Keys") FText Cancel;
	UPROPERTY(BlueprintReadOnly, Category = "Keys") FText NextUnit;

	/** The first key mapped to Action in Context, as display text. Empty if unmapped. */
	static FText KeyFor(const UInputMappingContext* Context, const UInputAction* Action);
	static FDwHudKeys FromInput(const FDwInputConfig& Input);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDwOnTurnBanner, EDwSide, Side, int32, Round);

/**
 * The combat HUD (WBP_CombatHud). Owned by UDwUIManagerSubsystem for the length of a fight.
 * Crew / Enemies / Actions keep their view model objects across refreshes (same handle or slot = same object), so
 * widgets bind once and juice can react to value changes.
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwCombatHudViewModel : public UDwViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(UDwCombatSubsystem& InCombat, UDwCombatInteraction* InInteraction, const FDwHudKeys& InKeys);
	void Shutdown();

	/** Rebuilds everything from the live fight. Called on every game event; also callable directly (tests). */
	void Refresh();

	/** Crew in roster order, enemies in acting order (C14), actions in action bar order. */
	const TArray<TObjectPtr<UDwUnitViewModel>>& GetCrew() const { return Crew; }
	const TArray<TObjectPtr<UDwUnitViewModel>>& GetEnemies() const { return Enemies; }
	const TArray<TObjectPtr<UDwActionViewModel>>& GetActions() const { return Actions; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") const FDwShipView& GetPlayerShip() const { return PlayerShip; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") const FDwShipView& GetEnemyShip() const { return EnemyShip; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") int32 GetRound() const { return Round; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") EDwSide GetActiveSide() const { return ActiveSide; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") bool IsPlayerTurn() const { return ActiveSide == EDwSide::Player; }
	/** The player may act right now (their turn, nothing resolving, no outcome). */
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") bool CanAct() const { return bCanAct; }
	/** "Round 2 - YOUR TURN". */
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") const FText& GetTurnText() const { return TurnText; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") EDwCombatOutcome GetOutcome() const { return Outcome; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") const FText& GetOutcomeText() const { return OutcomeText; }
	/** C14: no player unit can do anything; the End Turn button shows a non-modal "End turn?" prompt. */
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") bool IsEndTurnSuggested() const { return bEndTurnSuggested; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") const FDwHudKeys& GetKeys() const { return Keys; }
	/** What to do next ("Click one of your pirates.", "Aiming Musket - ..."). */
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") const FText& GetHint() const { return Hint; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") const TArray<FDwPreviewLine>& GetPreview() const { return Preview; }
	/** The interaction's last rejection or hint (red). */
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") const FText& GetMessage() const { return Message; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|UI") UDwUnitViewModel* FindUnit(FDwUnitHandle Handle) const;

	// Intents (all through UDwCombatInteraction)
	UFUNCTION(BlueprintCallable, Category = "Deadwater|UI") void SelectUnit(FDwUnitHandle Handle);
	UFUNCTION(BlueprintCallable, Category = "Deadwater|UI") bool SelectAction(int32 SlotIndex);
	UFUNCTION(BlueprintCallable, Category = "Deadwater|UI") void EndTurn();
	UFUNCTION(BlueprintCallable, Category = "Deadwater|UI") bool Cancel();

	/** A side's turn began (TurnStarted): show the turn banner. Juice: OnTurnBannerShown(Side). */
	UPROPERTY(BlueprintAssignable, Category = "Deadwater|UI") FDwOnTurnBanner OnTurnBanner;

private:
	UFUNCTION() void HandleCombatEvent(const FDwCombatEvent& Event);
	UFUNCTION() void HandleCommandResolved(const FDwCombatCommand& Command);
	UFUNCTION() void HandleCombatEnded(const FDwCombatResult& Result);
	UFUNCTION() void HandleEndTurnSuggested();
	UFUNCTION() void HandleInteractionChanged();

	void RefreshUnits(const FDwCombatSnapshot& Snapshot);
	void RefreshActions(const FDwCombatSnapshot& Snapshot);
	void RefreshPrompt(const FDwCombatSnapshot& Snapshot);
	TArray<FDwStatusView> GatherStatuses(const FDwUnitSnapshot& Unit) const;
	FText NameOf(const FDwAbilityTarget& Target, const FDwCombatSnapshot& Snapshot) const;

	TWeakObjectPtr<UDwCombatSubsystem> Combat;
	TWeakObjectPtr<UDwCombatInteraction> Interaction;

	UPROPERTY(Transient) TArray<TObjectPtr<UDwUnitViewModel>> Crew;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwUnitViewModel>> Enemies;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwActionViewModel>> Actions;

	FDwShipView PlayerShip;
	FDwShipView EnemyShip;
	FDwHudKeys Keys;
	int32 Round = 0;
	EDwSide ActiveSide = EDwSide::Player;
	bool bCanAct = false;
	bool bEndTurnSuggested = false;
	EDwCombatOutcome Outcome = EDwCombatOutcome::None;
	FText TurnText;
	FText OutcomeText;
	FText Hint;
	FText Message;
	TArray<FDwPreviewLine> Preview;
};
