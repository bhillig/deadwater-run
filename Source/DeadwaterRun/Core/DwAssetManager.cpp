// Copyright Brandon Hillig 2026

#include "Core/DwAssetManager.h"

#include "Core/DwGameData.h"
#include "Core/DwLog.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"

UDwAssetManager& UDwAssetManager::Get()
{
	check(GEngine);
	UDwAssetManager* Manager = Cast<UDwAssetManager>(GEngine->AssetManager);
	checkf(Manager, TEXT("AssetManagerClassName must be /Script/DeadwaterRun.DwAssetManager (Config/DefaultEngine.ini)."));
	return *Manager;
}

void UDwAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	// In the editor the asset registry is still scanning here, so primary asset lists are empty until the scan ends.
	// Runs immediately in cooked builds, after the scan in the editor.
	CallOrRegister_OnCompletedInitialScan(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::LoadAllDefinitions));
}

void UDwAssetManager::LoadAllDefinitions()
{
	int32 TypeCount = 0;
	for (const FPrimaryAssetType& Type : UDwDefinitionBase::FTypes::All())
	{
		TArray<FPrimaryAssetId> Ids;
		if (!GetPrimaryAssetIdList(Type, Ids) || Ids.IsEmpty())
		{
			continue;
		}

		if (TSharedPtr<FStreamableHandle> Handle = LoadPrimaryAssets(Ids))
		{
			Handle->WaitUntilComplete();
			DefinitionHandles.Add(Handle);
		}
		++TypeCount;
	}
	UE_LOG(LogDwData, Log, TEXT("UDwAssetManager loaded definitions for %d primary asset types."), TypeCount);
}

const UDwGameData* UDwAssetManager::GetGameData()
{
	if (!GameData)
	{
		const FPrimaryAssetId Id(UDwDefinitionBase::FTypes::GameData, UDwGameData::AssetName);
		const FSoftObjectPath Path = GetPrimaryAssetPath(Id);
		GameData = Cast<UDwGameData>(Path.TryLoad());
		UE_CLOG(!GameData, LogDwData, Error, TEXT("Primary asset %s not found. Create /Game/DeadwaterRun/Data/DA_GameData."), *Id.ToString());
	}
	return GameData;
}
