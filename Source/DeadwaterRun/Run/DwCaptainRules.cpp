// Copyright Brandon Hillig 2026

#include "Run/DwCaptainRules.h"

#include "Core/DwGameplayTags.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/Tuning/DwTuningRows.h"

namespace DwCaptainRules
{
	/** The captain's tuning row, only if the captain has the Veteran Crew rule. */
	static const FDwCaptainTuningRow* VeteranRow(const UDwCaptainDefinition* Captain)
	{
		return Captain && Captain->RuleTags.HasTagExact(DwTags::Rule_Captain_VeteranCrew) ? Captain->GetTuningRow<FDwCaptainTuningRow>() : nullptr;
	}
}

float FDwCaptainRules::GetXpMultiplier(const UDwCaptainDefinition* Captain)
{
	const FDwCaptainTuningRow* Row = DwCaptainRules::VeteranRow(Captain);
	return Row ? FMath::Max(0.f, Row->XpMultiplier) : 1.f;
}

float FDwCaptainRules::GetLevelThresholdMultiplier(const UDwCaptainDefinition* Captain)
{
	const FDwCaptainTuningRow* Row = DwCaptainRules::VeteranRow(Captain);
	return Row && Row->LevelThresholdMultiplier > 0.f ? Row->LevelThresholdMultiplier : 1.f;
}
