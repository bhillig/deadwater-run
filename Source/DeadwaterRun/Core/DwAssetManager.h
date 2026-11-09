// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "DwAssetManager.generated.h"

class UDwGameData;
struct FStreamableHandle;

/**
 * Loads every Deadwater definition at startup (the data set is small) and serves typed lookups.
 * Meshes, textures and sounds inside definitions are soft references, loaded on demand.
 * Set as AssetManagerClassName in DefaultEngine.ini. Docs/Architecture/13-Data-Layout.md.
 */
UCLASS()
class DEADWATERRUN_API UDwAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	static UDwAssetManager& Get();

	/** DA_GameData. Loads it synchronously on first use; nullptr (with an error log) if the asset is missing. */
	const UDwGameData* GetGameData();

	/** Every loaded definition of one primary asset type, e.g. GetDefinitions<UDwCaptainDefinition>(UDwDefinitionBase::FTypes::Captain). */
	template <typename T>
	TArray<T*> GetDefinitions(FPrimaryAssetType Type) const
	{
		TArray<UObject*> Objects;
		GetPrimaryAssetObjectList(Type, Objects);

		TArray<T*> Result;
		Result.Reserve(Objects.Num());
		for (UObject* Object : Objects)
		{
			if (T* Typed = Cast<T>(Object))
			{
				Result.Add(Typed);
			}
		}
		return Result;
	}

protected:
	virtual void StartInitialLoading() override;

private:
	void LoadAllDefinitions();

	UPROPERTY(Transient)
	TObjectPtr<const UDwGameData> GameData;

	/** Keeps the startup loads resident for the whole session. */
	TArray<TSharedPtr<FStreamableHandle>> DefinitionHandles;
};
