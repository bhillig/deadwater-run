// Copyright Brandon Hillig 2026

#include "Run/DwCaptainRules.h"

#include "Core/DwGameplayTags.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Run/DwRunTypes.h"

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

float FDwCaptainRules::GetStartingFear(const FDwCaptainCounters& Counters, const UDwCaptainDefinition* Captain)
{
	if (!Captain || !Captain->RuleTags.HasTagExact(DwTags::Rule_Captain_DreadReputation))
	{
		return 0.f;
	}
	const FDwCaptainTuningRow* Row = Captain->GetTuningRow<FDwCaptainTuningRow>();
	if (!Row)
	{
		return 0.f;
	}
	return static_cast<float>(FMath::Clamp(Counters.SurrenderWins * Row->FearPerSurrender, 0, FMath::Max(0, Row->StartingFearCap)));
}

void FDwCaptainRules::OnCombatWon(FDwCaptainCounters& Counters, EDwCombatOutcome Outcome)
{
	Counters.SurrenderWins += Outcome == EDwCombatOutcome::VictorySurrender ? 1 : 0;
}

FDwCaptainCombatRules FDwCaptainRules::GetCombatRules(const UDwCaptainDefinition* Captain, const FDwCaptainCounters& Counters)
{
	FDwCaptainCombatRules Rules;
	if (!Captain)
	{
		return Rules;
	}
	Rules.RuleTags = Captain->RuleTags;
	if (const FDwCaptainTuningRow* Row = Captain->GetTuningRow<FDwCaptainTuningRow>())
	{
		Rules.SynergyDamageMultiplier = Rules.Has(DwTags::Rule_Captain_Synergy) ? FMath::Max(0.f, Row->SynergyDamageMultiplier) : 1.f;
		Rules.GuardDamageMultiplier = Rules.Has(DwTags::Rule_Captain_Defend) ? FMath::Max(0.f, Row->GuardDamageMultiplier) : 1.f;
		if (Rules.Has(DwTags::Rule_Captain_KillBonus))
		{
			Rules.KillBonusPerKill = FMath::Max(0.f, Row->KillBonusPerKill);
			Rules.KillBonusCap = FMath::Max(0.f, Row->KillBonusCap);
			Rules.KillBonusPct = FMath::Clamp(Counters.ConquerorKillBonusPct, 0.f, Rules.KillBonusCap);
			Rules.KillBonusIgnoredUnitTags = Captain->KillBonusIgnoredUnitTags;
		}
	}
	return Rules;
}

float FDwCaptainRules::OnEnemyKilled(float Current, const FDwCaptainCombatRules& Rules, const FGameplayTagContainer& VictimTags)
{
	if (!Rules.Has(DwTags::Rule_Captain_KillBonus) || VictimTags.HasAny(Rules.KillBonusIgnoredUnitTags))
	{
		return Current;
	}
	return FMath::Min(Current + Rules.KillBonusPerKill, Rules.KillBonusCap);
}
