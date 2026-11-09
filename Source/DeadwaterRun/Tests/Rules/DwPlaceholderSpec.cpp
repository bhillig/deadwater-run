// Copyright Brandon Hillig 2026

// DW-003: proves the automation harness end to end (build -> headless run -> report -> Test.ps1 PASS/FAIL).
// Real rule specs replace this as systems land (DeadwaterRun.Rules.Damage, .Grid, .Outcome, ...).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDwPlaceholderSpec, "DeadwaterRun.Rules.Placeholder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDwPlaceholderSpec)

void FDwPlaceholderSpec::Define()
{
	Describe("Harness", [this]()
	{
		It("runs a pure test headless", [this]()
		{
			TestEqual(TEXT("Manhattan distance (3,4)->(0,0)"), FMath::Abs(3) + FMath::Abs(4), 7);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
