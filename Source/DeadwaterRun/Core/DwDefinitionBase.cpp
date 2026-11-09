// Copyright Brandon Hillig 2026

#include "Core/DwDefinitionBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "DwDefinitionBase"

const FPrimaryAssetType UDwDefinitionBase::FTypes::Captain(TEXT("DwCaptain"));
const FPrimaryAssetType UDwDefinitionBase::FTypes::Unit(TEXT("DwUnit"));
const FPrimaryAssetType UDwDefinitionBase::FTypes::Ability(TEXT("DwAbility"));
const FPrimaryAssetType UDwDefinitionBase::FTypes::AbilityPool(TEXT("DwAbilityPool"));
const FPrimaryAssetType UDwDefinitionBase::FTypes::Item(TEXT("DwItem"));
const FPrimaryAssetType UDwDefinitionBase::FTypes::Perk(TEXT("DwPerk"));
const FPrimaryAssetType UDwDefinitionBase::FTypes::PerkPool(TEXT("DwPerkPool"));
const FPrimaryAssetType UDwDefinitionBase::FTypes::Encounter(TEXT("DwEncounter"));
const FPrimaryAssetType UDwDefinitionBase::FTypes::Kraken(TEXT("DwKraken"));
const FPrimaryAssetType UDwDefinitionBase::FTypes::Event(TEXT("DwEvent"));
const FPrimaryAssetType UDwDefinitionBase::FTypes::ShipUpgrade(TEXT("DwShipUpgrade"));
const FPrimaryAssetType UDwDefinitionBase::FTypes::DeckLayout(TEXT("DwDeckLayout"));
const FPrimaryAssetType UDwDefinitionBase::FTypes::AiProfile(TEXT("DwAiProfile"));
const FPrimaryAssetType UDwDefinitionBase::FTypes::Rules(TEXT("DwRules"));
const FPrimaryAssetType UDwDefinitionBase::FTypes::GameData(TEXT("DwGameData"));

TConstArrayView<FPrimaryAssetType> UDwDefinitionBase::FTypes::All()
{
	static const FPrimaryAssetType Types[] = {
		Captain, Unit, Ability, AbilityPool, Item, Perk, PerkPool, Encounter,
		Kraken, Event, ShipUpgrade, DeckLayout, AiProfile, Rules, GameData
	};
	return Types;
}

FPrimaryAssetId UDwDefinitionBase::GetPrimaryAssetId() const
{
	// Class default objects (including Blueprint subclasses' CDOs) are not content.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return FPrimaryAssetId();
	}
	return FPrimaryAssetId(GetDwAssetType(), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UDwDefinitionBase::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (!GetDwAssetType().IsValid())
	{
		Context.AddError(FText::Format(LOCTEXT("NoType", "{0} does not declare a primary asset type (override GetDwAssetType)."),
			FText::FromString(GetClass()->GetName())));
		Result = EDataValidationResult::Invalid;
	}

	if (DisplayName.IsEmptyOrWhitespace())
	{
		Context.AddError(LOCTEXT("NoDisplayName", "DisplayName is empty."));
		Result = EDataValidationResult::Invalid;
	}

	if (const UScriptStruct* ExpectedRow = GetTuningRowStruct())
	{
		if (!Tuning.DataTable)
		{
			Context.AddError(FText::Format(LOCTEXT("NoTuningTable", "Tuning table is not set (expects a {0} table)."),
				FText::FromString(ExpectedRow->GetName())));
			Result = EDataValidationResult::Invalid;
		}
		else if (Tuning.DataTable->GetRowStruct() != ExpectedRow)
		{
			Context.AddError(FText::Format(LOCTEXT("WrongTuningTable", "Tuning table {0} has rows of {1}, expected {2}."),
				FText::FromString(Tuning.DataTable->GetName()),
				FText::FromString(GetNameSafe(Tuning.DataTable->GetRowStruct())),
				FText::FromString(ExpectedRow->GetName())));
			Result = EDataValidationResult::Invalid;
		}
		else if (!Tuning.DataTable->GetRowMap().Contains(Tuning.RowName))
		{
			Context.AddError(FText::Format(LOCTEXT("MissingTuningRow", "Tuning row '{0}' does not exist in {1}."),
				FText::FromName(Tuning.RowName), FText::FromString(Tuning.DataTable->GetName())));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
