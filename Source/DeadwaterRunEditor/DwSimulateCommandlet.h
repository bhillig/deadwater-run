// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DwSimulateCommandlet.generated.h"

/**
 * The headless run simulator's command line (Docs/Architecture/17, wrapped by Tools/Simulate.ps1):
 *
 *   UnrealEditor-Cmd DeadwaterRun.uproject -run=DwSimulate -Runs=1000 -Captain=All -Seed=1 -Policy=DA_SimPolicy_Default
 *       [-ImportTuning] [-Art] -Out=Saved/Sim/report.json -nullrhi -unattended -nosplash -nosound
 *
 * Plays -Runs seeds (Seed, Seed+1, ...) for each captain through UDwRunDriver, then writes report.json (summary + every
 * run) and summary.csv beside it. -ImportTuning first syncs every DT_*Tuning table from Tuning/*Tuning.json (and saves
 * them). -Art spawns the art Blueprints instead of the plain actors (slower, same results). Prints "DWSIM <line>" lines;
 * the first is "SIM PASS <runs> runs | win ... | avg m min | stuck 0" or "SIM FAIL ..." (any stuck run fails).
 * Exit code 0 only on PASS. Owner: tools-engineer.
 */
UCLASS()
class UDwSimulateCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UDwSimulateCommandlet();

	virtual int32 Main(const FString& Params) override;
};
