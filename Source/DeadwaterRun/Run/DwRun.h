// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Run/DwRunTypes.h"
#include "DwRun.generated.h"

class UDwCaptainDefinition;
class UDwEncounterDefinition;
class UDwGameData;
struct FDwEconomyTuningRow;
struct FDwRunTuningRow;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDwOnRunPhaseChanged, EDwRunPhase, NewPhase, EDwRunPhase, OldPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDwOnRunStateChanged);

/**
 * One voyage (Docs/Architecture/09-Run-Map.md D1). World-free: the game holds it in UDwRunSubsystem, the simulator and
 * tests create it directly. The only writer of FDwRunState: every change goes through a named method that checks the
 * phase, applies the change, then broadcasts OnStateChanged and (if the phase moved) OnPhaseChanged.
 * Calls in the wrong phase are refused (false, logged as a warning) and change nothing.
 *
 * Flow: Map -> ChooseNode(fight) -> Combat -> ApplyCombatResult -> Rewards -> ClaimRewards -> [LevelUp -> ChoosePerk]
 * x levels gained -> Map ... -> ChooseNode(boss) -> Combat -> Victory. Any defeat -> Defeat. Abandon -> Defeat (Abandoned).
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwRun : public UObject
{
	GENERATED_BODY()

public:
	/** DT_EconomyTuning / DT_RunTuning single-row tables. */
	static const FName DefaultTuningRow;

	/**
	 * Sets up a new run for Captain: the roster (captain, then the starting crew, full Health, level 1), the ship at full
	 * Health, starting gold, and the map (FDwMapGenerator + FDwMapValidator) from the encounter pool. Phase = Map.
	 * False (with a message per problem) if content is missing; the run is unusable then.
	 */
	bool Initialize(const UDwGameData& InGameData, const UDwCaptainDefinition& Captain, int32 Seed,
		TConstArrayView<const UDwEncounterDefinition*> EncounterPool, TArray<FString>* OutErrors = nullptr);

	const FDwRunState& GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Deadwater|Run") EDwRunPhase GetPhase() const { return State.Phase; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|Run") bool IsOver() const { return State.IsOver(); }
	UFUNCTION(BlueprintPure, Category = "Deadwater|Run") int32 GetSeed() const { return State.Seed; }

	/** The current node, or null before the first one. */
	const FDwMapNode* GetCurrentNode() const;

	/** Node indices the player can sail to now. Empty outside the Map phase. */
	UFUNCTION(BlueprintPure, Category = "Deadwater|Run")
	TArray<int32> GetChoices() const;

	/**
	 * Map phase only. Sails to NodeIndex if it is one of GetChoices() (forward only): marks it visited, makes it current
	 * and enters it - Fight/Boss -> Combat, Shop -> Shop, Event -> Event.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Run")
	bool ChooseNode(int32 NodeIndex);

	/**
	 * Combat phase only. The fight at the current node: the living crew with their Health, level, learned abilities, the
	 * captain's two abilities and rule effects; the ship's Health; the inventory; captain rule tags; seed =
	 * FDwRunRandom::ForNode(Seed, CurrentNode). Mana is never carried: it starts full at spawn (C9).
	 */
	bool BuildEncounterSetup(FDwEncounterSetup& Out, TArray<FString>& OutErrors) const;

	/**
	 * Combat phase only. Writes Health back, applies permadeath (bAlive = false, never revived), the ship's Health and the
	 * items used, and updates stats. Any Defeat* (including captain death, C10) -> Defeat; VictoryBoss -> Victory;
	 * other victories -> Rewards: kill XP to each living killer and ClearXp to every living crew member (x the captain's
	 * Veteran Crew multipliers), level-ups queued in PendingLevelUps, gold/plunder/ability choices in PendingRewards.
	 */
	bool ApplyCombatResult(const FDwCombatResult& Result);

	/**
	 * Rewards phase only. Adds the pending gold and items and, if AbilityChoice is a valid index, teaches that offer's
	 * ability to CrewId (one of its EligibleCrew). With MaxAbilitySlots learned already, ReplaceSlot (an index into the
	 * crew member's Abilities) must name the one it replaces (P4); otherwise the claim is refused. INDEX_NONE skips the
	 * ability. Then LevelUp if level-ups are pending, else back to the map.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Run")
	bool ClaimRewards(int32 AbilityChoice, FGuid CrewId, int32 ReplaceSlot = -1);

	/**
	 * LevelUp phase only. Applies PerkChoices[ChoiceIndex] of the front level-up to its crew member (INDEX_NONE only when
	 * there were no choices), then shows the next level-up or returns to the map.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Run")
	bool ChoosePerk(int32 ChoiceIndex);

	/** The level-up on screen (front of PendingLevelUps), or null. */
	const FDwPendingLevelUp* GetCurrentLevelUp() const;

	/** Shop phase only: back to the map. DW-204 adds purchases before it. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Run")
	bool LeaveShop();

	/** Event phase only: back to the map. DW-205 replaces it with ChooseEventOption. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Run")
	bool LeaveEvent();

	/** Ends the run as Defeat (Abandoned), from any phase except the two terminal ones (U5). */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Run")
	bool Abandon();

	/** Game only (UDwRunSubsystem): real seconds since Set Sail, for the summary. */
	void SetElapsedSeconds(float Seconds);

	const UDwGameData* GetGameData() const { return GameData; }

	UPROPERTY(BlueprintAssignable, Category = "Deadwater|Run") FDwOnRunPhaseChanged OnPhaseChanged;
	/** After every accepted change, phase or not. View models refresh on it. */
	UPROPERTY(BlueprintAssignable, Category = "Deadwater|Run") FDwOnRunStateChanged OnStateChanged;

private:
	bool RequirePhase(EDwRunPhase Expected, const TCHAR* What) const;
	FGuid MakeCrewId();
	void AddCrewMember(const UDwUnitDefinition& Unit, bool bIsCaptain);
	/** Back to the map after a completed node (counts it as cleared). */
	void CompleteNode();
	/** XP, level-ups and the reward bundle after a won (non-boss) fight. */
	void QueueRewards(const FDwCombatResult& Result);
	/** Rolls the front level-up's perk choices once. */
	void RollCurrentLevelUp();
	const FDwEconomyTuningRow* GetEconomyTuning() const;
	const FDwRunTuningRow* GetRunTuning() const;
	void EndRun(EDwRunPhase Phase, EDwRunEndReason Reason);
	/** OnStateChanged, then OnPhaseChanged if the phase moved away from OldPhase. */
	void Changed(EDwRunPhase OldPhase);

	UPROPERTY() FDwRunState State;
	UPROPERTY() TObjectPtr<const UDwGameData> GameData;
};
