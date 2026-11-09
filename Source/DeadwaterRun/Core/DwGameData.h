// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/DwDefinitionBase.h"
#include "DwGameData.generated.h"

class ADwShip;
class UDwCaptainDefinition;
class UDwDeckLayout;
class UDwEncounterDefinition;
class UDwMapRules;
class UDwShopDefinition;
class UGameplayEffect;
class UWorld;

/** Shared GameplayEffect classes, so C++ never hard-codes asset paths (Docs/Architecture/02 "Shared GameplayEffects"). Filled in DW-101. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwSharedEffects
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Init") TSubclassOf<UGameplayEffect> InitUnitStats;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Init") TSubclassOf<UGameplayEffect> InitShipStats;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core") TSubclassOf<UGameplayEffect> Damage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core") TSubclassOf<UGameplayEffect> Heal;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core") TSubclassOf<UGameplayEffect> RestoreMana;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core") TSubclassOf<UGameplayEffect> AddFear;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core") TSubclassOf<UGameplayEffect> Cleanse;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status") TSubclassOf<UGameplayEffect> Cowering;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status") TSubclassOf<UGameplayEffect> Frozen;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status") TSubclassOf<UGameplayEffect> Rooted;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status") TSubclassOf<UGameplayEffect> Guarded;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Captain") TSubclassOf<UGameplayEffect> ConquerorExtraMove;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Captain") TSubclassOf<UGameplayEffect> DamageBonus;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn") TSubclassOf<UGameplayEffect> TurnBudgetRefresh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn") TSubclassOf<UGameplayEffect> CostAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn") TSubclassOf<UGameplayEffect> CostMove;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn") TSubclassOf<UGameplayEffect> CostMana;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn") TSubclassOf<UGameplayEffect> SpendAll;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Core") TSubclassOf<UGameplayEffect> Kill;
};

/** Every tuning table, so systems find numbers without asset paths (Docs/Architecture/13 "Tuning tables"). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwTuningTables
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tuning") TObjectPtr<UDataTable> Unit;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tuning") TObjectPtr<UDataTable> Ability;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tuning") TObjectPtr<UDataTable> Item;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tuning") TObjectPtr<UDataTable> Perk;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tuning") TObjectPtr<UDataTable> Captain;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tuning") TObjectPtr<UDataTable> Ship;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tuning") TObjectPtr<UDataTable> Encounter;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tuning") TObjectPtr<UDataTable> Economy;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tuning") TObjectPtr<UDataTable> Progression;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tuning") TObjectPtr<UDataTable> Run;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tuning") TObjectPtr<UDataTable> Combat;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tuning") TObjectPtr<UDataTable> Kraken;
};

/**
 * The one global data asset, DA_GameData (Docs/Architecture/13-Data-Layout.md "UDwGameData").
 * Slots for UDwAudioCueMap, UDwMusicSet and UDwUIScreenSet are added by DW-114 and DW-107 with their classes.
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwGameData : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	FDwSharedEffects Effects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tuning")
	FDwTuningTables TuningTables;

	/** Display order on the captain select screen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Run")
	TArray<TObjectPtr<UDwCaptainDefinition>> Captains;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Run")
	TObjectPtr<UDwMapRules> MapRules;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Run")
	TObjectPtr<UDwShopDefinition> DefaultShop;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Run")
	TObjectPtr<UDwEncounterDefinition> KrakenEncounter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<UDwDeckLayout> DefaultPlayerDeck;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship")
	TSoftClassPtr<ADwShip> PlayerShipClass;

	/** DT_ShipTuning row for the player ship. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship")
	FDataTableRowHandle PlayerShipTuning;

	/** World units per tile; layouts may override. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board", meta = (ClampMin = "1"))
	float TileSize = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Levels")
	TSoftObjectPtr<UWorld> MainMenuLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Levels")
	TSoftObjectPtr<UWorld> SeaLevel;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::GameData; }

	/** The asset name the Asset Manager loads. */
	static const FName AssetName;
};
