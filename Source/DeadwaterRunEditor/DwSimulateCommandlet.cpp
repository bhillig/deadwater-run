// Copyright Brandon Hillig 2026

#include "DwSimulateCommandlet.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Core/DwLog.h"
#include "Data/DwCaptainDefinition.h"
#include "DwCombatContentLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Run/DwRunSubsystem.h"
#include "Sim/DwRunDriver.h"
#include "Sim/DwSimPolicy.h"
#include "UObject/UObjectGlobals.h"

namespace DwSimulate
{
	const FString PolicyFolder = TEXT("/Game/DeadwaterRun/Data/Sim");

	/** Collect garbage every this many runs (each run leaves a UDwRun and a few fights' actors behind). */
	constexpr int32 GcEvery = 20;

	void Print(const FString& Line)
	{
		UE_LOG(LogDwSim, Display, TEXT("DWSIM %s"), *Line);
	}

	int32 Fail(const FString& Why)
	{
		Print(TEXT("SIM FAIL (") + Why + TEXT(")"));
		return 1;
	}
}

UDwSimulateCommandlet::UDwSimulateCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UDwSimulateCommandlet::Main(const FString& Params)
{
	using namespace DwSimulate;
	int32 RunsPerCaptain = 100;
	int32 FirstSeed = 1;
	FString CaptainArg = TEXT("All");
	FString PolicyName = TEXT("DA_SimPolicy_Default");
	FString Out = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Sim"), TEXT("report.json"));
	FParse::Value(*Params, TEXT("Runs="), RunsPerCaptain);
	FParse::Value(*Params, TEXT("Seed="), FirstSeed);
	FParse::Value(*Params, TEXT("Captain="), CaptainArg);
	FParse::Value(*Params, TEXT("Policy="), PolicyName);
	FParse::Value(*Params, TEXT("Out="), Out);
	const bool bImportTuning = FParse::Param(*Params, TEXT("ImportTuning"));
	const bool bArt = FParse::Param(*Params, TEXT("Art"));
	Out = FPaths::ConvertRelativePathToFull(FPaths::IsRelative(Out) ? FPaths::Combine(FPaths::ProjectDir(), Out) : Out);
	if (RunsPerCaptain < 1)
	{
		return Fail(TEXT("-Runs must be at least 1"));
	}

	// The asset manager lists definitions once the registry has finished its scan.
	IAssetRegistry::GetChecked().SearchAllAssets(/*bSynchronousSearch*/ true);

	if (bImportTuning)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Tuning"), TEXT("*Tuning.json")), true, false);
		Files.Sort();
		TArray<FString> Log;
		bool bOk = true;
		for (const FString& File : Files)
		{
			bOk &= UDwCombatContentLibrary::SyncTuningTable(File.LeftChop(FString(TEXT("Tuning.json")).Len()), Log);
		}
		for (const FString& Line : Log)
		{
			Print(TEXT("  ") + Line);
		}
		if (!bOk)
		{
			return Fail(TEXT("tuning import failed"));
		}
	}

	const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
	if (!GameData)
	{
		return Fail(TEXT("no DA_GameData"));
	}
	if (UDwRunSubsystem::GatherEncounterPool().IsEmpty())
	{
		return Fail(TEXT("no encounters: the asset manager has not loaded the definitions"));
	}
	const UDwSimPolicy* Policy = LoadObject<UDwSimPolicy>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *PolicyFolder, *PolicyName, *PolicyName));
	if (!Policy)
	{
		return Fail(FString::Printf(TEXT("no policy %s/%s"), *PolicyFolder, *PolicyName));
	}
	TArray<const UDwCaptainDefinition*> Captains;
	for (const UDwCaptainDefinition* Captain : GameData->Captains)
	{
		if (Captain && (CaptainArg == TEXT("All") || UDwRunDriver::CaptainName(*Captain) == CaptainArg))
		{
			Captains.Add(Captain);
		}
	}
	if (Captains.IsEmpty())
	{
		return Fail(TEXT("no captain matches -Captain=") + CaptainArg);
	}

	UDwRunDriver* Driver = NewObject<UDwRunDriver>();
	Driver->AddToRoot();
	Driver->bPlainActors = !bArt;
	Driver->Initialize();

	const double Start = FPlatformTime::Seconds();
	FDwSimReportFile File;
	File.Policy = Policy->GetName();
	File.FirstSeed = FirstSeed;
	const int32 Total = RunsPerCaptain * Captains.Num();
	for (int32 Index = 0; Index < RunsPerCaptain; ++Index)
	{
		for (const UDwCaptainDefinition* Captain : Captains)
		{
			File.Runs.Add(Driver->PlayRun(*GameData, *Captain, FirstSeed + Index, *Policy));
			if (File.Runs.Num() % GcEvery == 0)
			{
				CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
			}
			if (File.Runs.Num() % 100 == 0)
			{
				UE_LOG(LogDwSim, Display, TEXT("%d / %d runs, %.0f s"), File.Runs.Num(), Total, FPlatformTime::Seconds() - Start);
			}
		}
	}
	const double Seconds = FPlatformTime::Seconds() - Start;
	Driver->Shutdown();
	Driver->RemoveFromRoot();

	File.Summary = FDwSimReports::Summarize(File.Runs);
	const FString Csv = FPaths::Combine(FPaths::GetPath(Out), TEXT("summary.csv"));
	if (!FFileHelper::SaveStringToFile(FDwSimReports::ToJson(File), *Out, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
		|| !FFileHelper::SaveStringToFile(FDwSimReports::ToCsv(File.Runs), *Csv, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return Fail(TEXT("could not write ") + Out);
	}

	const FDwSimRunReport* FirstStuck = File.Runs.FindByPredicate([](const FDwSimRunReport& R) { return R.IsStuck(); });
	int32 Rejected = 0;
	for (const FDwSimRunReport& Run : File.Runs)
	{
		for (const FDwSimFightReport& Fight : Run.Fights)
		{
			Rejected += Fight.RejectedCommands;
		}
	}
	const FString Line = FDwSimReports::OneLine(File.Summary);
	Print(FirstStuck ? FString::Printf(TEXT("SIM FAIL %s (first: %s seed %d: %s)"), *Line, *FirstStuck->Captain, FirstStuck->Seed, *FirstStuck->StuckReason)
		: TEXT("SIM PASS ") + Line);
	Print(FString::Printf(TEXT("  time %.1f s (%.0f ms per run), policy %s, seeds %d-%d, %s actors"), Seconds, 1000.0 * Seconds / Total,
		*Policy->GetName(), FirstSeed, FirstSeed + RunsPerCaptain - 1, bArt ? TEXT("art") : TEXT("plain")));
	Print(FString::Printf(TEXT("  outcomes: %d sunk, %d plundered, %d surrendered | %.2f rounds per fight | %.2f levels per fight | %.2f crew lost per run"),
		File.Summary.FightsSunk, File.Summary.FightsPlundered, File.Summary.FightsSurrendered, File.Summary.AvgRoundsPerFight,
		File.Summary.LevelsPerFight, File.Summary.AvgCrewDeaths));
	TArray<FString> Layers;
	for (int32 Layer = 0; Layer < File.Summary.DefeatsByLayer.Num(); ++Layer)
	{
		Layers.Add(FString::Printf(TEXT("%d:%d"), Layer, File.Summary.DefeatsByLayer[Layer]));
	}
	Print(TEXT("  defeats by layer: ") + (Layers.IsEmpty() ? FString(TEXT("none")) : FString::Join(Layers, TEXT(" "))));
	if (Rejected > 0)
	{
		Print(FString::Printf(TEXT("  WARNING %d player commands were refused by the rules (brain/rules mismatch, see the log)"), Rejected));
	}
	Print(TEXT("  report: ") + Out);
	Print(TEXT("  csv: ") + Csv);
	return FirstStuck ? 1 : 0;
}
