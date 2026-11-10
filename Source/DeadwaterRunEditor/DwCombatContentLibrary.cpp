// Copyright Brandon Hillig 2026

#include "DwCombatContentLibrary.h"

#include "Abilities/DwGA_EffectPayload.h"
#include "Abilities/DwGA_FireCannon.h"
#include "Abilities/DwGA_Move.h"
#include "Abilities/DwGA_UseItem.h"
#include "Abilities/DwGameplayEffects.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Board/DwBoardBuilder.h"
#include "Board/DwDeckLayout.h"
#include "Combat/DwShip.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataTable.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"

namespace DwCombatContent
{
	const FString Abilities = TEXT("/Game/DeadwaterRun/Abilities");
	const FString AbilityData = TEXT("/Game/DeadwaterRun/Data/Abilities");
	const FString ItemData = TEXT("/Game/DeadwaterRun/Data/Items");
	const FString UnitData = TEXT("/Game/DeadwaterRun/Data/Units");
	const FString EncounterData = TEXT("/Game/DeadwaterRun/Data/Encounters");
	const FString Ships = TEXT("/Game/DeadwaterRun/Ships");
	const FString Tuning = TEXT("/Game/DeadwaterRun/Data/Tuning");
	const FString Boards = TEXT("/Game/DeadwaterRun/Data/Boards");
	const FString Input = TEXT("/Game/DeadwaterRun/Input");

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

	UBlueprint* FindOrCreateBlueprint(const FString& Folder, const FString& Name, UClass* Parent, TArray<FString>& Log)
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

	bool Save(UObject* Asset, TArray<FString>& Log)
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

	FDataTableRowHandle Row(UDataTable* Table, const TCHAR* Name, TArray<FString>& Log, bool& bOk)
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

	FGameplayTagContainer Tags(std::initializer_list<FGameplayTag> List)
	{
		FGameplayTagContainer Container;
		for (const FGameplayTag& Tag : List)
		{
			Container.AddTag(Tag);
		}
		return Container;
	}
}

bool UDwCombatContentLibrary::SyncTuningTable(const FString& Name, TArray<FString>& OutLog)
{
	using namespace DwCombatContent;
	const FString TableName = FString::Printf(TEXT("DT_%sTuning"), *Name);
	UDataTable* Table = Load<UDataTable>(Tuning, TableName);
	const FString JsonPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Tuning"), FString::Printf(TEXT("%sTuning.json"), *Name));
	FString Json;
	if (!Table || !FFileHelper::LoadFileToString(Json, *JsonPath))
	{
		OutLog.Add(FString::Printf(TEXT("ERROR missing %s or %s"), *TableName, *JsonPath));
		return false;
	}
	const TArray<FString> Problems = Table->CreateTableFromJSONString(Json);
	for (const FString& Problem : Problems)
	{
		OutLog.Add(FString::Printf(TEXT("ERROR %s: %s"), *TableName, *Problem));
	}
	OutLog.Add(FString::Printf(TEXT("Synced %s from Tuning/%sTuning.json (%d rows)"), *TableName, *Name, Table->GetRowMap().Num()));
	return Problems.IsEmpty() && Save(Table, OutLog);
}

TArray<FString> UDwCombatContentLibrary::CreateCombatPlaceholderContent()
{
	using namespace DwCombatContent;
	TArray<FString> OutLog;
	bool bOk = true;
	auto Fail = [&OutLog]() { OutLog.Add(TEXT("CONTENT FAIL")); return OutLog; };

	// 1. Tuning rows the content reads.
	for (const TCHAR* Table : { TEXT("Ability"), TEXT("Item"), TEXT("Ship") })
	{
		bOk &= SyncTuningTable(Table, OutLog);
	}
	UDataTable* AbilityTable = Load<UDataTable>(Tuning, TEXT("DT_AbilityTuning"));
	UDataTable* ItemTable = Load<UDataTable>(Tuning, TEXT("DT_ItemTuning"));
	UDataTable* UnitTable = Load<UDataTable>(Tuning, TEXT("DT_UnitTuning"));
	UDataTable* ShipTable = Load<UDataTable>(Tuning, TEXT("DT_ShipTuning"));
	UDataTable* EncounterTable = Load<UDataTable>(Tuning, TEXT("DT_EncounterTuning"));
	UDwGameData* GameData = Load<UDwGameData>(TEXT("/Game/DeadwaterRun/Data"), UDwGameData::AssetName.ToString());
	UDwDeckLayout* EnemyDeck = Load<UDwDeckLayout>(Boards, TEXT("DA_Deck_EnemyPlaceholder"));
	if (!GameData || !EnemyDeck)
	{
		OutLog.Add(TEXT("ERROR DA_GameData or DA_Deck_EnemyPlaceholder is missing"));
		return Fail();
	}
	const FDwSharedEffects& Effects = DwGas::GetSharedEffects();

	// 2. Ability Blueprints: defaults only, numbers come from DT_AbilityTuning.
	UBlueprint* MoveBp = FindOrCreateBlueprint(Abilities, TEXT("GA_Move"), UDwGA_Move::StaticClass(), OutLog);
	UBlueprint* SwordBp = FindOrCreateBlueprint(Abilities, TEXT("GA_Attack_Sword"), UDwGA_EffectPayload::StaticClass(), OutLog);
	UBlueprint* RifleBp = FindOrCreateBlueprint(Abilities, TEXT("GA_Attack_Rifle"), UDwGA_EffectPayload::StaticClass(), OutLog);
	UBlueprint* CannonBp = FindOrCreateBlueprint(Abilities, TEXT("GA_FireCannon"), UDwGA_FireCannon::StaticClass(), OutLog);
	UBlueprint* ItemBp = FindOrCreateBlueprint(Abilities, TEXT("GA_UseItem"), UDwGA_UseItem::StaticClass(), OutLog);
	if (!MoveBp || !SwordBp || !RifleBp || !CannonBp || !ItemBp)
	{
		return Fail();
	}
	FDwEffectApplication Damage;
	Damage.Effect = Effects.Damage;

	if (UDwGA_EffectPayload* Sword = Defaults<UDwGA_EffectPayload>(SwordBp))
	{
		Sword->Targeting = FDwTargetingRules();
		Sword->Targeting.Pattern = EDwTargetPattern::SingleUnit;
		Sword->Targeting.Side = EDwTargetSide::Enemy;
		Sword->Targeting.bCanTargetElevated = false; // C5: a sword can't reach the sniper's nest
		Sword->Applications = { Damage };
		Sword->HitCue = DwTags::GameplayCue_Hit_Melee;
		Sword->PlaceholderSeconds = 0.5f;
	}
	if (UDwGA_EffectPayload* Rifle = Defaults<UDwGA_EffectPayload>(RifleBp))
	{
		Rifle->Targeting = FDwTargetingRules();
		Rifle->Targeting.Pattern = EDwTargetPattern::SingleUnit;
		Rifle->Targeting.Side = EDwTargetSide::Enemy;
		Rifle->Targeting.bRequiresLineOfSight = true;
		Rifle->Applications = { Damage };
		Rifle->HitCue = DwTags::GameplayCue_Hit_Ranged;
		Rifle->PlaceholderSeconds = 0.5f;
	}
	for (UBlueprint* Blueprint : { MoveBp, SwordBp, RifleBp, CannonBp, ItemBp })
	{
		bOk &= Save(Blueprint, OutLog);
	}

	// 3. Ability definitions (the entry point content and commands refer to).
	auto MakeAbility = [&](const TCHAR* AssetName, const TCHAR* Display, UBlueprint* Class, const FGameplayTag& Tag,
		const FGameplayTagContainer& Categories, const TCHAR* TuningRow)
	{
		UDwAbilityDefinition* Ability = FindOrCreate<UDwAbilityDefinition>(AbilityData, AssetName, OutLog);
		Ability->DisplayName = FText::FromString(Display);
		Ability->AbilityClass = TSubclassOf<UDwGameplayAbility>(Class->GeneratedClass.Get());
		Ability->AbilityTag = Tag;
		Ability->AbilityTags = Categories;
		Ability->Tuning = Row(AbilityTable, TuningRow, OutLog, bOk);
		bOk &= Save(Ability, OutLog);
		return Ability;
	};
	UDwAbilityDefinition* MoveDef = MakeAbility(TEXT("DA_Ability_Move"), TEXT("Move"), MoveBp, DwTags::Ability_Move, {}, TEXT("Move"));
	UDwAbilityDefinition* SwordDef = MakeAbility(TEXT("DA_Ability_AttackSword"), TEXT("Cutlass"), SwordBp, DwTags::Ability_Attack_Melee,
		Tags({ DwTags::Ability_Attack_Melee, DwTags::Ability_Weapon_Sword }), TEXT("Attack_Sword"));
	UDwAbilityDefinition* RifleDef = MakeAbility(TEXT("DA_Ability_AttackRifle"), TEXT("Musket"), RifleBp, DwTags::Ability_Attack_Ranged,
		Tags({ DwTags::Ability_Attack_Ranged, DwTags::Ability_Weapon_Firearm }), TEXT("Attack_Rifle"));
	UDwAbilityDefinition* CannonDef = MakeAbility(TEXT("DA_Ability_FireCannon"), TEXT("Fire Cannon"), CannonBp, DwTags::Ability_Cannon, {}, TEXT("FireCannon"));
	UDwAbilityDefinition* ItemDef = MakeAbility(TEXT("DA_Ability_UseItem"), TEXT("Use Item"), ItemBp, DwTags::Ability_Item, {}, TEXT("UseItem"));
	SwordDef->Description = FText::FromString(TEXT("Strike an adjacent enemy. Can't reach the sniper's nest."));
	RifleDef->Description = FText::FromString(TEXT("Shoot an enemy in line of sight."));
	bOk &= Save(SwordDef, OutLog) && Save(RifleDef, OutLog);

	// 4. Items: self or an adjacent ally (range from DT_ItemTuning).
	auto MakeItem = [&](const TCHAR* AssetName, const TCHAR* Display, TSubclassOf<UGameplayEffect> Effect, const TCHAR* TuningRow)
	{
		UDwItemDefinition* Item = FindOrCreate<UDwItemDefinition>(ItemData, AssetName, OutLog);
		Item->DisplayName = FText::FromString(Display);
		Item->Effect = Effect;
		Item->Targeting = FDwTargetingRules();
		Item->Targeting.Pattern = EDwTargetPattern::SingleUnit;
		Item->Targeting.Side = EDwTargetSide::Ally;
		Item->MaxStack = 9;
		Item->Tuning = Row(ItemTable, TuningRow, OutLog, bOk);
		bOk &= Save(Item, OutLog);
		return Item;
	};
	MakeItem(TEXT("DA_Item_HealthRestore"), TEXT("Grog"), Effects.Heal, TEXT("HealthRestore"));
	MakeItem(TEXT("DA_Item_ManaRestore"), TEXT("Sea Salts"), Effects.RestoreMana, TEXT("ManaRestore"));

	// 5. Units (placeholder visuals: ADwCombatUnit's tinted cylinder until BP_Unit_* exist, DW-307).
	auto MakeUnit = [&](const TCHAR* AssetName, const TCHAR* Display, UDwAbilityDefinition* Attack, const FGameplayTagContainer& UnitTags, const TCHAR* TuningRow)
	{
		UDwUnitDefinition* Unit = FindOrCreate<UDwUnitDefinition>(UnitData, AssetName, OutLog);
		Unit->DisplayName = FText::FromString(Display);
		Unit->BasicAttack = Attack;
		Unit->UnitTags = UnitTags;
		Unit->Tuning = Row(UnitTable, TuningRow, OutLog, bOk);
		bOk &= Save(Unit, OutLog);
		return Unit;
	};
	MakeUnit(TEXT("DA_Unit_Captain_TeamLeader"), TEXT("Team Leader"), SwordDef, Tags({ DwTags::Unit_Role_Captain }), TEXT("Captain_TeamLeader"));
	UDwUnitDefinition* Swordsman = MakeUnit(TEXT("DA_Unit_Swordsman"), TEXT("Swordsman"), SwordDef, Tags({ DwTags::Unit_Archetype_Swordsman }), TEXT("Swordsman"));
	UDwUnitDefinition* Sniper = MakeUnit(TEXT("DA_Unit_Sniper"), TEXT("Sniper"), RifleDef, Tags({ DwTags::Unit_Archetype_Sniper }), TEXT("Sniper"));

	// 6. Ship and encounter.
	UBlueprint* ShipBp = FindOrCreateBlueprint(Ships, TEXT("BP_Ship_Placeholder"), ADwShip::StaticClass(), OutLog);
	if (!ShipBp)
	{
		return Fail();
	}
	bOk &= Save(ShipBp, OutLog);
	const TSoftClassPtr<ADwShip> ShipClass(ShipBp->GeneratedClass.Get());

	UDwEncounterDefinition* Encounter = FindOrCreate<UDwEncounterDefinition>(EncounterData, TEXT("DA_Encounter_Placeholder"), OutLog);
	Encounter->DisplayName = FText::FromString(TEXT("Placeholder Raiders"));
	Encounter->Kind = EDwEncounterKind::ShipBattle;
	Encounter->ShipDeck = EnemyDeck;
	if (Encounter->ShipClass.IsNull())
	{
		Encounter->ShipClass = ShipClass; // grey-box until UDwArtContentLibrary gives the crew its sloop
	}
	Encounter->ShipTuning = Row(ShipTable, TEXT("EnemyPlaceholder"), OutLog, bOk);
	Encounter->Tuning = Row(EncounterTable, TEXT("Placeholder"), OutLog, bOk);
	Encounter->Units.Reset();
	for (UDwUnitDefinition* Enemy : { Swordsman, Sniper, Swordsman })
	{
		FDwEnemySlot& Slot = Encounter->Units.AddDefaulted_GetRef();
		Slot.UnitDefinition = Enemy;
		Slot.Level = 1;
	}
	bOk &= Save(Encounter, OutLog);

	// 7. DA_GameData slots.
	GameData->CommonAbilities = { MoveDef, CannonDef, ItemDef };
	if (GameData->PlayerShipClass.IsNull())
	{
		GameData->PlayerShipClass = ShipClass;
	}
	if (!GameData->PlayerShipTuning.DataTable)
	{
		GameData->PlayerShipTuning = Row(ShipTable, TEXT("PlayerShip"), OutLog, bOk);
	}
	bOk &= Save(GameData, OutLog);

	// 8. Input (Docs/Architecture/03 "Player input") and the dev fight L_Sea starts until the run exists (DW-201).
	auto MakeAction = [&](const TCHAR* AssetName, EInputActionValueType Type)
	{
		UInputAction* Action = FindOrCreate<UInputAction>(Input, AssetName, OutLog);
		Action->ValueType = Type;
		bOk &= Save(Action, OutLog);
		return Action;
	};
	FDwInputConfig& Config = GameData->Input;
	Config.Select = MakeAction(TEXT("IA_Select"), EInputActionValueType::Boolean);
	Config.Cancel = MakeAction(TEXT("IA_Cancel"), EInputActionValueType::Boolean);
	Config.EndTurn = MakeAction(TEXT("IA_EndTurn"), EInputActionValueType::Boolean);
	Config.NextUnit = MakeAction(TEXT("IA_NextUnit"), EInputActionValueType::Boolean);
	Config.Pause = MakeAction(TEXT("IA_Pause"), EInputActionValueType::Boolean);
	Config.CameraPan = MakeAction(TEXT("IA_CameraPan"), EInputActionValueType::Axis2D);
	Config.CameraZoom = MakeAction(TEXT("IA_CameraZoom"), EInputActionValueType::Axis1D);
	Config.Abilities.Reset();
	for (int32 Slot = 1; Slot <= 6; ++Slot)
	{
		Config.Abilities.Add(MakeAction(*FString::Printf(TEXT("IA_Ability%d"), Slot), EInputActionValueType::Boolean));
	}

	UInputMappingContext* Context = FindOrCreate<UInputMappingContext>(Input, TEXT("IMC_Combat"), OutLog);
	Context->UnmapAll();
	Context->MapKey(Config.Select, EKeys::LeftMouseButton);
	Context->MapKey(Config.Cancel, EKeys::RightMouseButton);
	Context->MapKey(Config.EndTurn, EKeys::SpaceBar);
	Context->MapKey(Config.NextUnit, EKeys::Tab);
	Context->MapKey(Config.Pause, EKeys::Escape);
	Context->MapKey(Config.CameraZoom, EKeys::MouseWheelAxis);
	const FKey Keys[6] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six };
	for (int32 Slot = 0; Slot < 6; ++Slot)
	{
		Context->MapKey(Config.Abilities[Slot], Keys[Slot]);
	}
	// Pan: D/Right = +X, A/Left = -X, W/Up = +Y, S/Down = -Y (swizzle puts a 1D key on the Y axis).
	auto MapPan = [&](FKey Key, bool bNegate, bool bVertical)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Config.CameraPan, Key);
		if (bVertical)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Context));
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
		}
	};
	MapPan(EKeys::D, false, false); MapPan(EKeys::Right, false, false);
	MapPan(EKeys::A, true, false);  MapPan(EKeys::Left, true, false);
	MapPan(EKeys::W, false, true);  MapPan(EKeys::Up, false, true);
	MapPan(EKeys::S, true, true);   MapPan(EKeys::Down, true, true);
	bOk &= Save(Context, OutLog);
	Config.CombatContext = Context;

	GameData->DevEncounter = Encounter;
	GameData->DevCrew = { Load<UDwUnitDefinition>(UnitData, TEXT("DA_Unit_Captain_TeamLeader")), Swordsman, Sniper };
	GameData->DevItems.Reset();
	GameData->DevItems.Add(Load<UDwItemDefinition>(ItemData, TEXT("DA_Item_HealthRestore")), 2);
	GameData->DevItems.Add(Load<UDwItemDefinition>(ItemData, TEXT("DA_Item_ManaRestore")), 1);
	bOk &= Save(GameData, OutLog);

	// 9. What the fight's decks look like (the level's sea, the ships and the pirates are UDwArtContentLibrary, DW-307).
	for (const UDwDeckLayout* Deck : { GameData->DefaultPlayerDeck.Get(), EnemyDeck })
	{
		if (Deck)
		{
			OutLog.Add(FString::Printf(TEXT("%s:"), *Deck->GetName()));
			for (const FString& Line : FDwDeckAscii::Export(FDwDeckSource::FromLayout(*Deck)))
			{
				OutLog.Add(TEXT("    ") + Line);
			}
		}
	}
	OutLog.Add(bOk ? TEXT("CONTENT PASS") : TEXT("CONTENT FAIL"));
	return OutLog;
}
