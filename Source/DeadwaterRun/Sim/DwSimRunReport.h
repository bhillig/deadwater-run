// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "DwSimRunReport.generated.h"

/** One fight of a simulated run. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwSimFightReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 Node = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 Layer = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Encounter;
	/** EDwCombatOutcome name, or "Stuck". */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Outcome;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 Rounds = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 PlayerActions = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 EnemyActions = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 CrewLost = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 HullBefore = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 HullAfter = 0;
	/** Player commands the combat subsystem refused: the brain and the rules disagree (always a bug). */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 RejectedCommands = 0;
};

/** One shop visit. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwSimShopReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 Node = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 Layer = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 GoldOnArrival = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 HullPctOnArrival = 0;
	/** A repair was on sale and affordable on arrival. */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") bool bCouldAffordRepair = false;
	/** EDwShopOfferType name per purchase, in order. */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") TArray<FString> Bought;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 GoldOnLeaving = 0;
};

/**
 * One simulated run (Docs/Architecture/17 "FDwSimRunReport"). Only data that follows from build, content, seed and policy:
 * no wall-clock time, so the same inputs give a byte-identical report (DeadwaterRun.Sim.Determinism).
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwSimRunReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 Seed = 0;
	/** Captain short name (TeamLeader, Fearmongerer, Conqueror). */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Captain;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Policy;
	/** "Victory", "Defeat" or "Stuck". */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Result;
	/** EDwRunEndReason name ("None" when stuck). */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString EndReason;
	/** Why the run is Stuck: the phase and what failed. Empty otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString StuckReason;
	/** The node the run ended on (the boss on a victory) and its layer: where runs die. */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 LastNode = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 LastLayer = INDEX_NONE;
	/** The encounter of the fight that ended a defeated run. */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString DiedTo;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 NodesCleared = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 Kills = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 Rounds = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 FightsSunk = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 FightsPlundered = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 FightsSurrendered = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 GoldEarned = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 GoldSpent = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 CrewDeaths = 0;
	/** Levels gained by the whole crew over the run (sum over every crew member who ever sailed). */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 LevelsGained = 0;
	/** Decisions taken (phase steps): the stuck-run cap counts these. */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 Steps = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") float EstimatedMinutes = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") TArray<FDwSimFightReport> Fights;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") TArray<FDwSimShopReport> Shops;

	bool IsVictory() const { return Result == TEXT("Victory"); }
	bool IsStuck() const { return Result == TEXT("Stuck"); }
};

/** Per-captain line of the summary. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwSimCaptainSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Captain;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 Runs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 Wins = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") float WinRatePct = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") float AvgMinutes = 0.f;
};

/** The balance metrics of Docs/Architecture/17 "Metrics", over a batch of runs. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwSimSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 Runs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 Wins = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 Stuck = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") TArray<FDwSimCaptainSummary> Captains;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") float AvgMinutes = 0.f;
	/** Defeats per map layer (index = layer; the boss is the last). */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") TArray<int32> DefeatsByLayer;
	/** Defeats per encounter asset name. */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") TMap<FString, int32> DefeatsByEncounter;
	/** Won fights by outcome: sunk, plundered, surrendered. */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 FightsSunk = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 FightsPlundered = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 FightsSurrendered = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") float AvgGoldOnShopArrival = 0.f;
	/** Of the runs that reached the boss and saw a shop: % whose last shop had no affordable repair. */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") float PctNoRepairBeforeBoss = 0.f;
	/** Crew levels gained per fight fought (target about 1, Scope). */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") float LevelsPerFight = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") float AvgCrewDeaths = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") float AvgRoundsPerFight = 0.f;
};

/** report.json: the summary, then every run. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwSimReportFile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Policy;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 FirstSeed = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FDwSimSummary Summary;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") TArray<FDwSimRunReport> Runs;
};

/** Aggregation and serialization (pure; the commandlet and the tests share it). */
struct DEADWATERRUN_API FDwSimReports
{
	static FDwSimSummary Summarize(TConstArrayView<FDwSimRunReport> Runs);

	/** Pretty-printed JSON of the whole file. Deterministic for equal input. */
	static FString ToJson(const FDwSimReportFile& File);

	/** summary.csv: a header line and one line per run. */
	static FString ToCsv(TConstArrayView<FDwSimRunReport> Runs);

	/** "N runs | win TeamLeader x% ... | avg m min | stuck s": the tail of the SIM PASS line. */
	static FString OneLine(const FDwSimSummary& Summary);
};
