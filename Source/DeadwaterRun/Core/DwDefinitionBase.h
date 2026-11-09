// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "DwDefinitionBase.generated.h"

/**
 * Base of every Deadwater content definition (Docs/Architecture/13-Data-Layout.md "Definitions").
 * - Primary asset id = (GetDwAssetType(), asset name), registered in DefaultGame.ini AssetManagerSettings.
 * - Structure, references and text live here; numbers live in the linked tuning row (Tuning/*.json -> DT_*Tuning).
 */
UCLASS(Abstract, BlueprintType)
class DEADWATERRUN_API UDwDefinitionBase : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Definition")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Definition", meta = (MultiLine = true))
	FText Description;

	/** Row in the tuning table this definition reads its numbers from. Required when GetTuningRowStruct() is non-null. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Definition")
	FDataTableRowHandle Tuning;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Asset Manager primary asset type, e.g. "DwCaptain". Each concrete definition class overrides this. */
	virtual FPrimaryAssetType GetDwAssetType() const { return FPrimaryAssetType(); }

	/** Row struct the Tuning handle must point at, or nullptr if this definition has no tuning row. */
	virtual const UScriptStruct* GetTuningRowStruct() const { return nullptr; }

	/** Typed tuning lookup. Returns nullptr (and logs) if the row is missing or of the wrong type. */
	template <typename TRow>
	const TRow* GetTuningRow() const
	{
		return Tuning.DataTable && Tuning.DataTable->GetRowStruct() == TRow::StaticStruct()
			? Tuning.GetRow<TRow>(GetPathName())
			: nullptr;
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Primary asset type names. One place so config, code and tests agree. */
	struct DEADWATERRUN_API FTypes
	{
		static const FPrimaryAssetType Captain;
		static const FPrimaryAssetType Unit;
		static const FPrimaryAssetType Ability;
		static const FPrimaryAssetType AbilityPool;
		static const FPrimaryAssetType Item;
		static const FPrimaryAssetType Perk;
		static const FPrimaryAssetType PerkPool;
		static const FPrimaryAssetType Encounter;
		static const FPrimaryAssetType Kraken;
		static const FPrimaryAssetType Event;
		static const FPrimaryAssetType ShipUpgrade;
		static const FPrimaryAssetType DeckLayout;
		static const FPrimaryAssetType AiProfile;
		static const FPrimaryAssetType Rules;
		static const FPrimaryAssetType GameData;

		/** Every type above, in a stable order (asset manager loading, tests). */
		static TConstArrayView<FPrimaryAssetType> All();
	};
};
