// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Combat/DwCombatTypes.h"
#include "Data/DwDataTypes.h"
#include "DwRunTypes.generated.h"

class UDwAbilityDefinition;
class UDwCaptainDefinition;
class UDwEncounterDefinition;
class UDwEventDefinition;
class UDwItemDefinition;
class UDwPerkDefinition;
class UDwShipUpgradeDefinition;
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

	/** Unit tuning MaxHealth + HealthPerLevel per level above 1 (the same numbers combat spawns with), plus max-Health perks; 0 without a row. */
	float GetMaxHealth() const;
	FText GetDisplayName() const;
};

/**
 * The player's ship between fights (06 "Repair and upgrades"). MaxHealth and CannonDamage are the DT_ShipTuning numbers
 * plus the bought upgrades' DT_ShipUpgradeTuning bonuses (FDwShopRules adds them on purchase); combat spawns with them.
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwShipState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship") float Health = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship") float MaxHealth = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship") float CannonDamage = 0.f;
	/** Bought upgrades, in purchase order; each at most once (DW-204). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship") TArray<TObjectPtr<const UDwShipUpgradeDefinition>> Upgrades;

	bool HasUpgrade(const UDwShipUpgradeDefinition* Upgrade) const { return Upgrade && Upgrades.Contains(Upgrade); }
	/** Every upgrade's GrantedShipTags (e.g. Ship.Upgrade.ExtraCannon unlocks the third cannon tile). */
	FGameplayTagContainer GetUpgradeTags() const;
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
	/** Shop purchases by offer type, disbands included (simulator report, 10 "How it's tested"). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") TMap<EDwShopOfferType, int32> Purchases;
	/** Plunder items left behind because the inventory was full (P10). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats") int32 ItemsLeftBehind = 0;
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

/** Layered lanes plus one boss node (09 D2). Built by FDwMapGenerator, checked by FDwMapValidator (DW-202). */
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

/** One "learn a new move" choice after a fight (P5): the ability and who may learn it (10 "Rewards"). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwAbilityOffer
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rewards") TObjectPtr<const UDwAbilityDefinition> Ability;
	/** Living crew whose learnable pool has it, who don't know it yet and aren't blocked from it (K2). Never empty. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rewards") TArray<FGuid> EligibleCrew;
};

/** XP one crew member earned in a fight, for the rewards screen's XP bars (12 "Flow"). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwXpGain
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rewards") FGuid CrewId;
	/** After the captain's XP multiplier. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rewards") int32 Xp = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rewards") int32 LevelsGained = 0;
};

/** What a won fight pays, waiting for ClaimRewards (10 "Rewards"). Gold and items are added on claim, XP already counted. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwRewardBundle
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rewards") EDwCombatOutcome Outcome = EDwCombatOutcome::None;
	/** round(BaseGold x the outcome's loot multiplier). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rewards") int32 Gold = 0;
	/** Plunder (crew defeated or surrender, P6) that fits the inventory: ClaimRewards adds these. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rewards") TArray<FDwItemStack> Items;
	/** Plunder rolled but left behind: the inventory was full (DT_EconomyTuning.MaxInventory, P10). Never added. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rewards") TArray<FDwItemStack> ItemsNoRoom;
	/** Pick at most one (P5). Empty when no living crew can learn anything. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rewards") TArray<FDwAbilityOffer> AbilityChoices;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rewards") TArray<FDwXpGain> Xp;
};

/** Per-level stat gains (12 "a stat increase"), shown before/after on the level-up screen. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwLevelStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level") int32 MaxHealth = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level") int32 MaxMana = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level") int32 AttackDamage = 0;

	FDwLevelStats operator-(const FDwLevelStats& Other) const
	{
		FDwLevelStats Out;
		Out.MaxHealth = MaxHealth - Other.MaxHealth;
		Out.MaxMana = MaxMana - Other.MaxMana;
		Out.AttackDamage = AttackDamage - Other.AttackDamage;
		return Out;
	}
};

/** One level gained, waiting for its perk pick (12 "Flow"). The level and stats are already applied. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwPendingLevelUp
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level") FGuid CrewId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level") int32 NewLevel = 0;
	/** Stats at NewLevel minus stats at NewLevel - 1. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level") FDwLevelStats StatGain;
	/**
	 * Rolled when this level-up reaches the front of the queue (FDwRunRandom::ForPerks), so an earlier pick's stacks
	 * count. Empty when the pool has nothing left to offer: the level-up is then confirmed with ChoosePerk(INDEX_NONE).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level") TArray<TObjectPtr<const UDwPerkDefinition>> PerkChoices;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level") bool bPerksRolled = false;
	/** Position in this node's level-up queue: picks the perk stream, so the choices don't depend on the order of calls. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level") int32 Ordinal = 0;
};

/**
 * Whether a shop purchase goes through (10 "Shop", FDwShopRules::CanPurchase). Checked in this order, so the first
 * problem is the one reported: offer, target, room, gold.
 */
UENUM(BlueprintType)
enum class EDwPurchaseResult : uint8
{
	Ok,
	/** Not in the Shop phase, or no offer with that id. */
	NoOffer,
	/** Bought already (ability, recruit, upgrade and disband offers sell once; repair and items never sell out). */
	SoldOut,
	/** Ability: the crew member is unknown, dead, or can't learn it (not in their pool; the captain's blocked tags, K2). */
	InvalidTarget,
	/** Ability: the crew member knows it already. */
	AlreadyKnown,
	/** Ability: MaxAbilitySlots learned and ReplaceSlot doesn't name one to forget (P4). */
	NoSlot,
	/** Disband: the captain can't be disbanded, and the last crew member beside the captain stays. */
	CantDisband,
	/** Recruit: MaxCrew living crew already (the captain counts). */
	CrewFull,
	/** Repair: the hull is already at max. */
	ShipFull,
	/** Item: the shared inventory holds MaxInventory items (P10). */
	InventoryFull,
	NotEnoughGold
};

/** One thing for sale at the current shop (10 "GenerateInventory"). What the shop screen shows, one card each. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwShopOffer
{
	GENERATED_BODY()

	/** Stable for the visit (its index in FDwShopInventory::Offers); TryPurchase names it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop") int32 OfferId = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop") EDwShopOfferType Type = EDwShopOfferType::Repair;
	/**
	 * Gold it costs, read from the offered thing's tuning row when the shop opens (repair: DT_EconomyTuning.RepairCost;
	 * item: DT_ItemTuning.Price; ability: DT_AbilityTuning.ShopPrice; recruit: DT_UnitTuning.RecruitCost; upgrade:
	 * DT_ShipUpgradeTuning.Price). 0 for disband. A copy for display: the rows stay the only place prices are set.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop") int32 Price = 0;
	/** Repair: hull HP restored (RepairAmount). Disband: gold refunded (DisbandRefund). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop") int32 Amount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop") TObjectPtr<const UDwItemDefinition> Item;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop") TObjectPtr<const UDwAbilityDefinition> Ability;
	/** Ability: living crew who can learn it now (pool, not known, not blocked). Kept current after every purchase. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop") TArray<FGuid> EligibleCrew;
	/** Recruit: the archetype. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop") TObjectPtr<const UDwUnitDefinition> Unit;
	/** Recruit: the level they join at (FDwShopRules::GetRecruitLevel). Kept current after every purchase. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop") int32 Level = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop") TObjectPtr<const UDwShipUpgradeDefinition> Upgrade;
	/** Disband: who leaves. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop") FGuid CrewId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop") bool bSoldOut = false;
};

/** The shop at the current node: rolled once on arrival from FDwRunRandom::ForNode, cleared on LeaveShop. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwShopInventory
{
	GENERATED_BODY()

	/** Map node of this shop; INDEX_NONE when no shop is open. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop") int32 Node = INDEX_NONE;
	/** Repair, items, abilities, recruits, upgrades, then one disband per crew member beside the captain. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop") TArray<FDwShopOffer> Offers;
	/** This shop disbands crew: recruits who join during the visit get a disband offer too. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop") bool bDisbands = false;

	const FDwShopOffer* Find(int32 OfferId) const { return Offers.IsValidIndex(OfferId) ? &Offers[OfferId] : nullptr; }
	FDwShopOffer* Find(int32 OfferId) { return Offers.IsValidIndex(OfferId) ? &Offers[OfferId] : nullptr; }
	bool IsOpen() const { return Node != INDEX_NONE; }
};

/**
 * The event at the current node (11 "Flow"): rolled on arrival, then resolved by one choice. The event screen shows the
 * title, body and choices until ChosenIndex is set, then the result text and the change lines until LeaveEvent.
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwActiveEvent
{
	GENERATED_BODY()

	/** Map node of this event; INDEX_NONE when no event is open. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event") int32 Node = INDEX_NONE;
	/** Null when the pool had nothing for this layer: the event screen just lets the player sail on. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event") TObjectPtr<const UDwEventDefinition> Event;
	/** The choice taken; INDEX_NONE until ChooseEventOption. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event") int32 ChosenIndex = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event") FGuid ChosenCrew;
	/** What changed ("+50 gold", "Swordsman -30 HP (40/70)"), in the order applied. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event") TArray<FText> Changes;

	bool IsOpen() const { return Node != INDEX_NONE; }
	bool IsResolved() const { return ChosenIndex != INDEX_NONE; }
};

/** Everything a run is (09 "FDwRunState"). Only UDwRun writes it. */
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
	/** Rewards phase: the bundle ClaimRewards applies. Empty otherwise. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run") FDwRewardBundle PendingRewards;
	/** LevelUp phase (and Rewards, as a preview): one entry per level gained, in roster order; the front is the one shown. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run") TArray<FDwPendingLevelUp> PendingLevelUps;
	/** Shop phase: what's for sale (DW-204). Empty otherwise. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run") FDwShopInventory Shop;
	/** Event phase: the event at the current node (DW-205). Empty otherwise. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run") FDwActiveEvent Event;
	/** Every event rolled this run, in order: the roll prefers unseen ones (11 "Flow" 1). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run") TArray<TObjectPtr<const UDwEventDefinition>> SeenEvents;

	const FDwCrewMemberState* FindCrew(const FGuid& Id) const;
	FDwCrewMemberState* FindCrew(const FGuid& Id);
	const FDwCrewMemberState* GetCaptainMember() const;
	int32 CountLivingCrew() const;
	int32 CountItem(FName ItemId) const;
	/** Every item carried, all kinds together. */
	int32 CountItems() const;
	/** How many more items fit: MaxInventory minus what's carried (P10); MaxInventory <= 0 = no cap (INT32_MAX). */
	int32 GetItemRoom(int32 MaxInventory) const;
	/** Adds up to Count to the item's stack (one stack per item), as far as GetItemRoom allows. Returns how many were added. */
	int32 AddItem(const UDwItemDefinition* Item, int32 Count, int32 MaxInventory);
	bool IsOver() const { return Phase == EDwRunPhase::Victory || Phase == EDwRunPhase::Defeat; }
};

namespace DwRun
{
	DEADWATERRUN_API FString PhaseName(EDwRunPhase Phase);
	DEADWATERRUN_API FString EndReasonName(EDwRunEndReason Reason);
	DEADWATERRUN_API FString PurchaseResultName(EDwPurchaseResult Result);
	DEADWATERRUN_API FString OfferTypeName(EDwShopOfferType Type);
	/** The end reason a combat defeat means; None for victories and None. */
	DEADWATERRUN_API EDwRunEndReason EndReasonFor(EDwCombatOutcome Outcome);
}
