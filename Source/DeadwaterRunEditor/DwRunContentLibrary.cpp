// Copyright Brandon Hillig 2026

#include "DwRunContentLibrary.h"

#include "Abilities/DwAttributeSets.h"
#include "Abilities/DwGA_EffectPayload.h"
#include "Abilities/DwGameplayEffects.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwMapRules.h"
#include "Data/DwPerkDefinition.h"
#include "Data/DwPools.h"
#include "Data/DwShopDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "DwCombatContentLibrary.h"
#include "DwContentAssetUtils.h"

namespace DwRunContent
{
	const FString Abilities = TEXT("/Game/DeadwaterRun/Abilities");
	const FString AbilityData = TEXT("/Game/DeadwaterRun/Data/Abilities");
	const FString UnitData = TEXT("/Game/DeadwaterRun/Data/Units");
	const FString CaptainData = TEXT("/Game/DeadwaterRun/Data/Captains");
	const FString Tuning = TEXT("/Game/DeadwaterRun/Data/Tuning");

	struct FAbilitySpec
	{
		const TCHAR* Asset;
		const TCHAR* Name;
		FGameplayTag Tag;
		const TCHAR* TuningRow;
		const TCHAR* Description;
		const TCHAR* Ticket;
	};

	struct FCaptainSpec
	{
		const TCHAR* Asset;
		const TCHAR* Unit;
		const TCHAR* Name;
		const TCHAR* TuningRow;
		const TCHAR* Description;
		FAbilitySpec Abilities[2];
		TArray<FGameplayTag> RuleTags;
		TArray<FGameplayTag> BlockedAbilityTags;
		TArray<FGameplayTag> KillBonusIgnoredUnitTags;
	};

	/** Text from DeadwaterRun.md "Captains"; rules and numbers in Docs/Architecture/05 and DT_*Tuning. */
	TArray<FCaptainSpec> Captains()
	{
		return {
			{ TEXT("DA_Captain_TeamLeader"), TEXT("DA_Unit_Captain_TeamLeader"), TEXT("Team Leader"), TEXT("TeamLeader"),
				TEXT("Leads a loyal crew. Crew fighting side by side hit harder, and he can shield a crewmate from harm.\nVeteran Crew: his crew earns more XP and levels up faster."),
				{ { TEXT("DA_Ability_Captain_RallyCry"), TEXT("Rally Cry"), DwTags::Ability_Captain_RallyCry, TEXT("Captain_RallyCry"),
					TEXT("Heals all crew by 15 and removes all negative status effects."), TEXT("DW-207") },
				  { TEXT("DA_Ability_Captain_GrappleHook"), TEXT("Grapple Hook"), DwTags::Ability_Captain_GrappleHook, TEXT("Captain_GrappleHook"),
					TEXT("Pull a unit in sight (range 10) next to the captain, or pull the captain next to it."), TEXT("DW-207") } },
				{ DwTags::Rule_Captain_Synergy, DwTags::Rule_Captain_Defend, DwTags::Rule_Captain_VeteranCrew }, {}, {} },
			{ TEXT("DA_Captain_Fearmongerer"), TEXT("DA_Unit_Captain_Fearmongerer"), TEXT("Fearmongerer"), TEXT("Fearmongerer"),
				TEXT("Known across the seas for his ruthlessness. Enemies gather Fear; at 3 they cower, and if every enemy is at 3 the crew surrenders.\nDread Reputation: every surrender win adds starting Fear to later fights."),
				{ { TEXT("DA_Ability_Captain_SmokeAndFire"), TEXT("Smoke and Fire"), DwTags::Ability_Captain_SmokeAndFire, TEXT("Captain_SmokeAndFire"),
					TEXT("Lights the fuses in his beard: every enemy gets 1 Fear, and enemies already afraid take smoke damage."), TEXT("DW-206") },
				  { TEXT("DA_Ability_Captain_SixPistols"), TEXT("Six Pistols"), DwTags::Ability_Captain_SixPistols, TEXT("Captain_SixPistols"),
					TEXT("Six shots at random enemies for 15 damage each."), TEXT("DW-206") } },
				{ DwTags::Rule_Captain_Surrender, DwTags::Rule_Captain_DreadReputation }, {}, {} },
			{ TEXT("DA_Captain_Conqueror"), TEXT("DA_Unit_Captain_Conqueror"), TEXT("The Conqueror"), TEXT("Conqueror"),
				TEXT("Strikes twice a turn and fights with a sword only. His crew make one choice each turn: move or act.\nEvery enemy he kills himself gives him +1% damage for the run (up to 20%)."),
				{ { TEXT("DA_Ability_Captain_Charge"), TEXT("Charge"), DwTags::Ability_Captain_Charge, TEXT("Captain_Charge"),
					TEXT("Dash up to six tiles and strike the first enemy hit for heavy damage."), TEXT("DW-208") },
				  { TEXT("DA_Ability_Captain_EyeOfTheTiger"), TEXT("Eye of the Tiger"), DwTags::Ability_Captain_EyeOfTheTiger, TEXT("Captain_EyeOfTheTiger"),
					TEXT("Freeze one enemy anywhere on the board: it can't move or act for one turn."), TEXT("DW-208") } },
				{ DwTags::Rule_Captain_KillBonus }, { DwTags::Ability_Weapon_Firearm }, { DwTags::Unit_Kraken_Tentacle } },
		};
	}

	const FString PerkEffects = TEXT("/Game/DeadwaterRun/Abilities/Perks");
	const FString PerkData = TEXT("/Game/DeadwaterRun/Data/Perks");
	const FString RulesData = TEXT("/Game/DeadwaterRun/Data/Rules");
	const FString ItemData = TEXT("/Game/DeadwaterRun/Data/Items");

	struct FPerkSpec
	{
		const TCHAR* Asset;
		const TCHAR* Effect;
		const TCHAR* Name;
		const TCHAR* TuningRow;
		const TCHAR* Description;
		FGameplayAttribute Attribute;
		int32 MaxStacks;
	};

	/** DW-203 placeholders (final perks: DW-304, P2). Numbers live in Tuning/PerkTuning.json. */
	TArray<FPerkSpec> Perks()
	{
		return {
			{ TEXT("DA_Perk_SeaLegs"), TEXT("GE_Perk_SeaLegs"), TEXT("Sea Legs"), TEXT("SeaLegs"),
				TEXT("Moves {Value} more tile each turn."), UDwUnitAttributeSet::GetMoveRangeAttribute(), 1 },
			{ TEXT("DA_Perk_Hardened"), TEXT("GE_Perk_Hardened"), TEXT("Hardened"), TEXT("Hardened"),
				TEXT("Takes {ValuePct}% less damage."), UDwUnitAttributeSet::GetDamageReductionAttribute(), 2 },
			{ TEXT("DA_Perk_Cutthroat"), TEXT("GE_Perk_Cutthroat"), TEXT("Cutthroat"), TEXT("Cutthroat"),
				TEXT("Deals {ValuePct}% more damage."), UDwUnitAttributeSet::GetDamageBonusAttribute(), 2 },
		};
	}

	FGameplayTagContainer Container(const TArray<FGameplayTag>& Tags)
	{
		FGameplayTagContainer Out;
		for (const FGameplayTag& Tag : Tags)
		{
			Out.AddTag(Tag);
		}
		return Out;
	}
}

TArray<FString> UDwRunContentLibrary::CreateRunContent()
{
	using namespace DwContentAssets;
	using namespace DwRunContent;
	TArray<FString> Log;
	bool bOk = true;
	auto Fail = [&Log]() { Log.Add(TEXT("CONTENT FAIL")); return Log; };

	// 1. Tuning the run reads: captain rows, starting gold, captain unit rows.
	for (const TCHAR* Table : { TEXT("Captain"), TEXT("Economy"), TEXT("Unit"), TEXT("Encounter"), TEXT("Perk"), TEXT("Progression"), TEXT("Run") })
	{
		bOk &= UDwCombatContentLibrary::SyncTuningTable(Table, Log);
	}
	UDataTable* AbilityTable = Load<UDataTable>(Tuning, TEXT("DT_AbilityTuning"));
	UDataTable* UnitTable = Load<UDataTable>(Tuning, TEXT("DT_UnitTuning"));
	UDataTable* CaptainTable = Load<UDataTable>(Tuning, TEXT("DT_CaptainTuning"));
	UDwGameData* GameData = Load<UDwGameData>(TEXT("/Game/DeadwaterRun/Data"), UDwGameData::AssetName.ToString());
	UDwAbilityDefinition* Cutlass = Load<UDwAbilityDefinition>(AbilityData, TEXT("DA_Ability_AttackSword"));
	UDwUnitDefinition* Swordsman = Load<UDwUnitDefinition>(UnitData, TEXT("DA_Unit_Swordsman"));
	UDwUnitDefinition* Sniper = Load<UDwUnitDefinition>(UnitData, TEXT("DA_Unit_Sniper"));
	if (!GameData || !Cutlass || !Swordsman || !Sniper)
	{
		Log.Add(TEXT("ERROR DA_GameData, DA_Ability_AttackSword, DA_Unit_Swordsman or DA_Unit_Sniper is missing (run the combat content first)"));
		return Fail();
	}

	// 2. One do-nothing ability class for all six placeholders: the ability bar shows the right names and costs, the
	//    command path accepts them, and nothing pretends to be the real effect.
	UBlueprint* PlaceholderBp = FindOrCreateBlueprint(Abilities, TEXT("GA_Captain_Placeholder"), UDwGA_EffectPayload::StaticClass(), Log);
	if (!PlaceholderBp)
	{
		return Fail();
	}
	if (UDwGA_EffectPayload* Placeholder = Defaults<UDwGA_EffectPayload>(PlaceholderBp))
	{
		Placeholder->Targeting = FDwTargetingRules();
		Placeholder->Targeting.Pattern = EDwTargetPattern::Self;
		Placeholder->Applications.Reset();
		Placeholder->PlaceholderSeconds = 0.3f;
	}
	bOk &= Save(PlaceholderBp, Log);

	TArray<TObjectPtr<UDwCaptainDefinition>> Captains;
	for (const FCaptainSpec& Spec : DwRunContent::Captains())
	{
		// 3. The captain as a unit: a cutlass basic attack, stats from DT_UnitTuning (Captain_<Name>).
		UDwUnitDefinition* Unit = FindOrCreate<UDwUnitDefinition>(UnitData, Spec.Unit, Log);
		Unit->DisplayName = FText::FromString(Spec.Name);
		Unit->BasicAttack = Cutlass;
		Unit->UnitTags = FGameplayTagContainer(DwTags::Unit_Role_Captain);
		Unit->Tuning = Row(UnitTable, *FString::Printf(TEXT("Captain_%s"), Spec.TuningRow), Log, bOk);
		bOk &= Save(Unit, Log);

		// 4. Its two combat abilities.
		TArray<TObjectPtr<UDwAbilityDefinition>> CaptainAbilities;
		for (const FAbilitySpec& AbilitySpec : Spec.Abilities)
		{
			UDwAbilityDefinition* Ability = FindOrCreate<UDwAbilityDefinition>(AbilityData, AbilitySpec.Asset, Log);
			Ability->DisplayName = FText::FromString(AbilitySpec.Name);
			Ability->Description = FText::FromString(FString::Printf(TEXT("%s\n(Placeholder: no effect until %s.)"), AbilitySpec.Description, AbilitySpec.Ticket));
			Ability->AbilityClass = TSubclassOf<UDwGameplayAbility>(PlaceholderBp->GeneratedClass.Get());
			Ability->AbilityTag = AbilitySpec.Tag;
			Ability->AbilityTags = FGameplayTagContainer();
			Ability->Tuning = Row(AbilityTable, AbilitySpec.TuningRow, Log, bOk);
			bOk &= Save(Ability, Log);
			CaptainAbilities.Add(Ability);
		}

		// 5. The captain.
		UDwCaptainDefinition* Captain = FindOrCreate<UDwCaptainDefinition>(CaptainData, Spec.Asset, Log);
		Captain->DisplayName = FText::FromString(Spec.Name);
		Captain->Description = FText::FromString(Spec.Description);
		Captain->CaptainUnit = Unit;
		Captain->StartingCrew = { Swordsman, Sniper }; // K14 placeholder: the same crew for every captain
		Captain->CaptainAbilities = CaptainAbilities;
		Captain->RuleTags = Container(Spec.RuleTags);
		Captain->BlockedAbilityTags = Container(Spec.BlockedAbilityTags);
		Captain->KillBonusIgnoredUnitTags = Container(Spec.KillBonusIgnoredUnitTags);
		// Rule GEs (Conqueror's extra action, single-choice crew) are wired by DW-208 with their behaviour.
		Captain->Tuning = Row(CaptainTable, Spec.TuningRow, Log, bOk);
		bOk &= Save(Captain, Log);
		Captains.Add(Captain);
	}

	// 6. Captain select order.
	GameData->Captains = Captains;

	// 7. DW-203 placeholder perks: one shared pool (P2 placeholder) for every crew archetype and captain. Each perk is a
	//    GE_Perk_* Blueprint whose single modifier reads Data.Value, filled at spawn from DT_PerkTuning.
	UDataTable* PerkTable = Load<UDataTable>(Tuning, TEXT("DT_PerkTuning"));
	UDwPerkPool* PerkPool = FindOrCreate<UDwPerkPool>(PerkData, TEXT("DA_PerkPool_Shared"), Log);
	PerkPool->DisplayName = FText::FromString(TEXT("Shared perks"));
	PerkPool->Perks.Reset();
	for (const FPerkSpec& Spec : Perks())
	{
		UBlueprint* EffectBp = FindOrCreateBlueprint(PerkEffects, Spec.Effect, UDwGE_Perk::StaticClass(), Log);
		UDwGE_Perk* Effect = Defaults<UDwGE_Perk>(EffectBp);
		if (!Effect)
		{
			return Fail();
		}
		Effect->Modifiers.Reset();
		Effect->AddValueModifier(Spec.Attribute, EGameplayModOp::AddBase);
		bOk &= Save(EffectBp, Log);

		UDwPerkDefinition* Perk = FindOrCreate<UDwPerkDefinition>(PerkData, Spec.Asset, Log);
		Perk->DisplayName = FText::FromString(Spec.Name);
		Perk->Description = FText::FromString(Spec.Description);
		Perk->Effect = TSubclassOf<UGameplayEffect>(EffectBp->GeneratedClass.Get());
		Perk->GrantedAbility = nullptr;
		Perk->RequiredUnitTags = FGameplayTagContainer();
		Perk->MaxStacks = Spec.MaxStacks;
		Perk->Weight = 1.f;
		Perk->Tuning = Row(PerkTable, Spec.TuningRow, Log, bOk);
		bOk &= Save(Perk, Log);
		PerkPool->Perks.Add(Perk);
	}
	bOk &= Save(PerkPool, Log);
	TArray<UDwUnitDefinition*> CrewUnits = { Swordsman, Sniper };
	for (const UDwCaptainDefinition* Captain : Captains)
	{
		CrewUnits.Add(Captain->CaptainUnit);
	}
	for (UDwUnitDefinition* Unit : CrewUnits)
	{
		Unit->PerkPool = PerkPool;
		bOk &= Save(Unit, Log);
	}

	// 8. DA_Shop_Default: only the item list for now (plunder drops one of them, P6); DW-204 fills in the rest.
	UDwShopDefinition* Shop = FindOrCreate<UDwShopDefinition>(RulesData, TEXT("DA_Shop_Default"), Log);
	Shop->DisplayName = FText::FromString(TEXT("Shop"));
	Shop->Items.Reset();
	for (const TCHAR* ItemName : { TEXT("DA_Item_HealthRestore"), TEXT("DA_Item_ManaRestore") })
	{
		if (UDwItemDefinition* Item = Load<UDwItemDefinition>(ItemData, ItemName))
		{
			Shop->Items.Add(Item);
		}
		else
		{
			Log.Add(FString::Printf(TEXT("ERROR %s is missing (run the combat content first)"), ItemName));
			bOk = false;
		}
	}
	bOk &= Save(Shop, Log);
	GameData->DefaultShop = Shop;

	// 9. DW-202 DA_MapRules: the design numbers (UDwMapRules defaults: 8-9 nodes, 5 fights, 3 lanes, M1-M5 recommended
	//    defaults) plus placeholder type weights for the free slots - two fights for each shop or event.
	UDwMapRules* MapRules = FindOrCreate<UDwMapRules>(RulesData, TEXT("DA_MapRules"), Log);
	MapRules->DisplayName = FText::FromString(TEXT("Sea chart rules"));
	MapRules->TypeWeights.Fight = 2.f;
	MapRules->TypeWeights.Shop = 1.f;
	MapRules->TypeWeights.Event = 1.f;
	bOk &= Save(MapRules, Log);
	GameData->MapRules = MapRules;

	// 10. DW-210 shell travel: Set Sail opens L_Sea, Main Menu opens L_MainMenu (16-Shell D1).
	GameData->MainMenuLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/DeadwaterRun/Maps/L_MainMenu.L_MainMenu")));
	GameData->SeaLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/DeadwaterRun/Maps/L_Sea.L_Sea")));
	bOk &= Save(GameData, Log);

	Log.Add(bOk ? TEXT("CONTENT PASS") : TEXT("CONTENT FAIL"));
	return Log;
}
