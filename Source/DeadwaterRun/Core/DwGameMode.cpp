// Copyright Brandon Hillig 2026

#include "Core/DwGameMode.h"

#include "Combat/DwCombatSubsystem.h"
#include "Combat/DwEncounterSetupBuilder.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Core/DwHUD.h"
#include "Core/DwLog.h"
#include "Core/DwPlayerController.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Presentation/DwPresentationSubsystem.h"
#include "Presentation/DwSeaEnvironment.h"
#include "Run/DwRun.h"
#include "Run/DwRunSubsystem.h"
#include "TimerManager.h"

ADwGameMode::ADwGameMode()
{
	PlayerControllerClass = ADwPlayerController::StaticClass();
	// The combat HUD is CommonUI (WBP_CombatHud, UDwUIManagerSubsystem); ADwHUD only draws the dev run readout.
	HUDClass = ADwHUD::StaticClass();

	// Units are board actors driven by commands, not possessed pawns; ADwCameraRig is the view target.
	DefaultPawnClass = nullptr;
}

void ADwGameMode::StartPlay()
{
	Super::StartPlay(); // every actor, including the player controller, has begun play

	UDwRunSubsystem* Runs = UDwRunSubsystem::Get(this);
	if (Runs)
	{
		Runs->OnRunStarted.AddDynamic(this, &ThisClass::HandleRunStarted);
	}
	int32 Seed = -1;
	FParse::Value(FCommandLine::Get(), TEXT("DwSeed="), Seed);

	if (Runs && Runs->GetRun())
	{
		BindRun(Runs->GetRun()); // arrived from the menu's Set Sail
		return;
	}
	FString CaptainName;
	if (Runs && FParse::Value(FCommandLine::Get(), TEXT("DwCaptain="), CaptainName))
	{
		const UDwCaptainDefinition* Captain = UDwRunSubsystem::FindCaptain(CaptainName);
		UE_CLOG(!Captain, LogDwRun, Error, TEXT("-DwCaptain=%s: no such captain in DA_GameData.Captains."), *CaptainName);
		if (Captain && Runs->StartRun(Captain, Seed, /*bOpenSeaLevel*/ false))
		{
			return; // HandleRunStarted binds it
		}
	}
	if (bStartDevFight)
	{
		StartDevFight(Seed);
	}
}

void ADwGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UDwRunSubsystem* Runs = UDwRunSubsystem::Get(this))
	{
		Runs->OnRunStarted.RemoveAll(this);
	}
	UnbindRun();
	GetWorldTimerManager().ClearTimer(PostCombatTimer);
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------------------------------------------------
// Run phase bridge

void ADwGameMode::HandleRunStarted(UDwRun* Run)
{
	BindRun(Run);
}

void ADwGameMode::BindRun(UDwRun* Run)
{
	UnbindRun();
	if (!Run)
	{
		return;
	}
	BoundRun = Run;
	Run->OnPhaseChanged.AddDynamic(this, &ThisClass::HandleRunPhaseChanged);
	if (UDwCombatSubsystem* Combat = GetWorld()->GetSubsystem<UDwCombatSubsystem>())
	{
		Combat->OnCombatEnded.AddDynamic(this, &ThisClass::HandleRunCombatEnded);
		Combat->EndEncounter(); // a dev fight from before the run started
	}
	if (Run->GetPhase() == EDwRunPhase::Combat)
	{
		StartRunEncounter();
	}
}

void ADwGameMode::UnbindRun()
{
	if (BoundRun)
	{
		BoundRun->OnPhaseChanged.RemoveAll(this);
	}
	if (UDwCombatSubsystem* Combat = GetWorld() ? GetWorld()->GetSubsystem<UDwCombatSubsystem>() : nullptr)
	{
		Combat->OnCombatEnded.RemoveAll(this);
	}
	BoundRun = nullptr;
	bRunEncounterActive = false;
}

void ADwGameMode::HandleRunPhaseChanged(EDwRunPhase NewPhase, EDwRunPhase OldPhase)
{
	if (NewPhase == EDwRunPhase::Combat)
	{
		StartRunEncounter();
	}
	else if (OldPhase == EDwRunPhase::Combat && bRunEncounterActive)
	{
		// Left mid-fight (abandoned from the pause menu): the fight's result no longer matters, clear the board.
		bRunEncounterActive = false;
		GetWorldTimerManager().ClearTimer(PostCombatTimer);
		if (UDwCombatSubsystem* Combat = GetWorld()->GetSubsystem<UDwCombatSubsystem>())
		{
			Combat->EndEncounter();
		}
	}
}

bool ADwGameMode::StartRunEncounter()
{
	UDwCombatSubsystem* Combat = GetWorld()->GetSubsystem<UDwCombatSubsystem>();
	if (!BoundRun || !Combat)
	{
		return false;
	}
	FDwEncounterSetup Setup;
	TArray<FString> Errors;
	if (!BoundRun->BuildEncounterSetup(Setup, Errors))
	{
		UE_LOG(LogDwRun, Error, TEXT("Node %d: no fight can start: %s"), BoundRun->GetState().CurrentNode, *FString::Join(Errors, TEXT("; ")));
		return false;
	}
	const UDwSeaMood* Mood = ApplySeaMood(Setup.Seed);
	UE_LOG(LogDwRun, Log, TEXT("Node %d fight: %s, seed %d, sea %s."), BoundRun->GetState().CurrentNode, *GetNameSafe(Setup.Encounter), Setup.Seed,
		*GetNameSafe(Mood));
	bRunEncounterActive = Combat->StartEncounter(Setup);
	return bRunEncounterActive;
}

void ADwGameMode::HandleRunCombatEnded(const FDwCombatResult& Result)
{
	if (!bRunEncounterActive || !BoundRun || BoundRun->GetPhase() != EDwRunPhase::Combat)
	{
		return;
	}
	bRunEncounterActive = false;
	PendingResult = Result;
	// Never tear the fight down inside its own OnCombatEnded broadcast (an ability may still be on the stack).
	const float Delay = UDwPresentationSubsystem::IsInstant(this) ? 0.f : PostCombatSeconds;
	if (Delay > 0.f)
	{
		GetWorldTimerManager().SetTimer(PostCombatTimer, FTimerDelegate::CreateUObject(this, &ThisClass::FinishRunEncounter), Delay, false);
	}
	else
	{
		PostCombatTimer = GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::FinishRunEncounter));
	}
}

void ADwGameMode::FinishRunEncounter()
{
	if (UDwCombatSubsystem* Combat = GetWorld()->GetSubsystem<UDwCombatSubsystem>())
	{
		Combat->EndEncounter();
	}
	// Last, so a listener that sails on at once (ChooseNode -> Combat) starts its fight on a clean board.
	if (BoundRun)
	{
		BoundRun->ApplyCombatResult(PendingResult);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Dev fight (no run)

bool ADwGameMode::StartDevFight(int32 Seed)
{
	const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
	UDwCombatSubsystem* Combat = GetWorld()->GetSubsystem<UDwCombatSubsystem>();
	if (!GameData || !GameData->DevEncounter || GameData->DevCrew.IsEmpty() || !Combat)
	{
		UE_LOG(LogDwCombat, Warning, TEXT("No dev fight: DA_GameData needs DevEncounter and DevCrew."));
		return false;
	}

	TArray<FDwCombatantSetup> Crew;
	for (const UDwUnitDefinition* Unit : GameData->DevCrew)
	{
		if (Unit)
		{
			FDwCombatantSetup& Member = Crew.AddDefaulted_GetRef();
			Member.Definition = Unit;
			Member.RosterId = FGuid::NewGuid();
			Member.bIsCaptain = Crew.Num() == 1; // the first entry captains the crew
		}
	}
	if (Seed < 0)
	{
		Seed = static_cast<int32>(FDateTime::UtcNow().GetTicks() & 0x7fffffff);
	}
	FDwEncounterSetup Setup;
	TArray<FString> Errors;
	if (!FDwEncounterSetupBuilder::Build(*GameData, *GameData->DevEncounter, Crew, Seed, Setup, Errors))
	{
		UE_LOG(LogDwCombat, Error, TEXT("Dev fight setup failed: %s"), *FString::Join(Errors, TEXT("; ")));
		return false;
	}
	for (const TPair<TObjectPtr<UDwItemDefinition>, int32>& Item : GameData->DevItems)
	{
		if (Item.Key && Item.Value > 0)
		{
			FDwItemStack& Stack = Setup.Items.AddDefaulted_GetRef();
			Stack.Item = Item.Key;
			Stack.Count = Item.Value;
		}
	}
	const UDwSeaMood* Mood = ApplySeaMood(Seed);
	UE_LOG(LogDwCombat, Log, TEXT("Dev fight: %s, seed %d, sea %s (replay with -DwSeed=%d -DwMood=%s)"), *GameData->DevEncounter->GetName(), Seed,
		*GetNameSafe(Mood), Seed, *GetNameSafe(Mood));
	return Combat->StartEncounter(Setup);
}

const UDwSeaMood* ADwGameMode::ApplySeaMood(int32 Seed)
{
	const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
	if (!GameData)
	{
		return nullptr;
	}
	FString Forced;
	FParse::Value(FCommandLine::Get(), TEXT("DwMood="), Forced);
	const UDwSeaMood* Mood = ADwSeaEnvironment::PickMood(GameData->SeaMoods, Seed, Forced);
	for (TActorIterator<ADwSeaEnvironment> It(GetWorld()); Mood && It; ++It)
	{
		It->ApplyMood(Mood);
	}
	return Mood;
}
