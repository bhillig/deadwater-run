// Copyright Brandon Hillig 2026

// UI art pass: every player-facing ability, item, perk, ship upgrade, captain and unit has an icon or portrait that
// loads (Tools/Content/ui_icon_map.json applied by Tools/CreateUIContent.ps1), and each crew/raider look has its face.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "Combat/DwCombatUnit.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwPerkDefinition.h"
#include "Data/DwShipUpgradeDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Engine/Texture2D.h"

BEGIN_DEFINE_SPEC(FDwUIArtSpec, "DeadwaterRun.UI.Art",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	template <typename T>
	TArray<T*> LoadAll(const TCHAR* Folder)
	{
		TArray<FAssetData> Assets;
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssetsByPath(FName(Folder), Assets, /*bRecursive*/ true);
		TArray<T*> Out;
		for (const FAssetData& Asset : Assets)
		{
			if (T* Object = Cast<T>(Asset.GetAsset()))
			{
				Out.Add(Object);
			}
		}
		return Out;
	}

	void ExpectTexture(const UObject* Owner, const TSoftObjectPtr<UTexture2D>& Texture, const TCHAR* What)
	{
		TestTrue(FString::Printf(TEXT("%s has a %s"), *GetNameSafe(Owner), What), !Texture.IsNull());
		if (!Texture.IsNull())
		{
			TestNotNull(FString::Printf(TEXT("%s %s loads (%s)"), *GetNameSafe(Owner), What, *Texture.ToString()), Texture.LoadSynchronous());
		}
	}

END_DEFINE_SPEC(FDwUIArtSpec)

void FDwUIArtSpec::Define()
{
	It("every player-facing ability has an icon", [this]()
	{
		int32 Checked = 0;
		for (const UDwAbilityDefinition* Ability : LoadAll<UDwAbilityDefinition>(TEXT("/Game/DeadwaterRun/Data/Abilities")))
		{
			const FString Name = Ability->GetName();
			// Kraken moves are enemy-only; Move and UseItem are not cards (items carry their own icons).
			if (Name.Contains(TEXT("_Kraken_")) || Name.EndsWith(TEXT("_Move")) || Name.EndsWith(TEXT("_UseItem")))
			{
				continue;
			}
			ExpectTexture(Ability, Ability->Icon, TEXT("icon"));
			++Checked;
		}
		TestTrue(TEXT("abilities found"), Checked >= 20);
	});

	It("every item, perk and ship upgrade has an icon", [this]()
	{
		const TArray<UDwItemDefinition*> Items = LoadAll<UDwItemDefinition>(TEXT("/Game/DeadwaterRun/Data/Items"));
		const TArray<UDwPerkDefinition*> Perks = LoadAll<UDwPerkDefinition>(TEXT("/Game/DeadwaterRun/Data/Perks"));
		const TArray<UDwShipUpgradeDefinition*> Upgrades = LoadAll<UDwShipUpgradeDefinition>(TEXT("/Game/DeadwaterRun/Data/Ship"));
		TestTrue(TEXT("items, perks and upgrades found"), Items.Num() >= 2 && Perks.Num() >= 9 && Upgrades.Num() >= 3);
		for (const UDwItemDefinition* Item : Items) { ExpectTexture(Item, Item->Icon, TEXT("icon")); }
		for (const UDwPerkDefinition* Perk : Perks) { ExpectTexture(Perk, Perk->Icon, TEXT("icon")); }
		for (const UDwShipUpgradeDefinition* Upgrade : Upgrades) { ExpectTexture(Upgrade, Upgrade->Icon, TEXT("icon")); }
	});

	It("every captain and unit definition has a portrait, captains each their own", [this]()
	{
		TSet<FSoftObjectPath> Faces;
		const TArray<UDwCaptainDefinition*> Captains = LoadAll<UDwCaptainDefinition>(TEXT("/Game/DeadwaterRun/Data/Captains"));
		TestEqual(TEXT("three captains"), Captains.Num(), 3);
		for (const UDwCaptainDefinition* Captain : Captains)
		{
			ExpectTexture(Captain, Captain->Portrait, TEXT("portrait"));
			Faces.Add(Captain->Portrait.ToSoftObjectPath());
		}
		TestEqual(TEXT("three different captain faces"), Faces.Num(), 3);
		for (const UDwUnitDefinition* Unit : LoadAll<UDwUnitDefinition>(TEXT("/Game/DeadwaterRun/Data/Units")))
		{
			ExpectTexture(Unit, Unit->Portrait, TEXT("portrait"));
		}
	});

	It("crew and raider looks have their own faces", [this]()
	{
		for (const TCHAR* Name : { TEXT("BP_Unit_Swordsman"), TEXT("BP_Unit_Sniper"), TEXT("BP_Unit_Raider_Swordsman"), TEXT("BP_Unit_Raider_Sniper") })
		{
			const UClass* Class = LoadClass<ADwCombatUnit>(nullptr, *FString::Printf(TEXT("/Game/DeadwaterRun/Units/%s.%s_C"), Name, Name));
			if (!TestNotNull(FString::Printf(TEXT("%s exists"), Name), Class))
			{
				continue;
			}
			ExpectTexture(Class->GetDefaultObject(), Class->GetDefaultObject<ADwCombatUnit>()->Portrait, TEXT("portrait"));
		}
	});
}

#endif
