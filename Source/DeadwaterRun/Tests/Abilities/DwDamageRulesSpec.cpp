// Copyright Brandon Hillig 2026

// DW-101: FDwDamageRules table tests (Docs/Architecture/02 "How it's tested").

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/DwDamageRules.h"

BEGIN_DEFINE_SPEC(FDwDamageRulesSpec, "DeadwaterRun.Rules.Damage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDwDamageRulesSpec)

void FDwDamageRulesSpec::Define()
{
	struct FCase
	{
		const TCHAR* Name;
		FDwDamageInput Input;
		int32 Expected;
	};

	auto Make = [](float Flat, float Scale, float Attack, float Bonus, float Multiplier, float Reduction)
	{
		FDwDamageInput I;
		I.Flat = Flat; I.Scale = Scale; I.AttackDamage = Attack; I.DamageBonus = Bonus;
		I.SituationalMultiplier = Multiplier; I.DamageReduction = Reduction;
		return I;
	};

	It("follows the formula for every term", [this, Make]()
	{
		const FCase Cases[] = {
			{ TEXT("Flat only"),                       Make(20, 0, 0, 0, 1, 0),      20 },
			{ TEXT("Scaled attack"),                   Make(0, 1.5f, 10, 0, 1, 0),   15 },
			{ TEXT("Flat + scaled"),                   Make(20, 0.5f, 10, 0, 1, 0),  25 },
			{ TEXT("+20% bonus"),                      Make(20, 0.5f, 10, 0.2f, 1, 0), 30 },
			{ TEXT("Synergy x1.25"),                   Make(20, 0, 0, 0, 1.25f, 0),  25 },
			{ TEXT("50% reduction"),                   Make(20, 0.5f, 10, 0.2f, 1, 0.5f), 15 },
			{ TEXT("Reduction clamps at 90%"),         Make(100, 0, 0, 0, 1, 2.f),   10 },
			{ TEXT("Negative reduction ignored"),      Make(10, 0, 0, 0, 1, -1.f),   10 },
			{ TEXT("Rounds half away from zero"),      Make(5, 0, 0, 0, 1, 0.5f),    3 },
			{ TEXT("Rounds 0.7 up"),                   Make(7, 0, 0, 0, 1, 0.9f),    1 },
			{ TEXT("Rounds 0.4 down"),                 Make(4, 0, 0, 0, 1, 0.9f),    0 },
			{ TEXT("Never below zero"),                Make(10, 0, 0, -3.f, 1, 0),   0 },
			{ TEXT("Zero multiplier = no damage"),     Make(10, 0, 0, 0, 0, 0),      0 },
			{ TEXT("Kill bonus cap 20% on 15 x 6"),    Make(15, 0, 0, 0.2f, 1, 0),   18 },
		};
		for (const FCase& Case : Cases)
		{
			TestEqual(Case.Name, FDwDamageRules::Compute(Case.Input).Damage, Case.Expected);
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
