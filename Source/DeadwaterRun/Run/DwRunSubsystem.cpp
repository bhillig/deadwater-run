// Copyright Brandon Hillig 2026

#include "Run/DwRunSubsystem.h"

#include "Core/DwAssetManager.h"
#include "Core/DwDefinitionBase.h"
#include "Core/DwGameData.h"
#include "Core/DwLog.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwEncounterDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Run/DwRun.h"
#include "Run/DwRunRandom.h"

UDwRunSubsystem* UDwRunSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UDwRunSubsystem>() : nullptr;
}

UDwRun* UDwRunSubsystem::StartRun(const UDwCaptainDefinition* Captain, int32 Seed, bool bOpenSeaLevel)
{
	const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
	if (!Captain || !GameData)
	{
		UE_LOG(LogDwRun, Error, TEXT("StartRun: %s."), !Captain ? TEXT("no captain") : TEXT("DA_GameData is missing"));
		return nullptr;
	}
	ClearRun();

	if (Seed < 0)
	{
		Seed = FDwRunRandom::MakeRunSeed();
	}
	UDwRun* NewRun = NewObject<UDwRun>(this);
	TArray<FString> Errors;
	if (!NewRun->Initialize(*GameData, *Captain, Seed, GatherEncounterPool(), &Errors))
	{
		UE_LOG(LogDwRun, Error, TEXT("StartRun(%s, %d) failed: %s"), *Captain->GetName(), Seed, *FString::Join(Errors, TEXT("; ")));
		return nullptr;
	}
	Run = NewRun;
	StartSeconds = FPlatformTime::Seconds();
	Run->OnPhaseChanged.AddDynamic(this, &ThisClass::HandlePhaseChanged);
	UE_LOG(LogDwRun, Log, TEXT("Set Sail with %s, seed %d (replay with -DwCaptain=%s -DwSeed=%d)."), *Captain->GetName(), Seed, *Captain->GetName(), Seed);
	OnRunStarted.Broadcast(Run);

	// One travel per run (16-Shell D1): from the menu into L_Sea. Already in L_Sea (PIE, -DwCaptain) = no travel.
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (bOpenSeaLevel && World && !GameData->SeaLevel.IsNull())
	{
		const FString Current = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
		if (Current != GameData->SeaLevel.GetLongPackageName())
		{
			UGameplayStatics::OpenLevelBySoftObjectPtr(World, GameData->SeaLevel);
		}
	}
	return Run;
}

bool UDwRunSubsystem::AbandonRun()
{
	return Run && Run->Abandon();
}

void UDwRunSubsystem::ClearRun()
{
	if (!Run)
	{
		return;
	}
	Run->OnPhaseChanged.RemoveAll(this);
	Run = nullptr;
	OnRunCleared.Broadcast();
}

void UDwRunSubsystem::Deinitialize()
{
	ClearRun();
	Super::Deinitialize();
}

void UDwRunSubsystem::HandlePhaseChanged(EDwRunPhase NewPhase, EDwRunPhase /*OldPhase*/)
{
	if (Run && Run->IsOver())
	{
		Run->SetElapsedSeconds(static_cast<float>(FPlatformTime::Seconds() - StartSeconds));
	}
}

const UDwCaptainDefinition* UDwRunSubsystem::FindCaptain(const FString& Name)
{
	const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
	if (!GameData || Name.IsEmpty())
	{
		return nullptr;
	}
	for (const UDwCaptainDefinition* Captain : GameData->Captains)
	{
		if (Captain && (Captain->GetName() == Name || Captain->GetName() == TEXT("DA_Captain_") + Name))
		{
			return Captain;
		}
	}
	return nullptr;
}

TArray<const UDwEncounterDefinition*> UDwRunSubsystem::GatherEncounterPool()
{
	TArray<const UDwEncounterDefinition*> Pool;
	for (const UDwEncounterDefinition* Encounter : UDwAssetManager::Get().GetDefinitions<UDwEncounterDefinition>(UDwDefinitionBase::FTypes::Encounter))
	{
		if (Encounter && Encounter->Kind == EDwEncounterKind::ShipBattle)
		{
			Pool.Add(Encounter);
		}
	}
	Pool.Sort([](const UDwEncounterDefinition& A, const UDwEncounterDefinition& B) { return A.GetFName().LexicalLess(B.GetFName()); });
	return Pool;
}
