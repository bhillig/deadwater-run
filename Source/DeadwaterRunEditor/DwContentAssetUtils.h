// Copyright Brandon Hillig 2026

#pragma once

// Find-or-create / save helpers for the headless content libraries (UDw*ContentLibrary). Every line they log goes to
// the content script's output; "ERROR ..." lines fail the run. New libraries use these; the combat, AI, art and UI
// libraries still carry private copies of the same helpers (migrating them is a tools-engineer cleanup).

#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataTable.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"

namespace DwContentAssets
{
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

	/** Null (with an ERROR line) if an asset of that name exists with another parent class. */
	inline UBlueprint* FindOrCreateBlueprint(const FString& Folder, const FString& Name, UClass* Parent, TArray<FString>& Log)
	{
		if (UBlueprint* Existing = Load<UBlueprint>(Folder, Name))
		{
			if (Existing->ParentClass == Parent)
			{
				Log.Add(FString::Printf(TEXT("Updated %s/%s"), *Folder, *Name));
				return Existing;
			}
			Log.Add(FString::Printf(TEXT("ERROR %s/%s exists with parent %s, expected %s"), *Folder, *Name, *GetNameSafe(Existing->ParentClass), *Parent->GetName()));
			return nullptr;
		}
		UPackage* Package = CreatePackage(*(Folder / Name));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(Parent, Package, *Name, BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_None);
		FAssetRegistryModule::AssetCreated(Blueprint);
		Log.Add(FString::Printf(TEXT("Created %s/%s (parent %s)"), *Folder, *Name, *Parent->GetName()));
		return Blueprint;
	}

	template <typename T>
	T* Defaults(UBlueprint* Blueprint)
	{
		return Blueprint && Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject<T>() : nullptr;
	}

	inline bool Save(UObject* Asset, TArray<FString>& Log)
	{
		if (!Asset)
		{
			return false;
		}
		Asset->MarkPackageDirty();
		if (!UEditorAssetLibrary::SaveLoadedAsset(Asset, /*bOnlyIfIsDirty*/ false))
		{
			Log.Add(FString::Printf(TEXT("ERROR could not save %s"), *Asset->GetPathName()));
			return false;
		}
		return true;
	}

	/** A handle to Table's row Name; logs an ERROR and clears bOk if the row doesn't exist. */
	inline FDataTableRowHandle Row(UDataTable* Table, const TCHAR* Name, TArray<FString>& Log, bool& bOk)
	{
		FDataTableRowHandle Handle;
		Handle.DataTable = Table;
		Handle.RowName = Name;
		if (!Table || !Table->GetRowMap().Contains(Handle.RowName))
		{
			Log.Add(FString::Printf(TEXT("ERROR %s has no row %s"), *GetNameSafe(Table), Name));
			bOk = false;
		}
		return Handle;
	}
}
