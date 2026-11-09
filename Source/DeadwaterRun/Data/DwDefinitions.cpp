// Copyright Brandon Hillig 2026

// Tuning-row bindings and data validation for every definition class in Data/.
// Kept in one translation unit so the rules are reviewable side by side; behaviour arrives with each system's ticket.

#include "Data/DwAbilityDefinition.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwEventDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwKrakenDefinition.h"
#include "Data/DwMapRules.h"
#include "Data/DwPerkDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "DwDefinitions"

const UScriptStruct* UDwAbilityDefinition::GetTuningRowStruct() const   { return FDwAbilityTuningRow::StaticStruct(); }
const UScriptStruct* UDwUnitDefinition::GetTuningRowStruct() const      { return FDwUnitTuningRow::StaticStruct(); }
const UScriptStruct* UDwCaptainDefinition::GetTuningRowStruct() const   { return FDwCaptainTuningRow::StaticStruct(); }
const UScriptStruct* UDwItemDefinition::GetTuningRowStruct() const      { return FDwItemTuningRow::StaticStruct(); }
const UScriptStruct* UDwPerkDefinition::GetTuningRowStruct() const      { return FDwPerkTuningRow::StaticStruct(); }
const UScriptStruct* UDwEncounterDefinition::GetTuningRowStruct() const { return FDwEncounterTuningRow::StaticStruct(); }
const UScriptStruct* UDwKrakenDefinition::GetTuningRowStruct() const    { return FDwKrakenTuningRow::StaticStruct(); }

#if WITH_EDITOR

namespace DwValidation
{
	static void Error(FDataValidationContext& Context, EDataValidationResult& Result, const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	}

	static void CheckLayers(FDataValidationContext& Context, EDataValidationResult& Result, int32 MinLayer, int32 MaxLayer)
	{
		if (MinLayer > MaxLayer)
		{
			Error(Context, Result, FText::Format(LOCTEXT("Layers", "MinLayer ({0}) is greater than MaxLayer ({1})."), MinLayer, MaxLayer));
		}
	}
}

EDataValidationResult UDwAbilityDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!AbilityClass)
	{
		DwValidation::Error(Context, Result, LOCTEXT("NoAbilityClass", "AbilityClass is not set."));
	}
	if (!AbilityTag.IsValid())
	{
		DwValidation::Error(Context, Result, LOCTEXT("NoAbilityTag", "AbilityTag is not set (commands identify abilities by it)."));
	}
	return Result;
}

EDataValidationResult UDwCaptainDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!CaptainUnit)
	{
		DwValidation::Error(Context, Result, LOCTEXT("NoCaptainUnit", "CaptainUnit is not set."));
	}
	if (CaptainAbilities.Num() != RequiredCaptainAbilities || CaptainAbilities.Contains(nullptr))
	{
		DwValidation::Error(Context, Result, FText::Format(
			LOCTEXT("CaptainAbilities", "A captain needs exactly {0} combat abilities, all set (has {1})."),
			RequiredCaptainAbilities, CaptainAbilities.Num()));
	}
	if (StartingCrew.Contains(nullptr))
	{
		DwValidation::Error(Context, Result, LOCTEXT("NullCrew", "StartingCrew has an empty entry."));
	}
	return Result;
}

EDataValidationResult UDwEncounterDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	DwValidation::CheckLayers(Context, Result, MinLayer, MaxLayer);

	if (Kind == EDwEncounterKind::ShipBattle)
	{
		if (!ShipDeck)
		{
			DwValidation::Error(Context, Result, LOCTEXT("NoDeck", "Ship battle has no ShipDeck."));
		}
		if (ShipClass.IsNull())
		{
			DwValidation::Error(Context, Result, LOCTEXT("NoShipClass", "Ship battle has no ShipClass."));
		}
		if (!ShipTuning.DataTable || ShipTuning.DataTable->GetRowStruct() != FDwShipTuningRow::StaticStruct()
			|| !ShipTuning.DataTable->GetRowMap().Contains(ShipTuning.RowName))
		{
			DwValidation::Error(Context, Result, LOCTEXT("NoShipTuning", "ShipTuning must point at an existing DT_ShipTuning row."));
		}
		if (Units.IsEmpty())
		{
			DwValidation::Error(Context, Result, LOCTEXT("NoUnits", "Ship battle has no enemy units."));
		}
		for (int32 Index = 0; Index < Units.Num(); ++Index)
		{
			if (!Units[Index].UnitDefinition)
			{
				DwValidation::Error(Context, Result, FText::Format(LOCTEXT("NullUnit", "Units[{0}] has no UnitDefinition."), Index));
			}
		}
	}
	else if (!Kraken)
	{
		DwValidation::Error(Context, Result, LOCTEXT("NoKraken", "Kraken encounter has no Kraken definition."));
	}
	return Result;
}

EDataValidationResult UDwEventDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	DwValidation::CheckLayers(Context, Result, MinLayer, MaxLayer);

	if (Choices.Num() < MinChoices || Choices.Num() > MaxChoices)
	{
		DwValidation::Error(Context, Result, FText::Format(
			LOCTEXT("ChoiceCount", "An event needs {0}-{1} choices (has {2})."), MinChoices, MaxChoices, Choices.Num()));
	}
	for (int32 Index = 0; Index < Choices.Num(); ++Index)
	{
		const FDwEventChoice& Choice = Choices[Index];
		if (Choice.Label.IsEmptyOrWhitespace())
		{
			DwValidation::Error(Context, Result, FText::Format(LOCTEXT("NoLabel", "Choices[{0}] has no Label."), Index));
		}
		if (Choice.Outcomes.Contains(nullptr) || Choice.Requirements.Contains(nullptr))
		{
			DwValidation::Error(Context, Result, FText::Format(LOCTEXT("NullOutcome", "Choices[{0}] has an empty outcome or requirement."), Index));
		}
	}
	return Result;
}

EDataValidationResult UDwKrakenDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!Body || !Tentacle || !Arena)
	{
		DwValidation::Error(Context, Result, LOCTEXT("KrakenParts", "Body, Tentacle and Arena must all be set."));
	}
	if (Phases.IsEmpty())
	{
		DwValidation::Error(Context, Result, LOCTEXT("NoPhases", "The Kraken needs at least one phase."));
	}
	else if (!FMath::IsNearlyEqual(Phases[0].EnterBelowHealthPct, 1.f))
	{
		DwValidation::Error(Context, Result, LOCTEXT("FirstPhase", "The first phase must start at full health (EnterBelowHealthPct = 1.0)."));
	}
	for (int32 Index = 1; Index < Phases.Num(); ++Index)
	{
		if (Phases[Index].EnterBelowHealthPct >= Phases[Index - 1].EnterBelowHealthPct)
		{
			DwValidation::Error(Context, Result, FText::Format(
				LOCTEXT("PhaseOrder", "Phases[{0}] must start below the previous phase's health threshold."), Index));
		}
	}
	return Result;
}

EDataValidationResult UDwMapRules::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (MinPathNodes > MaxPathNodes)
	{
		DwValidation::Error(Context, Result, FText::Format(
			LOCTEXT("PathNodes", "MinPathNodes ({0}) is greater than MaxPathNodes ({1})."), MinPathNodes, MaxPathNodes));
	}
	if (MinFightsPerPath > MinPathNodes)
	{
		DwValidation::Error(Context, Result, FText::Format(
			LOCTEXT("Fights", "MinFightsPerPath ({0}) can't exceed MinPathNodes ({1})."), MinFightsPerPath, MinPathNodes));
	}
	return Result;
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
