// Copyright Brandon Hillig 2026

// DW-212: the headless run simulator (Docs/Architecture/17). Whole runs of the shipped game - real UDwRun, combat, GAS,
// crew AI on both sides - played by UDwRunDriver with DA_SimPolicy_Default.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Data/DwCaptainDefinition.h"
#include "Misc/DataValidation.h"
#include "Sim/DwRunDriver.h"
#include "Sim/DwSimPolicy.h"
#include "Sim/DwSimRunReport.h"

namespace DwSimTest
{
	const UDwSimPolicy* LoadPolicy()
	{
		return LoadObject<UDwSimPolicy>(nullptr, TEXT("/Game/DeadwaterRun/Data/Sim/DA_SimPolicy_Default.DA_SimPolicy_Default"));
	}
}

BEGIN_DEFINE_SPEC(FDwSimSpec, "DeadwaterRun.Sim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	UDwRunDriver* Driver = nullptr;
	const UDwGameData* GameData = nullptr;
	const UDwSimPolicy* Policy = nullptr;

	bool Ready()
	{
		return TestNotNull(TEXT("DA_GameData"), GameData) && TestNotNull(TEXT("DA_SimPolicy_Default"), Policy)
			&& TestTrue(TEXT("Captains"), GameData->Captains.Num() > 0);
	}

	/** One run on a fresh driver (fresh world): nothing carries over between the runs compared. */
	FDwSimRunReport PlayFresh(const UDwCaptainDefinition& Captain, int32 Seed, bool bPlainActors = true)
	{
		UDwRunDriver* Fresh = NewObject<UDwRunDriver>();
		Fresh->AddToRoot();
		Fresh->bPlainActors = bPlainActors;
		const FDwSimRunReport Report = Fresh->PlayRun(*GameData, Captain, Seed, *Policy);
		Fresh->Shutdown();
		Fresh->RemoveFromRoot();
		return Report;
	}

	static FString Json(const FDwSimRunReport& Report)
	{
		FDwSimReportFile File;
		File.Runs.Add(Report);
		File.Summary = FDwSimReports::Summarize(File.Runs);
		return FDwSimReports::ToJson(File);
	}

END_DEFINE_SPEC(FDwSimSpec)

void FDwSimSpec::Define()
{
	BeforeEach([this]()
	{
		GameData = UDwAssetManager::Get().GetGameData();
		Policy = DwSimTest::LoadPolicy();
		Driver = NewObject<UDwRunDriver>();
		Driver->AddToRoot();
	});
	AfterEach([this]()
	{
		Driver->Shutdown();
		Driver->RemoveFromRoot();
		Driver = nullptr;
	});

	Describe("Smoke", [this]()
	{
		It("ships a valid DA_SimPolicy_Default with a player profile", [this]()
		{
			if (!TestNotNull(TEXT("DA_SimPolicy_Default"), Policy)) { return; }
#if WITH_EDITOR
			FDataValidationContext Context;
			TestEqual(TEXT("Valid"), Policy->IsDataValid(Context), EDataValidationResult::Valid);
#endif
			TestNotNull(TEXT("PlayerProfile"), Policy->PlayerProfile.Get());
		});

		It("plays a whole run for every captain to Victory or Defeat, never stuck, every command legal", [this]()
		{
			if (!Ready()) { return; }
			TArray<FDwSimRunReport> Runs;
			for (const UDwCaptainDefinition* Captain : GameData->Captains)
			{
				const FDwSimRunReport R = Driver->PlayRun(*GameData, *Captain, 1, *Policy);
				Runs.Add(R);
				const FString What = R.Captain;
				AddInfo(FString::Printf(TEXT("%s seed 1: %s (%s) at layer %d %s, %d fights, %d kills, %d crew lost, %d levels, %.1f min"), *What, *R.Result,
					*R.EndReason, R.LastLayer, *R.DiedTo, R.Fights.Num(), R.Kills, R.CrewDeaths, R.LevelsGained, R.EstimatedMinutes));
				TestFalse(What + TEXT(" is not stuck: ") + R.StuckReason, R.IsStuck());
				TestTrue(What + TEXT(" ended in Victory or Defeat"), R.Result == TEXT("Victory") || R.Result == TEXT("Defeat"));
				TestTrue(What + TEXT(" fought"), R.Fights.Num() > 0);
				TestTrue(What + TEXT(" has an end node"), R.LastLayer >= 0);
				TestTrue(What + TEXT(" has an estimate"), R.EstimatedMinutes > 0.f);
				for (const FDwSimFightReport& Fight : R.Fights)
				{
					TestEqual(FString::Printf(TEXT("%s node %d: no refused commands"), *What, Fight.Node), Fight.RejectedCommands, 0);
					TestTrue(FString::Printf(TEXT("%s node %d: the crew acted"), *What, Fight.Node), Fight.PlayerActions > 0);
				}
				if (R.Result == TEXT("Defeat"))
				{
					TestFalse(What + TEXT(" names what it died to"), R.DiedTo.IsEmpty());
				}
			}
			const FDwSimSummary Summary = FDwSimReports::Summarize(Runs);
			AddInfo(FDwSimReports::OneLine(Summary));
			TestEqual(TEXT("Summary counts every run"), Summary.Runs, Runs.Num());
			TestEqual(TEXT("One line per captain"), Summary.Captains.Num(), GameData->Captains.Num());
			TestEqual(TEXT("Nothing stuck"), Summary.Stuck, 0);
		});
	});

	Describe("Determinism", [this]()
	{
		It("gives a byte-identical report for the same seed, captain and policy", [this]()
		{
			if (!Ready()) { return; }
			const UDwCaptainDefinition& Captain = *GameData->Captains[0];
			const FString First = Json(PlayFresh(Captain, 4));
			const FString Second = Json(PlayFresh(Captain, 4));
			TestTrue(TEXT("Report written"), First.Len() > 100);
			TestEqual(TEXT("Same bytes"), First, Second);
			TestNotEqual(TEXT("Another seed plays another run"), Json(PlayFresh(Captain, 5)), First);
		});

		It("gives the same report on one driver as on fresh ones (nothing leaks between runs)", [this]()
		{
			if (!Ready()) { return; }
			const UDwCaptainDefinition& Captain = *GameData->Captains[0];
			Driver->PlayRun(*GameData, Captain, 9, *Policy); // a run before, on the same world
			TestEqual(TEXT("Same bytes"), Json(Driver->PlayRun(*GameData, Captain, 4, *Policy)), Json(PlayFresh(Captain, 4)));
		});

		It("gives the same report with the art Blueprints as with the plain actors", [this]()
		{
			if (!Ready()) { return; }
			const UDwCaptainDefinition& Captain = *GameData->Captains[0];
			TestEqual(TEXT("Same bytes"), Json(PlayFresh(Captain, 4, /*bPlainActors*/ false)), Json(PlayFresh(Captain, 4, /*bPlainActors*/ true)));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
