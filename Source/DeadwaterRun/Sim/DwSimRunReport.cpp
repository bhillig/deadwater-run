// Copyright Brandon Hillig 2026

#include "Sim/DwSimRunReport.h"

#include "JsonObjectConverter.h"

FDwSimSummary FDwSimReports::Summarize(TConstArrayView<FDwSimRunReport> Runs)
{
	FDwSimSummary S;
	S.Runs = Runs.Num();
	double Minutes = 0.0, GoldOnArrival = 0.0, CrewDeaths = 0.0;
	int32 Shops = 0, Fights = 0, Levels = 0, Rounds = 0, ReachedBossWithShop = 0, NoRepairBeforeBoss = 0;
	for (const FDwSimRunReport& Run : Runs)
	{
		S.Wins += Run.IsVictory() ? 1 : 0;
		S.Stuck += Run.IsStuck() ? 1 : 0;
		Minutes += Run.EstimatedMinutes;
		CrewDeaths += Run.CrewDeaths;
		Levels += Run.LevelsGained;
		S.FightsSunk += Run.FightsSunk;
		S.FightsPlundered += Run.FightsPlundered;
		S.FightsSurrendered += Run.FightsSurrendered;

		FDwSimCaptainSummary* Captain = S.Captains.FindByPredicate([&Run](const FDwSimCaptainSummary& C) { return C.Captain == Run.Captain; });
		if (!Captain)
		{
			Captain = &S.Captains.AddDefaulted_GetRef();
			Captain->Captain = Run.Captain;
		}
		++Captain->Runs;
		Captain->Wins += Run.IsVictory() ? 1 : 0;
		Captain->AvgMinutes += Run.EstimatedMinutes; // summed here, divided below

		if (Run.Result == TEXT("Defeat") && Run.LastLayer >= 0)
		{
			if (S.DefeatsByLayer.Num() <= Run.LastLayer)
			{
				S.DefeatsByLayer.SetNumZeroed(Run.LastLayer + 1);
			}
			++S.DefeatsByLayer[Run.LastLayer];
			++S.DefeatsByEncounter.FindOrAdd(Run.DiedTo.IsEmpty() ? FString(TEXT("(none)")) : Run.DiedTo);
		}
		for (const FDwSimFightReport& Fight : Run.Fights)
		{
			++Fights;
			Rounds += Fight.Rounds;
		}
		for (const FDwSimShopReport& Shop : Run.Shops)
		{
			++Shops;
			GoldOnArrival += Shop.GoldOnArrival;
		}
		const bool bReachedBoss = Run.IsVictory() || Run.DiedTo.Contains(TEXT("Kraken"));
		if (bReachedBoss && !Run.Shops.IsEmpty())
		{
			++ReachedBossWithShop;
			NoRepairBeforeBoss += Run.Shops.Last().bCouldAffordRepair ? 0 : 1;
		}
	}
	for (FDwSimCaptainSummary& Captain : S.Captains)
	{
		Captain.WinRatePct = Captain.Runs > 0 ? 100.f * Captain.Wins / Captain.Runs : 0.f;
		Captain.AvgMinutes = Captain.Runs > 0 ? Captain.AvgMinutes / Captain.Runs : 0.f;
	}
	S.AvgMinutes = S.Runs > 0 ? static_cast<float>(Minutes / S.Runs) : 0.f;
	S.AvgCrewDeaths = S.Runs > 0 ? static_cast<float>(CrewDeaths / S.Runs) : 0.f;
	S.AvgGoldOnShopArrival = Shops > 0 ? static_cast<float>(GoldOnArrival / Shops) : 0.f;
	S.PctNoRepairBeforeBoss = ReachedBossWithShop > 0 ? 100.f * NoRepairBeforeBoss / ReachedBossWithShop : 0.f;
	S.LevelsPerFight = Fights > 0 ? static_cast<float>(Levels) / Fights : 0.f;
	S.AvgRoundsPerFight = Fights > 0 ? static_cast<float>(Rounds) / Fights : 0.f;
	return S;
}

FString FDwSimReports::ToJson(const FDwSimReportFile& File)
{
	FString Json;
	FJsonObjectConverter::UStructToJsonObjectString(File, Json, /*CheckFlags*/ 0, /*SkipFlags*/ 0, /*Indent*/ 0, nullptr, /*bPrettyPrint*/ true);
	return Json;
}

FString FDwSimReports::ToCsv(TConstArrayView<FDwSimRunReport> Runs)
{
	TArray<FString> Lines;
	Lines.Add(TEXT("Seed,Captain,Policy,Result,EndReason,LastLayer,DiedTo,NodesCleared,Fights,Rounds,Kills,Sunk,Plundered,Surrendered,GoldEarned,GoldSpent,CrewDeaths,LevelsGained,EstimatedMinutes,StuckReason"));
	for (const FDwSimRunReport& R : Runs)
	{
		Lines.Add(FString::Printf(TEXT("%d,%s,%s,%s,%s,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.1f,\"%s\""), R.Seed, *R.Captain, *R.Policy, *R.Result,
			*R.EndReason, R.LastLayer, *R.DiedTo, R.NodesCleared, R.Fights.Num(), R.Rounds, R.Kills, R.FightsSunk, R.FightsPlundered,
			R.FightsSurrendered, R.GoldEarned, R.GoldSpent, R.CrewDeaths, R.LevelsGained, R.EstimatedMinutes, *R.StuckReason.Replace(TEXT("\""), TEXT("'"))));
	}
	return FString::Join(Lines, TEXT("\n")) + TEXT("\n");
}

FString FDwSimReports::OneLine(const FDwSimSummary& Summary)
{
	TArray<FString> Rates;
	for (const FDwSimCaptainSummary& Captain : Summary.Captains)
	{
		Rates.Add(FString::Printf(TEXT("%s %.0f%%"), *Captain.Captain, Captain.WinRatePct));
	}
	return FString::Printf(TEXT("%d runs | win %s | avg %.1f min | stuck %d"), Summary.Runs, *FString::Join(Rates, TEXT(" ")), Summary.AvgMinutes, Summary.Stuck);
}
