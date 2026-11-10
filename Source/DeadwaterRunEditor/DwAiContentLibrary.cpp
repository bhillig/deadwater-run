// Copyright Brandon Hillig 2026

#include "DwAiContentLibrary.h"

#include "AI/DwAiProfile.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Core/DwGameData.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/Tuning/DwAiTuningRow.h"
#include "DwCombatContentLibrary.h"
#include "EditorAssetLibrary.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"

namespace DwAiContent
{
	const FString Data = TEXT("/Game/DeadwaterRun/Data");
	const FString AiData = TEXT("/Game/DeadwaterRun/Data/AI");
	const FString Tuning = TEXT("/Game/DeadwaterRun/Data/Tuning");
	const FString EncounterData = TEXT("/Game/DeadwaterRun/Data/Encounters");

	template <typename T>
	T* Load(const FString& Folder, const FString& Name)
	{
		return LoadObject<T>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Folder, *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	template <typename T>
	T* FindOrCreate(const FString& Folder, const FString& Name, TArray<FString>& Log)
	{
		if (T* Existing = Load<T>(Folder, Name))
		{
			Log.Add(FString::Printf(TEXT("Updated %s/%s"), *Folder, *Name));
			return Existing;
		}
		UPackage* Package = CreatePackage(*(Folder / Name));
		T* Asset = NewObject<T>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Asset);
		Log.Add(FString::Printf(TEXT("Created %s/%s"), *Folder, *Name));
		return Asset;
	}

	bool Save(UObject* Asset, TArray<FString>& Log)
	{
		Asset->MarkPackageDirty();
		if (!UEditorAssetLibrary::SaveLoadedAsset(Asset, /*bOnlyIfIsDirty*/ false))
		{
			Log.Add(FString::Printf(TEXT("ERROR could not save %s"), *Asset->GetPathName()));
			return false;
		}
		return true;
	}
}

TArray<FString> UDwAiContentLibrary::CreateAiContent()
{
	using namespace DwAiContent;
	TArray<FString> Log;
	bool bOk = true;

	// 1. DT_AiTuning, filled from Tuning/AiTuning.json (the Balance Designer's source of truth).
	UDataTable* Table = FindOrCreate<UDataTable>(Tuning, TEXT("DT_AiTuning"), Log);
	if (Table->RowStruct != FDwAiTuningRow::StaticStruct())
	{
		Table->RowStruct = FDwAiTuningRow::StaticStruct();
		bOk &= Save(Table, Log);
	}
	bOk &= UDwCombatContentLibrary::SyncTuningTable(TEXT("Ai"), Log);
	if (!Table->GetRowMap().Contains(TEXT("Default")))
	{
		Log.Add(TEXT("ERROR Tuning/AiTuning.json has no Default row"));
		Log.Add(TEXT("CONTENT FAIL"));
		return Log;
	}

	// 2. The default crew profile: no cannon or boarding preference (crew personalities are Open-Questions N2).
	UDwAiProfile* Profile = FindOrCreate<UDwAiProfile>(AiData, TEXT("DA_AiProfile_Default"), Log);
	Profile->DisplayName = FText::FromString(TEXT("Default crew"));
	Profile->Description = FText::FromString(TEXT("Placeholder personality until the crews are designed: fights where it can, focuses the wounded, avoids open danger."));
	Profile->Tuning.DataTable = Table;
	Profile->Tuning.RowName = TEXT("Default");
	Profile->bPreferCannons = false;
	Profile->bBoarders = false;
	bOk &= Save(Profile, Log);

	// 3. The placeholder crew uses it, so the dev fight in L_Sea has an enemy that plays.
	if (UDwEncounterDefinition* Encounter = Load<UDwEncounterDefinition>(EncounterData, TEXT("DA_Encounter_Placeholder")))
	{
		Encounter->AiProfile = Profile;
		bOk &= Save(Encounter, Log);
		Log.Add(TEXT("DA_Encounter_Placeholder.AiProfile = DA_AiProfile_Default"));
	}
	else
	{
		Log.Add(TEXT("ERROR DA_Encounter_Placeholder is missing (run the combat content first)"));
		bOk = false;
	}

	// 4. DA_GameData lists every tuning table.
	if (UDwGameData* GameData = Load<UDwGameData>(Data, UDwGameData::AssetName.ToString()))
	{
		GameData->TuningTables.Ai = Table;
		bOk &= Save(GameData, Log);
	}
	else
	{
		Log.Add(TEXT("ERROR DA_GameData is missing"));
		bOk = false;
	}

	Log.Add(bOk ? TEXT("CONTENT PASS") : TEXT("CONTENT FAIL"));
	return Log;
}
