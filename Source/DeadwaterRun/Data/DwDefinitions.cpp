// Copyright Brandon Hillig 2026

// Tuning-row bindings and data validation for every definition class in Data/.
// Kept in one translation unit so the rules are reviewable side by side; behaviour arrives with each system's ticket.

#include "Core/DwGameplayTags.h"
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
	if (!Body || !Arena)
	{
		DwValidation::Error(Context, Result, LOCTEXT("KrakenParts", "Body and Arena must both be set."));
	}
	// The DW-209 placeholder has no tentacles; a Kraken that has some (DW-301) needs their unit.
	const FDwKrakenTuningRow* KrakenTuning = GetTuningRow<FDwKrakenTuningRow>();
	if (KrakenTuning && KrakenTuning->TentacleCount > 0 && !Tentacle)
	{
		DwValidation::Error(Context, Result, LOCTEXT("KrakenTentacle", "DT_KrakenTuning asks for tentacles, so Tentacle must be set."));
	}
	if (Body && !Body->UnitTags.HasTagExact(DwTags::Unit_Kraken_Body))
	{
		DwValidation::Error(Context, Result, LOCTEXT("KrakenBodyTag", "Body must carry Unit.Kraken.Body (killing it wins the boss fight)."));
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

FText UDwPerkDefinition::GetFormattedDescription() const
{
	const FDwPerkTuningRow* Row = GetTuningRow<FDwPerkTuningRow>();
	const float Value = Row ? Row->Value : 0.f;
	const float Value2 = Row ? Row->Value2 : 0.f;
	FFormatNamedArguments Args;
	Args.Add(TEXT("Value"), FText::AsNumber(Value));
	Args.Add(TEXT("ValuePct"), FText::AsNumber(FMath::RoundToInt(Value * 100.f)));
	Args.Add(TEXT("Value2"), FText::AsNumber(Value2));
	Args.Add(TEXT("Value2Pct"), FText::AsNumber(FMath::RoundToInt(Value2 * 100.f)));
	return FText::Format(Description, Args);
}
