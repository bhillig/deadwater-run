// Copyright Brandon Hillig 2026

#include "Run/DwProgressionRules.h"

#include "Data/DwAbilityDefinition.h"
#include "Data/DwPerkDefinition.h"
#include "Data/DwPools.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Engine/DataTable.h"

int32 FDwProgressionRules::GetXpToNext(const UDataTable* Progression, int32 Level, float ThresholdMultiplier)
{
	const FDwProgressionTuningRow* Row = Progression && Progression->GetRowStruct() == FDwProgressionTuningRow::StaticStruct()
		? Progression->FindRow<FDwProgressionTuningRow>(FName(*LexToString(Level)), TEXT("FDwProgressionRules"), /*bWarnIfRowMissing*/ false)
		: nullptr;
	if (!Row || Row->XpToNext <= 0)
	{
		return 0;
	}
	return FMath::Max(1, FMath::RoundToInt(Row->XpToNext * FMath::Max(ThresholdMultiplier, 0.f)));
}

int32 FDwProgressionRules::GrantXp(FDwCrewMemberState& Crew, int32 Amount, float XpMultiplier, float ThresholdMultiplier,
	const UDataTable* Progression, int32 MaxLevel)
{
	if (!Crew.bAlive || Amount <= 0)
	{
		return 0;
	}
	auto AtCap = [&Crew, MaxLevel]() { return MaxLevel > 0 && Crew.Level >= MaxLevel; };
	if (AtCap())
	{
		return 0; // XP has nowhere to go
	}
	Crew.Xp += FMath::Max(0, FMath::RoundToInt(Amount * FMath::Max(XpMultiplier, 0.f)));

	int32 Gained = 0;
	for (int32 Needed = GetXpToNext(Progression, Crew.Level, ThresholdMultiplier); Needed > 0 && Crew.Xp >= Needed && !AtCap();
		Needed = GetXpToNext(Progression, Crew.Level, ThresholdMultiplier))
	{
		const float OldMax = Crew.GetMaxHealth();
		Crew.Xp -= Needed;
		++Crew.Level;
		++Gained;
		Crew.Health += FMath::Max(0.f, Crew.GetMaxHealth() - OldMax); // the new Health is usable, not an empty bar
	}
	if (AtCap() || GetXpToNext(Progression, Crew.Level, ThresholdMultiplier) == 0)
	{
		Crew.Xp = 0; // last level: no progress bar to fill
	}
	return Gained;
}

FDwLevelStats FDwProgressionRules::GetLevelStats(const FDwUnitTuningRow& Row, int32 Level)
{
	FDwLevelStats Stats;
	Stats.MaxHealth = Row.GetMaxHealthAtLevel(Level);
	Stats.MaxMana = Row.GetMaxManaAtLevel(Level);
	Stats.AttackDamage = Row.GetAttackDamageAtLevel(Level);
	return Stats;
}

int32 FDwProgressionRules::CountPerk(const FDwCrewMemberState& Crew, const UDwPerkDefinition& Perk)
{
	int32 Count = 0;
	for (const UDwPerkDefinition* Owned : Crew.Perks)
	{
		Count += Owned == &Perk ? 1 : 0;
	}
	return Count;
}

bool FDwProgressionRules::CanTakePerk(const FDwCrewMemberState& Crew, const UDwPerkDefinition& Perk)
{
	if (CountPerk(Crew, Perk) >= FMath::Max(1, Perk.MaxStacks))
	{
		return false;
	}
	return Perk.RequiredUnitTags.IsEmpty() || (Crew.Unit && Crew.Unit->UnitTags.HasAll(Perk.RequiredUnitTags));
}

TArray<const UDwPerkDefinition*> FDwProgressionRules::RollPerkChoices(const FDwCrewMemberState& Crew, const UDwPerkPool* Pool, int32 Count,
	FRandomStream& Random)
{
	TArray<const UDwPerkDefinition*> Candidates;
	if (Pool)
	{
		for (const UDwPerkDefinition* Perk : Pool->Perks)
		{
			if (Perk && Perk->Weight > 0.f && !Candidates.Contains(Perk) && CanTakePerk(Crew, *Perk))
			{
				Candidates.Add(Perk);
			}
		}
	}

	TArray<const UDwPerkDefinition*> Choices;
	while (Choices.Num() < Count && !Candidates.IsEmpty())
	{
		float Total = 0.f;
		for (const UDwPerkDefinition* Perk : Candidates)
		{
			Total += Perk->Weight;
		}
		float Roll = Random.GetFraction() * Total;
		int32 Pick = Candidates.Num() - 1;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			Roll -= Candidates[Index]->Weight;
			if (Roll < 0.f)
			{
				Pick = Index;
				break;
			}
		}
		Choices.Add(Candidates[Pick]);
		Candidates.RemoveAt(Pick);
	}
	return Choices;
}

bool FDwProgressionRules::ApplyPerk(FDwCrewMemberState& Crew, const UDwPerkDefinition& Perk)
{
	if (!CanTakePerk(Crew, Perk))
	{
		return false;
	}
	Crew.Perks.Add(&Perk);
	const UDwAbilityDefinition* Granted = Perk.GrantedAbility;
	if (Granted && !Crew.Abilities.ContainsByPredicate([Granted](const UDwAbilityDefinition* Known) { return Known == Granted; }))
	{
		Crew.Abilities.Add(Granted);
	}
	return true;
}
