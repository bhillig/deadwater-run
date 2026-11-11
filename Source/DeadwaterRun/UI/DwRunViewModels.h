// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Run/DwRunTypes.h"
#include "UI/DwViewModelBase.h"
#include "DwRunViewModels.generated.h"

class UDwRun;

/*
 * View models of the run screens (Docs/Architecture/14-UI.md "Screens", DW-211): map, rewards, level-up, and the shop /
 * event shells. Each copies what UDwRun says and sends intents to UDwRun (ChooseNode, ClaimRewards, ChoosePerk,
 * LeaveShop, LeaveEvent) - nothing else. Disabled reasons repeat the checks UDwRun makes, so a disabled button never
 * gets a refusal and an enabled one never does either. They refresh on UDwRun::OnStateChanged.
 */

/** One clickable card or button on a run screen (an ability offer, a crew member, a perk, Take, Leave, ...). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwChoiceView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Choice") FText Title;
	UPROPERTY(BlueprintReadOnly, Category = "Choice") FText Body;
	UPROPERTY(BlueprintReadOnly, Category = "Choice") bool bEnabled = true;
	UPROPERTY(BlueprintReadOnly, Category = "Choice") bool bSelected = false;
	/** Why it can't be clicked; empty when enabled. */
	UPROPERTY(BlueprintReadOnly, Category = "Choice") FText DisabledReason;
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
// Shop and event shells

/** Phases Shop and Event until DW-204 / DW-205 fill them: a title, a line of text and the way out. */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwNodeShellViewModel : public UDwRunScreenViewModel
{
	GENERATED_BODY()

public:
	FText GetHeadline() const { return Headline; }
	FText GetBody() const { return Body; }
	/** Leave (shop) / Continue (event): UDwRun::LeaveShop / LeaveEvent. */
	UDwChoiceViewModel* GetLeave() const { return Leave; }

protected:
	virtual void Refresh() override;

private:
	FText Headline;
	FText Body;
	UPROPERTY(Transient) TObjectPtr<UDwChoiceViewModel> Leave;
};
