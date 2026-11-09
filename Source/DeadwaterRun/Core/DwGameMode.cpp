// Copyright Brandon Hillig 2026

#include "Core/DwGameMode.h"

#include "Combat/DwCombatSubsystem.h"
#include "Combat/DwEncounterSetupBuilder.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Core/DwLog.h"
#include "Core/DwPlaceholderHUD.h"
#include "Core/DwPlayerController.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ADwGameMode::ADwGameMode()
{
	PlayerControllerClass = ADwPlayerController::StaticClass();
	HUDClass = ADwPlaceholderHUD::StaticClass(); // DW-107 swaps in the CommonUI HUD

	// Units are board actors driven by commands, not possessed pawns; ADwCameraRig is the view target.
	DefaultPawnClass = nullptr;
}

void ADwGameMode::StartPlay()
{
	Super::StartPlay(); // every actor, including the player controller, has begun play
	if (bStartDevFight)
	{
		int32 Seed = -1;
		FParse::Value(FCommandLine::Get(), TEXT("DwSeed="), Seed);
		StartDevFight(Seed);
	}
}

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
	if (!FDwEncounterSetupBuilder::BuildShipBattle(*GameData, *GameData->DevEncounter, Crew, Seed, Setup, Errors))
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
	UE_LOG(LogDwCombat, Log, TEXT("Dev fight: %s, seed %d (replay with -DwSeed=%d)"), *GameData->DevEncounter->GetName(), Seed, Seed);
	return Combat->StartEncounter(Setup);
}
