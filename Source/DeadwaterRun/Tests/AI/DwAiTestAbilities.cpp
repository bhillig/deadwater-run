// Copyright Brandon Hillig 2026

#include "Tests/AI/DwAiTestAbilities.h"

#include "Abilities/DwGameplayEffects.h"
#include "Core/DwGameplayTags.h"

UDwAiTestPayload_Rifle::UDwAiTestPayload_Rifle()
{
	Targeting.Pattern = EDwTargetPattern::SingleUnit;
	Targeting.Side = EDwTargetSide::Enemy;
	Targeting.bRequiresLineOfSight = true;
	FDwEffectApplication Damage;
	Damage.Effect = UDwGE_Damage::StaticClass();
	Applications.Add(Damage);
	HitCue = DwTags::GameplayCue_Hit_Ranged;
}

UDwAiTestPayload_Heal::UDwAiTestPayload_Heal()
{
	Targeting.Pattern = EDwTargetPattern::SingleUnit;
	Targeting.Side = EDwTargetSide::Ally;
	FDwEffectApplication Heal;
	Heal.Effect = UDwGE_Heal::StaticClass();
	Applications.Add(Heal);
	HitCue = DwTags::GameplayCue_Unit_Heal;
}
