// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/DwDefinitionBase.h"
#include "Data/DwDataTypes.h"
#include "DwEncounterDefinition.generated.h"

class ADwShip;
class UDwAiProfile;
class UDwDeckLayout;
class UDwKrakenDefinition;
class UDwUnitDefinition;

/** One enemy in a ship battle. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwEnemySlot
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UDwUnitDefinition> UnitDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "1"))
	int32 Level = 1;

	/** Overrides the encounter's AiProfile for this enemy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UDwAiProfile> AiProfileOverride;
};

/** A fight node's content. An "enemy crew" in Scope terms = one ShipBattle encounter (Docs/Architecture/13). */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwEncounterDefinition : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Encounter")
	EDwEncounterKind Kind = EDwEncounterKind::ShipBattle;

	/** Map layers this encounter may appear on (difficulty tier, Open-Questions N3). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Encounter", meta = (ClampMin = "0"))
	int32 MinLayer = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Encounter", meta = (ClampMin = "0"))
	int32 MaxLayer = 99;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Encounter", meta = (ClampMin = "0"))
	float Weight = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship Battle", meta = (EditCondition = "Kind == EDwEncounterKind::ShipBattle"))
	TObjectPtr<UDwDeckLayout> ShipDeck;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship Battle", meta = (EditCondition = "Kind == EDwEncounterKind::ShipBattle"))
	TSoftClassPtr<ADwShip> ShipClass;

	/** DT_ShipTuning row for the enemy ship. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship Battle", meta = (EditCondition = "Kind == EDwEncounterKind::ShipBattle"))
	FDataTableRowHandle ShipTuning;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship Battle", meta = (EditCondition = "Kind == EDwEncounterKind::ShipBattle"))
	TArray<FDwEnemySlot> Units;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship Battle", meta = (EditCondition = "Kind == EDwEncounterKind::ShipBattle"))
	TObjectPtr<UDwAiProfile> AiProfile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kraken", meta = (EditCondition = "Kind == EDwEncounterKind::Kraken"))
	TObjectPtr<UDwKrakenDefinition> Kraken;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::Encounter; }
	virtual const UScriptStruct* GetTuningRowStruct() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
