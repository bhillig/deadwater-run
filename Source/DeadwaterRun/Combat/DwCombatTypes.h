// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Abilities/DwTargeting.h"
#include "Board/DwGridModel.h"
#include "Core/DwTypes.h"
#include "Data/DwDataTypes.h"
#include "DwCombatTypes.generated.h"

class ADwBoard;
class ADwCombatUnit;
class ADwShip;
class UDwAbilityDefinition;
class UDwAiProfile;
class UDwEncounterDefinition;
class UDwItemDefinition;
class UDwUnitDefinition;
class UGameplayEffect;

/*
 * Pure combat types (Docs/Architecture/03-Combat-Turns.md "Pure types"). No world, no actors: commands, events, setup
 * and results are plain data so the player, the AI, tests and the headless simulator all speak the same language.
 */

/** Docs/Architecture/03 D1: every actor submits one of these to UDwCombatSubsystem. */
UENUM(BlueprintType)
enum class EDwCommandType : uint8
{
	/** Ability.Move to Target.Tile. */
	Move,
	/** Any other granted ability, by AbilityTag, aimed at Target. */
	Ability,
	/** The unit is done for this turn (UI convenience on the player side; advances the enemy sequence). */
	EndUnit,
	/** The side ends its turn. */
	EndTurn
};

/** Why SubmitCommand accepted or rejected a command. Every rejection leaves the fight unchanged. */
UENUM(BlueprintType)
enum class EDwCommandResult : uint8
{
	Accepted,
	/** An ability is still resolving (input is locked). */
	Busy,
	/** Wrong side, or (enemy side) not this unit's turn in the sequence. */
	NotYourTurn,
	/** Unknown, dead or off-board unit. */
	InvalidUnit,
	/** The unit has no ability with this tag. */
	UnknownAbility,
	/** Not among the ability's valid targets (out of range, no LOS, unreachable tile, no cannon in reach, ...). */
	InvalidTarget,
	/** Not enough MP, no action or no move left this turn. */
	CannotAfford,
	/** A status blocks the ability (incapacitated, rooted) or GAS refused to activate it. */
	Blocked,
	/** No item of that id left. */
	NoItem,
	/** The fight already has an outcome. */
	CombatOver
};

/**
 * One combat command. Docs/Architecture/03 lists TargetTile/TargetUnit separately; they live in one FDwAbilityTarget
 * here because abilities, previews and the AI already aim with that type (and it can also name a ship, for cannons).
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwCombatCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command") EDwCommandType Type = EDwCommandType::EndTurn;
	/** The acting unit (unused for EndTurn). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command") FDwUnitHandle Unit;
	/** The side ending its turn (EndTurn only; other commands take the unit's side). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command") EDwSide Side = EDwSide::Player;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command") FGameplayTag AbilityTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command") FDwAbilityTarget Target;
	/** Ability.Item only: which item to use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command") FName ItemId;

	static FDwCombatCommand MakeMove(FDwUnitHandle InUnit, FIntPoint Tile);
	static FDwCombatCommand MakeAbility(FDwUnitHandle InUnit, const FGameplayTag& InAbilityTag, const FDwAbilityTarget& InTarget, FName InItemId = NAME_None);
	static FDwCombatCommand MakeEndUnit(FDwUnitHandle InUnit);
	static FDwCombatCommand MakeEndTurn(EDwSide InSide);

	/** The ability tag this command activates (Ability.Move for Move), or an empty tag for EndUnit/EndTurn. */
	FGameplayTag GetAbilityTag() const;

	FString ToString() const;
};

UENUM(BlueprintType)
enum class EDwCombatEventType : uint8
{
	UnitDamaged,
	UnitHealed,
	UnitDied,
	UnitMoved,
	ShipDamaged,
	ShipSunk,
	StatusApplied,
	StatusRemoved,
	FearChanged,
	TurnStarted,
	UnitTurnSkipped,
	AbilityUsed,
	IntentDeclared,
	OutcomeReached
};

/** Docs/Architecture/03 "FDwCombatEvent". Consumers: floating text, combat log, run stats, simulator report, replays. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwCombatEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Event") EDwCombatEventType Type = EDwCombatEventType::TurnStarted;
	UPROPERTY(BlueprintReadOnly, Category = "Event") FDwUnitHandle Source;
	UPROPERTY(BlueprintReadOnly, Category = "Event") FDwUnitHandle Target;
	/** Ship events and TurnStarted: the side concerned. */
	UPROPERTY(BlueprintReadOnly, Category = "Event") EDwSide Side = EDwSide::Neutral;
	/** Damage, heal, fear delta; the round for TurnStarted. */
	UPROPERTY(BlueprintReadOnly, Category = "Event") float Magnitude = 0.f;
	/** Status tag, ability tag, ... */
	UPROPERTY(BlueprintReadOnly, Category = "Event") FGameplayTag Tag;
	/** UnitMoved: the tile reached. */
	UPROPERTY(BlueprintReadOnly, Category = "Event") FIntPoint Tile = FIntPoint::ZeroValue;
};

/** Docs/Architecture/03 "Outcome priority". Defeats are checked before victories (C12). */
UENUM(BlueprintType)
enum class EDwCombatOutcome : uint8
{
	None,
	VictorySunk,
	VictoryCrewDefeated,
	VictorySurrender,
	VictoryBoss,
	DefeatCrewDead,
	DefeatCaptainDead,
	DefeatShipSunk
};

namespace DwCombat
{
	inline bool IsVictory(EDwCombatOutcome Outcome)
	{
		return Outcome == EDwCombatOutcome::VictorySunk || Outcome == EDwCombatOutcome::VictoryCrewDefeated
			|| Outcome == EDwCombatOutcome::VictorySurrender || Outcome == EDwCombatOutcome::VictoryBoss;
	}
	inline bool IsDefeat(EDwCombatOutcome Outcome)
	{
		return Outcome == EDwCombatOutcome::DefeatCrewDead || Outcome == EDwCombatOutcome::DefeatCaptainDead
			|| Outcome == EDwCombatOutcome::DefeatShipSunk;
	}
	inline EDwSide Opposite(EDwSide Side) { return Side == EDwSide::Player ? EDwSide::Enemy : EDwSide::Player; }
}

/** Per-encounter switches only; captain death is always a defeat (C10), so it is not a flag. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwOutcomeConfig
{
	GENERATED_BODY()

	/** Fearmongerer (Rule.Captain.Surrender): every living enemy at MaxFear surrenders. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome") bool bSurrenderEnabled = false;
	/** Kraken fight: killing the body wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome") bool bIsBossFight = false;
};

/** One combatant to spawn. Crew come from run state (DW-201); enemies from the encounter definition. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwCombatantSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combatant") TObjectPtr<const UDwUnitDefinition> Definition;
	/** FDwCrewMemberState::Id for crew; invalid for enemies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combatant") FGuid RosterId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combatant", meta = (ClampMin = "1")) int32 Level = 1;
	/** Health carried over from the last fight (C9); < 0 = full. Mana always starts at MaxMana. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combatant") float Health = -1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combatant") bool bIsCaptain = false;
	/** Learned and captain abilities, on top of the definition's basic attack and defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combatant") TArray<TObjectPtr<const UDwAbilityDefinition>> ExtraAbilities;
	/** Perks, captain rule GEs (applied at spawn). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combatant") TArray<TSubclassOf<UGameplayEffect>> SpawnEffects;
	/** Dread Reputation and similar (K9). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combatant") float StartingFear = 0.f;
	/** Overrides the definition's UnitClass (tests, placeholder visuals). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combatant") TSubclassOf<ADwCombatUnit> UnitClassOverride;
	/** Enemies: the crew AI profile (slot override, else the encounter's). Any enemy with one puts the side under UDwAiSideController (DW-106). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combatant") TObjectPtr<const UDwAiProfile> AiProfile;
};

/** A ship to spawn. MaxHealth and CannonDamage are tuning (+ upgrades); Health is run state. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwShipSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship") float MaxHealth = 250.f;
	/** < 0 = full. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship") float Health = -1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship") float CannonDamage = 70.f;
	/** Ship upgrades (DW-204). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship") TArray<TSubclassOf<UGameplayEffect>> UpgradeEffects;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship") TSubclassOf<ADwShip> ShipClass;
	/** UDwDeckLayout::MeshAlignment of this ship's deck: the hull relative to the deck centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship") FTransform MeshAlignment;
};

/** Items of one kind carried into a fight. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item") TObjectPtr<const UDwItemDefinition> Item;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "0")) int32 Count = 0;
};

/**
 * Everything a fight needs (Docs/Architecture/03 "FDwEncounterSetup"). Built by UDwRun (DW-201), by the dev quick start,
 * or inline by tests. The board is passed as a built grid so tests can author decks in ASCII.
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwEncounterSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") TObjectPtr<const UDwEncounterDefinition> Encounter;

	/** Both decks composed by FDwBoardBuilder. Spawn tiles decide where units start, in tile index order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") FDwGridModel Grid;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") FDwGridRules GridRules;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") float TileSize = 100.f;
	/** World gap drawn between the two decks (hull walls and rails); 0 without ship art. Presentation only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") float DeckGap = 0.f;
	/** BP_Board (tile overlay, no grey-box decks); null = ADwBoard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") TSubclassOf<ADwBoard> BoardClass;

	/** Player crew in roster order; the captain is the one with bIsCaptain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") TArray<FDwCombatantSetup> Crew;
	/** Enemies in encounter order, which is also their acting order (C14). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") TArray<FDwCombatantSetup> Enemies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") FDwShipSetup PlayerShip;
	/** False in the Kraken fight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") bool bHasEnemyShip = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") FDwShipSetup EnemyShip;

	/** Ability.Move, Ability.Cannon, Ability.Item: granted to every unit (UDwGameData::CommonAbilities). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") TArray<TObjectPtr<const UDwAbilityDefinition>> CommonAbilities;

	/** Run inventory view. UseItem spends from it; the result reports what was used. Item id = definition asset name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") TArray<FDwItemStack> Items;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") FDwOutcomeConfig OutcomeConfig;
	/** Captain rule tags (Rule.Captain.*), read by synergy, defend, surrender (DW-206..208). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") FGameplayTagContainer CaptainRuleTags;

	/** Cower threshold and policy from DT_CombatTuning "Default". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") float MaxFear = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") EDwFearCowerPolicy FearCowerPolicy = EDwFearCowerPolicy::ResetToZero;

	/** FDwRunRandom::ForNode(RunSeed, NodeIndex). Same seed + same commands = same fight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup") int32 Seed = 0;
};

/** Kill credit for XP (DW-203). Sinking and other uncredited deaths have no killer (C11). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwKillRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Result") FGuid KillerRosterId;
	UPROPERTY(BlueprintReadOnly, Category = "Result") FDwUnitHandle Killer;
	UPROPERTY(BlueprintReadOnly, Category = "Result") FDwUnitHandle Victim;
	UPROPERTY(BlueprintReadOnly, Category = "Result") FPrimaryAssetId VictimDefinition;
	UPROPERTY(BlueprintReadOnly, Category = "Result") EDwSide VictimSide = EDwSide::Enemy;
};

USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwCrewResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Result") FGuid RosterId;
	UPROPERTY(BlueprintReadOnly, Category = "Result") bool bAlive = true;
	UPROPERTY(BlueprintReadOnly, Category = "Result") float Health = 0.f;
};

/** Docs/Architecture/03 "FDwCombatResult". Mana is not returned: it refills every fight (C9). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwCombatResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Result") EDwCombatOutcome Outcome = EDwCombatOutcome::None;
	UPROPERTY(BlueprintReadOnly, Category = "Result") int32 Rounds = 0;
	/** Every crew member that started the fight, in setup order. */
	UPROPERTY(BlueprintReadOnly, Category = "Result") TArray<FDwCrewResult> Crew;
	UPROPERTY(BlueprintReadOnly, Category = "Result") TArray<FDwKillRecord> Kills;
	UPROPERTY(BlueprintReadOnly, Category = "Result") float PlayerShipHealth = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Result") TMap<FName, int32> ItemsUsed;

	bool operator==(const FDwCombatResult& Other) const;
	FString ToString() const;
};

/** Seed + every accepted command, in order: enough to replay a fight exactly (QA, simulator). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwCombatRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Record") int32 Seed = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Record") TArray<FDwCombatCommand> Commands;
};
