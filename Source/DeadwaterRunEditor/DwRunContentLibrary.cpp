// Copyright Brandon Hillig 2026

#include "DwRunContentLibrary.h"

#include "Abilities/DwGA_EffectPayload.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwCaptainDefinition.h"
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
	for (const TCHAR* Table : { TEXT("Captain"), TEXT("Economy"), TEXT("Unit") })
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
	bOk &= Save(GameData, Log);

	Log.Add(bOk ? TEXT("CONTENT PASS") : TEXT("CONTENT FAIL"));
	return Log;
}
