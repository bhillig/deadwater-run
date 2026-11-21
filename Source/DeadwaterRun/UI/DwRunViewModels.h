// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Run/DwRunTypes.h"
#include "UI/DwViewModelBase.h"
#include "DwRunViewModels.generated.h"

class UTexture2D;

class UDwRun;

/*
 * View models of the run screens (Docs/Architecture/14-UI.md "Screens", DW-211): map, rewards, level-up, shop and the
 * event shell. Each copies what UDwRun says and sends intents to UDwRun (ChooseNode, ClaimRewards, ChoosePerk,
 * TryPurchase, LeaveShop, ChooseEventOption, LeaveEvent) - nothing else. Disabled reasons repeat the checks UDwRun makes, so a disabled button never
 * gets a refusal and an enabled one never does either. They refresh on UDwRun::OnStateChanged.
 */

/** One clickable card or button on a run screen (an ability offer, a crew member, a perk, Take, Leave, ...). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwChoiceView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Choice") FText Title;
	UPROPERTY(BlueprintReadOnly, Category = "Choice") FText Body;
	/** What it is at a glance: the move, item, perk or upgrade icon, or the crew member's / captain's portrait. */
	UPROPERTY(BlueprintReadOnly, Category = "Choice") TSoftObjectPtr<UTexture2D> Icon;
	/** The icon is a flat white glyph (the perks' pack status icons) the widget tints, not a coloured render. */
	UPROPERTY(BlueprintReadOnly, Category = "Choice") bool bIconIsGlyph = false;
	UPROPERTY(BlueprintReadOnly, Category = "Choice") bool bEnabled = true;
	UPROPERTY(BlueprintReadOnly, Category = "Choice") bool bSelected = false;
	/** Why it can't be clicked; empty when enabled. */
	UPROPERTY(BlueprintReadOnly, Category = "Choice") FText DisabledReason;
	/**
	 * Set for choices that can't be undone (disband): WBP_ChoiceButton asks first with WBP_Confirm and only then calls
	 * Select. Empty = select on click. The view model's Select itself never asks (tests and dev commands call it).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Choice") FText ConfirmTitle;
	UPROPERTY(BlueprintReadOnly, Category = "Choice") FText ConfirmBody;
	UPROPERTY(BlueprintReadOnly, Category = "Choice") FText ConfirmYes;
	/** The UI sound of an accepted click (UI.Sound.Purchase for purchases); unset = UI.Sound.Click. */
	UPROPERTY(BlueprintReadOnly, Category = "Choice") FGameplayTag SelectSound;
};

/** A choice as its own small view model, so a WBP_ChoiceButton binds it like any other widget. */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwChoiceViewModel : public UDwViewModelBase
{
	GENERATED_BODY()

public:
	const FDwChoiceView& GetView() const { return View; }
	/** Sends the choice's intent. False (nothing changes) when disabled. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|UI") bool Select();

	void Set(const FDwChoiceView& InView, TFunction<bool()> InOnSelect);

private:
	FDwChoiceView View;
	TFunction<bool()> OnSelect;
};

/** Base of the run screen view models: binds a UDwRun, refreshes on OnStateChanged. */
UCLASS(Abstract)
class DEADWATERRUN_API UDwRunScreenViewModel : public UDwViewModelBase
{
	GENERATED_BODY()

public:
	void Bind(UDwRun* InRun);
	UDwRun* GetRun() const { return Run.Get(); }
	virtual void BeginDestroy() override;

protected:
	/** Copy the run into the view, then NotifyChanged. */
	virtual void Refresh() {}
	/** A choice view model at Index of Pool, created on demand (stable objects, so widgets keep their bindings). */
	UDwChoiceViewModel* Choice(TArray<TObjectPtr<UDwChoiceViewModel>>& Pool, int32 Index);

private:
	UFUNCTION() void HandleRunChanged();
	void Unbind();
	TWeakObjectPtr<UDwRun> Run;
};

// ---------------------------------------------------------------------------------------------------------------------
// Map

UENUM(BlueprintType)
enum class EDwMapNodeState : uint8
{
	/** Ahead, not reachable from here yet. */
	Ahead,
	/** One of the routes the player can take now. */
	Choosable,
	/** Where the run is. */
	Current,
	/** Sailed through. */
	Sailed,
	/** Left behind: on a layer already passed, never visited. */
	Missed
};

USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwMapNodeView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Map") int32 Index = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Map") int32 Layer = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Map") int32 Lane = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Map") EDwNodeType Type = EDwNodeType::Fight;
	UPROPERTY(BlueprintReadOnly, Category = "Map") EDwMapNodeState State = EDwMapNodeState::Ahead;
	/** "Fight", "Shop", "?" (M4: events don't show good or bad), "Kraken". Fights show no crew (M6). */
	UPROPERTY(BlueprintReadOnly, Category = "Map") FText Label;
	UPROPERTY(BlueprintReadOnly, Category = "Map") FText Tooltip;
	UPROPERTY(BlueprintReadOnly, Category = "Map") TArray<int32> Next;
};

USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwCrewLineView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crew") FText Name;
	/** "L2  80 / 110 HP" or "lost". */
	UPROPERTY(BlueprintReadOnly, Category = "Crew") FText Detail;
	UPROPERTY(BlueprintReadOnly, Category = "Crew") bool bAlive = true;
	UPROPERTY(BlueprintReadOnly, Category = "Crew") bool bIsCaptain = false;
};

/** The sea chart (phase Map) and the run header every run screen shows: gold, hull, crew. */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwMapViewModel : public UDwRunScreenViewModel
{
	GENERATED_BODY()

public:
	const TArray<FDwMapNodeView>& GetNodes() const { return Nodes; }
	const FDwMapNodeView* FindNode(int32 Index) const;
	int32 GetLayerCount() const { return LayerCount; }
	int32 GetLaneCount() const { return LaneCount; }
	int32 GetGold() const { return Gold; }
	FText GetShipText() const { return ShipText; }
	FText GetTitle() const { return Title; }
	const TArray<FDwCrewLineView>& GetCrew() const { return Crew; }

	/** Only Choosable nodes; otherwise false with the reason (and UDwRun is never called). */
	bool CanChoose(int32 NodeIndex, FText* OutReason = nullptr) const;
	/** Intent: UDwRun::ChooseNode. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|UI") bool ChooseNode(int32 NodeIndex);

protected:
	virtual void Refresh() override;

private:
	TArray<FDwMapNodeView> Nodes;
	TArray<FDwCrewLineView> Crew;
	int32 LayerCount = 0;
	int32 LaneCount = 0;
	int32 Gold = 0;
	FText ShipText;
	FText Title;
};

// ---------------------------------------------------------------------------------------------------------------------
// Rewards

/** Phase Rewards: what the fight paid and "learn a new move" (P5) with who learns it and, if full, what they forget (P4). */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwRewardsViewModel : public UDwRunScreenViewModel
{
	GENERATED_BODY()

public:
	FText GetHeadline() const { return Headline; }
	/** "+225 gold", "+ Sea Salts", "Swordsman +120 XP - level up!" ... */
	const TArray<FText>& GetLines() const { return Lines; }
	/** Ability offers, then (once one is picked) who learns it, then (if their slots are full) what they forget. */
	const TArray<TObjectPtr<UDwChoiceViewModel>>& GetOffers() const { return Offers; }
	const TArray<TObjectPtr<UDwChoiceViewModel>>& GetLearners() const { return Learners; }
	const TArray<TObjectPtr<UDwChoiceViewModel>>& GetForget() const { return Forget; }
	/** "Take the rewards" with its disabled reason while a pick is half made. */
	UDwChoiceViewModel* GetClaim() const { return Claim; }
	FText GetNoOffersText() const { return NoOffersText; }

	/** Selection state (intents to this view model; the run only hears about it on Claim). */
	bool SelectOffer(int32 Index);
	bool SelectLearner(int32 Index);
	bool SelectForget(int32 Index);
	/** Intent: UDwRun::ClaimRewards with the current selection. */
	bool ClaimRewards();
	/** Why Take is disabled now; empty when it isn't. */
	FText GetClaimBlocker() const;

protected:
	virtual void Refresh() override;

private:
	int32 MaxSlots() const;
	const FDwCrewMemberState* SelectedLearner() const;

	FText Headline;
	FText NoOffersText;
	TArray<FText> Lines;
	int32 SelectedOffer = INDEX_NONE;
	FGuid SelectedCrew;
	int32 SelectedSlot = INDEX_NONE;
	/** The bundle these selections belong to (a new fight resets them). */
	int32 BundleNode = INDEX_NONE;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwChoiceViewModel>> Offers;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwChoiceViewModel>> Learners;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwChoiceViewModel>> Forget;
	UPROPERTY(Transient) TObjectPtr<UDwChoiceViewModel> Claim;
};

// ---------------------------------------------------------------------------------------------------------------------
// Level-up

/** Phase LevelUp: the front of PendingLevelUps - who, the new level, the stat gains, and the perk cards. */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwLevelUpViewModel : public UDwRunScreenViewModel
{
	GENERATED_BODY()

public:
	FText GetHeadline() const { return Headline; }
	/** "+10 max HP", ... */
	FText GetStatText() const { return StatText; }
	/** "2 more level-ups after this one" or empty. */
	FText GetQueueText() const { return QueueText; }
	/** One per perk choice; a single "Continue" when the pool had nothing left to offer. */
	const TArray<TObjectPtr<UDwChoiceViewModel>>& GetCards() const { return Cards; }

protected:
	virtual void Refresh() override;

private:
	FText Headline;
	FText StatText;
	FText QueueText;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwChoiceViewModel>> Cards;
};

// ---------------------------------------------------------------------------------------------------------------------
// Shop

/**
 * WBP_Shop (phase Shop; Docs/Architecture/10-Economy-Shop-Loot.md "The shop screen's read model"): one card per
 * FDwShopOffer, grouped as the screen shows them. Every card's enabled state and reason come from UDwRun::CanPurchase,
 * so an enabled card is always a purchase the run accepts.
 * - Supplies (repair, items), recruits and upgrades buy on click; disbands ask first (FDwChoiceView::ConfirmTitle).
 * - A move (ability offer) is picked first, then who learns it (picked for you when only one can), then, if their slots
 *   are full, the move they forget; "Buy" stays disabled with the reason until the pick is complete. A second click on
 *   the picked move drops the pick.
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwShopViewModel : public UDwRunScreenViewModel
{
	GENERATED_BODY()

public:
	int32 GetGold() const { return Gold; }
	FText GetShipText() const { return ShipText; }
	/** "Hold 3 / 6" (P10), or "Hold 3" with no cap. */
	FText GetHoldText() const { return HoldText; }
	const TArray<FDwCrewLineView>& GetCrew() const { return Crew; }

	/** Repair, then items. */
	const TArray<TObjectPtr<UDwChoiceViewModel>>& GetSupplies() const { return Supplies; }
	/** Ability offers. */
	const TArray<TObjectPtr<UDwChoiceViewModel>>& GetMoves() const { return Moves; }
	/** Once a move is picked: who can learn it. */
	const TArray<TObjectPtr<UDwChoiceViewModel>>& GetLearners() const { return Learners; }
	/** Once a full learner is picked: the move to forget (P4). */
	const TArray<TObjectPtr<UDwChoiceViewModel>>& GetForget() const { return Forget; }
	/** "Buy <move> for <learner>", or null while no move is picked. */
	UDwChoiceViewModel* GetBuyMove() const { return bMovePicked ? BuyMove.Get() : nullptr; }
	/** The next step of the move pick, or what the moves are for when none is picked. */
	FText GetMoveHint() const { return MoveHint; }
	const TArray<TObjectPtr<UDwChoiceViewModel>>& GetRecruits() const { return Recruits; }
	const TArray<TObjectPtr<UDwChoiceViewModel>>& GetUpgrades() const { return Upgrades; }
	const TArray<TObjectPtr<UDwChoiceViewModel>>& GetDisbands() const { return Disbands; }
	UDwChoiceViewModel* GetLeave() const { return Leave; }

	/** The latest purchase attempt and how many there were this visit (the screen plays OnPurchase when it moves). */
	EDwPurchaseResult GetLastResult() const { return LastResult; }
	int32 GetPurchaseCount() const { return PurchaseCount; }

	/** Intents. Buy: any offer but a move (TryPurchase). Moves go through SelectMove / SelectLearner / SelectForget / BuySelectedMove. */
	bool Buy(int32 OfferId);
	bool SelectMove(int32 OfferId);
	bool SelectLearner(int32 Index);
	bool SelectForget(int32 Index);
	bool BuySelectedMove();

	/** The disabled reason for a refused purchase (empty for Ok). Crew = the learner of a move. */
	static FText DescribeResult(EDwPurchaseResult Result, const UDwRun& Run, const FDwShopOffer* Offer, const FGuid& Crew);

protected:
	virtual void Refresh() override;

private:
	const FDwShopOffer* FindOffer(int32 OfferId) const;
	/** The move's purchase as picked so far: Ok, or the first thing missing. */
	EDwPurchaseResult CheckMove(FText& OutReason) const;
	/** Enabled state of a move card: Ok if some eligible learner (with any slot to forget) can buy it, else the first refusal. */
	EDwPurchaseResult CheckMoveCard(const FDwShopOffer& Offer, FGuid& OutCrew) const;
	bool Purchase(int32 OfferId, const FGuid& Crew, int32 Slot);
	void ClearPick();

	int32 Gold = 0;
	FText ShipText;
	FText HoldText;
	FText MoveHint;
	TArray<FDwCrewLineView> Crew;
	int32 SelectedMove = INDEX_NONE;
	FGuid SelectedCrew;
	int32 SelectedSlot = INDEX_NONE;
	bool bMovePicked = false;
	/** The shop these selections belong to (a new shop resets them). */
	int32 ShopNode = INDEX_NONE;
	EDwPurchaseResult LastResult = EDwPurchaseResult::Ok;
	int32 PurchaseCount = 0;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwChoiceViewModel>> Supplies;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwChoiceViewModel>> Moves;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwChoiceViewModel>> Learners;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwChoiceViewModel>> Forget;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwChoiceViewModel>> Recruits;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwChoiceViewModel>> Upgrades;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwChoiceViewModel>> Disbands;
	UPROPERTY(Transient) TObjectPtr<UDwChoiceViewModel> BuyMove;
	UPROPERTY(Transient) TObjectPtr<UDwChoiceViewModel> Leave;
};

// ---------------------------------------------------------------------------------------------------------------------
// Event shell

/**
 * WBP_NodeShell: phase Event (DW-205):
 * 1. The event's title and body, one button per choice ("you will ..." line, disabled with the failing requirement).
 * 2. A choice that lands on a crew member the player picks: one button per crew member who can be picked, and Back.
 * 3. After the choice: the result text and every change, and Continue (UDwRun::LeaveEvent).
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwNodeShellViewModel : public UDwRunScreenViewModel
{
	GENERATED_BODY()

public:
	FText GetHeadline() const { return Headline; }
	FText GetBody() const { return Body; }
	/** The buttons in order: the event's choices, crew to pick, or the one way out (Leave / Continue). */
	const TArray<TObjectPtr<UDwChoiceViewModel>>& GetActions() const { return Actions; }

	/** Intent: the event choice at Index. Choices that need a crew member open step 2 instead of resolving. */
	bool SelectEventChoice(int32 Index);
	/** Step 2: back to the choices. */
	bool CancelCrewPick();

protected:
	virtual void Refresh() override;

private:
	void RefreshEvent(const UDwRun& TheRun, int32& Count);

	FText Headline;
	FText Body;
	/** Step 2: the choice waiting for its crew member, INDEX_NONE otherwise. Reset when a new event opens. */
	int32 PendingChoice = INDEX_NONE;
	int32 PendingNode = INDEX_NONE;
	UPROPERTY(Transient) TArray<TObjectPtr<UDwChoiceViewModel>> Actions;
};
