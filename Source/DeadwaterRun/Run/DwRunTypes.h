// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Combat/DwCombatTypes.h"
#include "Data/DwDataTypes.h"
#include "DwRunTypes.generated.h"

class UDwAbilityDefinition;
class UDwCaptainDefinition;
class UDwEncounterDefinition;
class UDwPerkDefinition;
class UDwUnitDefinition;

/*
 * Pure run state (Docs/Architecture/09-Run-Map.md "Run classes"). No world: the simulator and tests drive UDwRun
 * directly. Definitions are held as object pointers (like FDwCombatantSetup), not FPrimaryAssetIds, so transient test
 * content works and UDwRun keeps them referenced; every Deadwater definition is resident anyway (UDwAssetManager).
 */

/** Where the run is. Captain select and the summary are shell screens outside these phases (16-Shell.md). */
UENUM(BlueprintType)
enum class EDwRunPhase : uint8
{
	/** Picking the next node. */
	Map,
	/** A fight (fight node or the boss) is being played. */
	Combat,
	/** DW-203: gold, abilities, plunder after a won fight. */
	Rewards,
	/** DW-203: perk choices. */
	LevelUp,
	/** DW-204. */
	Shop,
	/** DW-205. */
	Event,
	/** The Kraken is dead. Terminal. */
	Victory,
	/** Ship sunk, crew dead, captain dead or abandoned. Terminal. */
	Defeat
};

/** The run summary's headline (16-Shell.md "End of run"). */
UENUM(BlueprintType)
enum class EDwRunEndReason : uint8
{
	None,
	Victory,
	ShipSunk,
	CrewDead,
	CaptainDead,
	Abandoned
};

/** A crew member between fights (05-Crew-Captains.md). No Mana: MP refills at every fight start (C9). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwCrewMemberState
{
	GENERATED_BODY()

	/** Stable for the whole run; the combat unit reports it back as RosterId. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crew") FGuid Id;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crew") TObjectPtr<const UDwUnitDefinition> Unit;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crew") bool bIsCaptain = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crew") int32 Level = 1;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crew") int32 Xp = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crew") float Health = 0.f;
	/** Permadeath: dead crew stay in the roster with bAlive = false for the summary, never revived. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crew") bool bAlive = true;
	/** Learned abilities (shop, loot, perks), on top of the unit's basic attack and defaults. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crew") TArray<TObjectPtr<const UDwAbilityDefinition>> Abilities;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crew") TArray<TObjectPtr<const UDwPerkDefinition>> Perks;

	/** Unit tuning MaxHealth + HealthPerLevel per level above 1 (the same numbers combat spawns with); 0 without a row. */
	float GetMaxHealth() const;
	FText GetDisplayName() const;
};

/** The player's ship between fights. Upgrades arrive with DW-204. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwShipState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship") float Health = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship") float MaxHealth = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship") float CannonDamage = 0.f;
};

/** Run-long captain rule counters (05-Crew-Captains.md). Written by FDwCaptainRules (DW-206, DW-208). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwCaptainCounters
{
	GENERATED_BODY()

	/** Fearmongerer, Dread Reputation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Captain") int32 SurrenderWins = 0;
	/** Conqueror kill bonus (0.01 = +1%). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Captain") float ConquerorKillBonusPct = 0.f;
};

/** A crew member lost on the voyage (summary "crew lost"). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwCrewLoss
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") FGuid CrewId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") FText Name;
	/** Map node index where they fell. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") int32 Node = INDEX_NONE;
};

/** The story of the voyage: run summary (U4) and simulator report (09 "FDwRunStats"). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwRunStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") TObjectPtr<const UDwCaptainDefinition> Captain;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") int32 Seed = 0;
	/** Nodes completed (fights won, shops left, events resolved, the boss beaten). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") int32 NodesCleared = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") int32 FightsSunk = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") int32 FightsPlundered = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") int32 FightsSurrendered = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") int32 GoldEarned = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") int32 GoldSpent = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") TArray<FDwCrewLoss> CrewLost;
	/** Enemies killed by the crew (sinking credits nobody, C11). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") int32 Kills = 0;
	/** Combat rounds over the whole run (simulator only). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") int32 Rounds = 0;
	/** Real seconds from Set Sail to the end (game only; the simulator leaves 0). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") float ElapsedSeconds = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") EDwRunEndReason EndReason = EDwRunEndReason::None;
};

/** One node on the sea chart. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwMapNode
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map") int32 Layer = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map") int32 Lane = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map") EDwNodeType Type = EDwNodeType::Fight;
	/** Fight and boss nodes: assigned at generation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map") TObjectPtr<const UDwEncounterDefinition> Encounter;
	/** Node indices reachable from here (forward only). Empty for the boss. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map") TArray<int32> Next;
	/** Sailed through: drawn as complete. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map") bool bVisited = false;
};

/** Layered lanes plus one boss node (09 D2). Built by FDwMapGenerator (DW-202); FDwPlaceholderMap until then. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwMapGraph
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map") TArray<FDwMapNode> Nodes;
	/** Layers before the boss. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map") int32 LayerCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map") int32 BossNode = INDEX_NONE;

	bool IsValidNode(int32 Index) const { return Nodes.IsValidIndex(Index); }

	/** Where the player can sail from Current: every first-layer node from the start (INDEX_NONE), else Current's Next. */
	TArray<int32> GetChoices(int32 Current) const;
};

/** Everything a run is (09 "FDwRunState"). Only UDwRun writes it. Shop, event, reward and level-up state arrive with DW-203..205. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwRunState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run") int32 Seed = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run") EDwRunPhase Phase = EDwRunPhase::Map;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run") TObjectPtr<const UDwCaptainDefinition> Captain;
	/** Captain first, then the starting crew in definition order, then recruits. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run") TArray<FDwCrewMemberState> Crew;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run") FDwShipState Ship;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run") int32 Gold = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run") TArray<FDwItemStack> Inventory;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run") FDwMapGraph Map;
	/** INDEX_NONE before the first node. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run") int32 CurrentNode = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run") FDwCaptainCounters Counters;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run") FDwRunStats Stats;
	/** Serial of the last roster id handed out (starting crew, then recruits in DW-204): ids are seeded, never random. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run") int32 LastCrewSerial = 0;

	const FDwCrewMemberState* FindCrew(const FGuid& Id) const;
	FDwCrewMemberState* FindCrew(const FGuid& Id);
	const FDwCrewMemberState* GetCaptainMember() const;
	int32 CountLivingCrew() const;
	int32 CountItem(FName ItemId) const;
	bool IsOver() const { return Phase == EDwRunPhase::Victory || Phase == EDwRunPhase::Defeat; }
};

namespace DwRun
{
	DEADWATERRUN_API FString PhaseName(EDwRunPhase Phase);
	DEADWATERRUN_API FString EndReasonName(EDwRunEndReason Reason);
	/** The end reason a combat defeat means; None for victories and None. */
	DEADWATERRUN_API EDwRunEndReason EndReasonFor(EDwCombatOutcome Outcome);
}
