// Copyright Brandon Hillig 2026

// Development console commands that drive the run until the captain select and map screens exist (DW-210, DW-211).
// Not compiled into Shipping. Fight cheats (Dw.Win, Dw.Kill, ...) are the cheat manager's (DW-111).

#if !UE_BUILD_SHIPPING

#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Core/DwLog.h"
#include "Data/DwCaptainDefinition.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Run/DwRun.h"
#include "Run/DwRunSubsystem.h"

namespace DwRunDevCommands
{
	/** Dw.Run.Start [Captain] [Seed] - Captain by short name (TeamLeader, Fearmongerer, Conqueror); default the first. */
	static void Start(const TArray<FString>& Args, UWorld* World)
	{
		UDwRunSubsystem* Runs = UDwRunSubsystem::Get(World);
		const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
		const UDwCaptainDefinition* Captain = Args.Num() > 0 ? UDwRunSubsystem::FindCaptain(Args[0])
			: (GameData && !GameData->Captains.IsEmpty() ? GameData->Captains[0].Get() : nullptr);
		if (!Runs || !Captain)
		{
			UE_LOG(LogDwRun, Warning, TEXT("Dw.Run.Start: unknown captain '%s'."), Args.Num() > 0 ? *Args[0] : TEXT(""));
			return;
		}
		Runs->StartRun(Captain, Args.Num() > 1 ? FCString::Atoi(*Args[1]) : -1);
	}

	/** Dw.Run.Choose <n> - the n-th route the readout lists (1-based). */
	static void Choose(const TArray<FString>& Args, UWorld* World)
	{
		const UDwRunSubsystem* Runs = UDwRunSubsystem::Get(World);
		UDwRun* Run = Runs ? Runs->GetRun() : nullptr;
		const TArray<int32> Choices = Run ? Run->GetChoices() : TArray<int32>();
		const int32 Pick = Args.Num() > 0 ? FCString::Atoi(*Args[0]) - 1 : 0;
		if (!Choices.IsValidIndex(Pick))
		{
			UE_LOG(LogDwRun, Warning, TEXT("Dw.Run.Choose: no route %d (%d available)."), Pick + 1, Choices.Num());
			return;
		}
		Run->ChooseNode(Choices[Pick]);
	}

	static void Abandon(const TArray<FString>& /*Args*/, UWorld* World)
	{
		if (UDwRunSubsystem* Runs = UDwRunSubsystem::Get(World))
		{
			Runs->AbandonRun();
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs StartCommand(TEXT("Dw.Run.Start"),
		TEXT("Dw.Run.Start [TeamLeader|Fearmongerer|Conqueror] [Seed]: Set Sail with a new run."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Start));
	static FAutoConsoleCommandWithWorldAndArgs ChooseCommand(TEXT("Dw.Run.Choose"),
		TEXT("Dw.Run.Choose <n>: sail to the n-th route on the run readout."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Choose));
	static FAutoConsoleCommandWithWorldAndArgs AbandonCommand(TEXT("Dw.Run.Abandon"),
		TEXT("Dw.Run.Abandon: end the run as abandoned."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Abandon));
}

#endif // !UE_BUILD_SHIPPING
