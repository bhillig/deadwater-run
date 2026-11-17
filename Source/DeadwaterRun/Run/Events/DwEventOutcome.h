// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Run/DwRunTypes.h"
#include "DwEventOutcome.generated.h"

class UDataTable;
class UDwAbilityDefinition;
class UDwCaptainDefinition;
class UDwItemDefinition;
class UDwUnitDefinition;
struct FDwEconomyTuningRow;

/**
 * Event outcomes and requirements (Docs/Architecture/11-Events.md "Data"). Edited inline on UDwEventDefinition's
 * choices; a small closed set, so Brandon authors events with dropdowns and no code. Outcomes change FDwRunState only
 * (never the world), through the same rules the shop and rewards use, and say what they did in FDwEventContext::Changes.
 */

/** Who a crew outcome lands on. Only living crew are ever picked. */
UENUM(BlueprintType)
enum class EDwCrewSelector : uint8
{
	/** Every living crew member (the captain too, except for LoseCrew). */
	All,
	/** One living crew member, picked from the event's seeded stream. */
	Random,
	Captain,
	/** The one the player picks on the event screen (UDwRun::ChooseEventOption's CrewId). */
	Chosen
};

/** Everything an outcome reads and writes while one choice resolves. Built by UDwRun::ChooseEventOption. */
struct DEADWATERRUN_API FDwEventContext
{
	FDwEventContext(FDwRunState& InState, FRandomStream& InRandom) : State(InState), Random(InRandom) {}

	FDwRunState& State;
	/** FDwRunRandom::ForEvent(Seed, Node): the same seed and choice always resolve the same way. */
	FRandomStream& Random;
	/** EDwCrewSelector::Chosen. */
	FGuid ChosenCrew;
	/** MaxCrew, MaxInventory, RecruitLevelOffset. Null = no caps, recruits join at level 1. */
	const FDwEconomyTuningRow* Economy = nullptr;
	/** GrantXp: DT_ProgressionTuning and DT_RunTuning.MaxLevel, with the captain's Veteran Crew multipliers. */
	const UDataTable* Progression = nullptr;
	int32 MaxLevel = 0;
	float XpMultiplier = 1.f;
	float ThresholdMultiplier = 1.f;

	/** Out: one line per applied change ("+50 gold", "Swordsman -30 HP (40/70)"), for the result screen and the log. */
	TArray<FText> Changes;
	/** Out: one entry per level gained (GrantXp), in the order gained. UDwRun numbers their Ordinals. */
	TArray<FDwPendingLevelUp> LevelUps;
	/** Out: AbilityChoice offers, shown on the rewards screen after the event. */
	TArray<FDwAbilityOffer> AbilityChoices;

	/** Living crew matching Selector. bAllowCaptain = false leaves the captain out of All, Random and Chosen. */
	TArray<FDwCrewMemberState*> Select(EDwCrewSelector Selector, bool bAllowCaptain = true);
	int32 GetMaxInventory() const;
};

/** One thing an event choice does. Abstract: pick a subclass in the choice's Outcomes list. */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class DEADWATERRUN_API UDwEventOutcome : public UObject
{
	GENERATED_BODY()

public:
	/** Changes the run and appends what it did to Context.Changes. */
	virtual void Apply(FDwEventContext& Context) const PURE_VIRTUAL(UDwEventOutcome::Apply, );

	/** "You will ..." line for the event screen and data validation ("-40 gold", "A random crew member loses 30 HP"). */
	virtual FText Describe() const { return FText::GetEmpty(); }

	/** True when the player has to pick a crew member before this choice resolves (EDwCrewSelector::Chosen). */
	virtual bool NeedsChosenCrew() const { return false; }

	/** Whether Member may be the picked crew member (LoseCrew never takes the captain). */
	virtual bool AcceptsChosen(const FDwCrewMemberState& Member) const { return true; }

	/** Authoring problems for UDwEventDefinition::IsDataValid. */
	virtual void Validate(TArray<FText>& OutProblems) const {}
};

/** An explicit "nothing happens": a choice with no other outcome must say so (11 "How it's tested"). */
UCLASS(meta = (DisplayName = "Nothing"))
class DEADWATERRUN_API UDwEventOutcome_Nothing : public UDwEventOutcome
{
	GENERATED_BODY()

public:
	virtual void Apply(FDwEventContext& Context) const override {}
	virtual FText Describe() const override;
};

/** Gold up or down. Gold never goes below 0. */
UCLASS(meta = (DisplayName = "Gold"))
class DEADWATERRUN_API UDwEventOutcome_Gold : public UDwEventOutcome
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcome") int32 Delta = 0;

	virtual void Apply(FDwEventContext& Context) const override;
	virtual FText Describe() const override;
	virtual void Validate(TArray<FText>& OutProblems) const override;
};

/** Hull up (to max) or down. C10a a): event damage never sinks the ship, the hull stops at 1. */
UCLASS(meta = (DisplayName = "Ship Health"))
class DEADWATERRUN_API UDwEventOutcome_ShipHealth : public UDwEventOutcome
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcome") int32 Delta = 0;

	virtual void Apply(FDwEventContext& Context) const override;
	virtual FText Describe() const override;
	virtual void Validate(TArray<FText>& OutProblems) const override;
};

/** Crew HP up (to max) or down. C10a a): event damage never kills, Health stops at 1. */
UCLASS(meta = (DisplayName = "Crew Health"))
class DEADWATERRUN_API UDwEventOutcome_CrewHealth : public UDwEventOutcome
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcome") int32 Delta = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcome") EDwCrewSelector Selector = EDwCrewSelector::All;

	virtual void Apply(FDwEventContext& Context) const override;
	virtual FText Describe() const override;
	virtual bool NeedsChosenCrew() const override { return Selector == EDwCrewSelector::Chosen; }
	virtual void Validate(TArray<FText>& OutProblems) const override;
};

/** Items into the shared inventory, as far as MaxInventory allows (P10); the rest is left behind and said so. */
UCLASS(meta = (DisplayName = "Give Item"))
class DEADWATERRUN_API UDwEventOutcome_GiveItem : public UDwEventOutcome
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcome") TObjectPtr<const UDwItemDefinition> Item;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcome", meta = (ClampMin = "1")) int32 Count = 1;

	virtual void Apply(FDwEventContext& Context) const override;
	virtual FText Describe() const override;
	virtual void Validate(TArray<FText>& OutProblems) const override;
};

/** "Learn a new move": Count ability offers (the rewards flow, FDwRewardRules::RollAbilityChoices) after the event. */
UCLASS(meta = (DisplayName = "Ability Choice"))
class DEADWATERRUN_API UDwEventOutcome_AbilityChoice : public UDwEventOutcome
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcome", meta = (ClampMin = "1")) int32 Count = 3;

	virtual void Apply(FDwEventContext& Context) const override;
	virtual FText Describe() const override;
	virtual void Validate(TArray<FText>& OutProblems) const override;
};

/** XP (x the captain's Veteran Crew multiplier); level-ups it causes are played after the event. */
UCLASS(meta = (DisplayName = "Grant XP"))
class DEADWATERRUN_API UDwEventOutcome_GrantXp : public UDwEventOutcome
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcome", meta = (ClampMin = "1")) int32 Amount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcome") EDwCrewSelector Selector = EDwCrewSelector::All;

	virtual void Apply(FDwEventContext& Context) const override;
	virtual FText Describe() const override;
	virtual bool NeedsChosenCrew() const override { return Selector == EDwCrewSelector::Chosen; }
	virtual void Validate(TArray<FText>& OutProblems) const override;
};

/**
 * A new crew member of Unit at the shop's recruit level (P8, FDwShopRules::GetRecruitLevel). With MaxCrew aboard it
 * does nothing and says so; gate the choice with CrewCountAtMost to keep it from being a trap.
 */
UCLASS(meta = (DisplayName = "Recruit"))
class DEADWATERRUN_API UDwEventOutcome_Recruit : public UDwEventOutcome
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcome") TObjectPtr<const UDwUnitDefinition> Unit;

	virtual void Apply(FDwEventContext& Context) const override;
	virtual FText Describe() const override;
	virtual void Validate(TArray<FText>& OutProblems) const override;
};

/** Permadeath off-screen: crew member(s) gone for good. Never the captain (05 "Spawn and write-back"). */
UCLASS(meta = (DisplayName = "Lose Crew"))
class DEADWATERRUN_API UDwEventOutcome_LoseCrew : public UDwEventOutcome
{
	GENERATED_BODY()

public:
	/** All = every living crew member beside the captain. Captain is not allowed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcome") EDwCrewSelector Selector = EDwCrewSelector::Random;

	virtual void Apply(FDwEventContext& Context) const override;
	virtual FText Describe() const override;
	virtual bool NeedsChosenCrew() const override { return Selector == EDwCrewSelector::Chosen; }
	virtual bool AcceptsChosen(const FDwCrewMemberState& Member) const override { return !Member.bIsCaptain; }
	virtual void Validate(TArray<FText>& OutProblems) const override;
};

/** One branch of a gamble. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwEventRandomOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option", meta = (ClampMin = "0")) float Weight = 1.f;
	/** Applied in order when this branch is picked; empty = nothing happens. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Option") TArray<TObjectPtr<UDwEventOutcome>> Outcomes;
};

/** A gamble: one weighted branch, picked from the event's seeded stream (stable per seed). */
UCLASS(meta = (DisplayName = "Random Of"))
class DEADWATERRUN_API UDwEventOutcome_RandomOf : public UDwEventOutcome
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcome") TArray<FDwEventRandomOption> Options;

	/** Index of the branch Random picks (INDEX_NONE when no option has weight). Advances Random once. */
	int32 PickOption(FRandomStream& Random) const;

	virtual void Apply(FDwEventContext& Context) const override;
	virtual FText Describe() const override;
	virtual bool NeedsChosenCrew() const override;
	virtual bool AcceptsChosen(const FDwCrewMemberState& Member) const override;
	virtual void Validate(TArray<FText>& OutProblems) const override;
};

// ---------------------------------------------------------------------------------------------------------------------
// Requirements

/** A condition that must hold for an event choice to be enabled. Abstract: pick a subclass in the choice's list. */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class DEADWATERRUN_API UDwEventRequirement : public UObject
{
	GENERATED_BODY()

public:
	virtual bool IsMet(const FDwRunState& State) const PURE_VIRTUAL(UDwEventRequirement::IsMet, return true;);

	/** Shown on a disabled choice ("Requires 50 gold"). */
	virtual FText Describe() const { return FText::GetEmpty(); }

	virtual void Validate(TArray<FText>& OutProblems) const {}
};

UCLASS(meta = (DisplayName = "Min Gold"))
class DEADWATERRUN_API UDwEventRequirement_MinGold : public UDwEventRequirement
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement", meta = (ClampMin = "1")) int32 Amount = 0;

	virtual bool IsMet(const FDwRunState& State) const override { return State.Gold >= Amount; }
	virtual FText Describe() const override;
	virtual void Validate(TArray<FText>& OutProblems) const override;
};

/** Living crew, the captain included. */
UCLASS(meta = (DisplayName = "Crew Count At Least"))
class DEADWATERRUN_API UDwEventRequirement_CrewCountAtLeast : public UDwEventRequirement
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement", meta = (ClampMin = "1")) int32 Count = 1;

	virtual bool IsMet(const FDwRunState& State) const override { return State.CountLivingCrew() >= Count; }
	virtual FText Describe() const override;
};

/** Living crew, the captain included. Gates Recruit choices: "CrewCountAtMost 3" = room for one more of 4. */
UCLASS(meta = (DisplayName = "Crew Count At Most"))
class DEADWATERRUN_API UDwEventRequirement_CrewCountAtMost : public UDwEventRequirement
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement", meta = (ClampMin = "1")) int32 Count = 3;

	virtual bool IsMet(const FDwRunState& State) const override { return State.CountLivingCrew() <= Count; }
	virtual FText Describe() const override;
};

UCLASS(meta = (DisplayName = "Has Item"))
class DEADWATERRUN_API UDwEventRequirement_HasItem : public UDwEventRequirement
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement") TObjectPtr<const UDwItemDefinition> Item;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement", meta = (ClampMin = "1")) int32 Count = 1;

	virtual bool IsMet(const FDwRunState& State) const override;
	virtual FText Describe() const override;
	virtual void Validate(TArray<FText>& OutProblems) const override;
};

UCLASS(meta = (DisplayName = "Captain Is"))
class DEADWATERRUN_API UDwEventRequirement_CaptainIs : public UDwEventRequirement
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement") TObjectPtr<const UDwCaptainDefinition> Captain;

	virtual bool IsMet(const FDwRunState& State) const override { return Captain && State.Captain == Captain; }
	virtual FText Describe() const override;
	virtual void Validate(TArray<FText>& OutProblems) const override;
};
