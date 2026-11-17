// Copyright Brandon Hillig 2026

#include "DwRunContentLibrary.h"

#include "Abilities/DwAttributeSets.h"
#include "Abilities/DwGA_EffectPayload.h"
#include "Abilities/DwGA_Charge.h"
#include "Abilities/DwGA_Grapple.h"
#include "Abilities/DwGameplayEffects.h"
#include "Board/DwBoardBuilder.h"
#include "Board/DwDeckLayout.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwMapRules.h"
#include "Data/DwPerkDefinition.h"
#include "Data/DwPools.h"
#include "Data/DwShipUpgradeDefinition.h"
#include "Data/DwShopDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Data/DwUnitDefinition.h"
#include "DwCombatContentLibrary.h"
#include "DwContentAssetUtils.h"
#include "DwEventImporter.h"

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
		/** Abilities a rule gives him besides his two (UDwCaptainDefinition::RuleAbilities). */
		TArray<FAbilitySpec> RuleAbilities;
		/** Rule GEs at spawn: on the captain, and on every crew member beside him (UDwCaptainDefinition). */
		TArray<UClass*> CaptainEffects;
		TArray<UClass*> CrewEffects;
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
					TEXT("Pull a unit in sight (range 10), friend or foe, to the free tile beside the captain. Works across ships and out of the nest."), TEXT("DW-207") } },
				{ DwTags::Rule_Captain_Synergy, DwTags::Rule_Captain_Defend, DwTags::Rule_Captain_VeteranCrew }, {}, {},
				{ { TEXT("DA_Ability_Captain_Defend"), TEXT("Defend"), DwTags::Ability_Captain_Defend, TEXT("Captain_Defend"),
					TEXT("Guard an adjacent crewmate until your next turn: single-target attacks on them hit the captain instead, for half damage."),
					TEXT("DW-207") } } },
			{ TEXT("DA_Captain_Fearmongerer"), TEXT("DA_Unit_Captain_Fearmongerer"), TEXT("Fearmongerer"), TEXT("Fearmongerer"),
				TEXT("Known across the seas for his ruthlessness. Enemies gather Fear; at 3 they cower, and if every enemy is at 3 the crew surrenders.\nDread Reputation: every surrender win adds starting Fear to later fights."),
				{ { TEXT("DA_Ability_Captain_SmokeAndFire"), TEXT("Smoke and Fire"), DwTags::Ability_Captain_SmokeAndFire, TEXT("Captain_SmokeAndFire"),
					TEXT("Lights the fuses in his beard: enemies already afraid take 20 damage, then every enemy gains 1 Fear."), TEXT("DW-206") },
				  { TEXT("DA_Ability_Captain_SixPistols"), TEXT("Six Pistols"), DwTags::Ability_Captain_SixPistols, TEXT("Captain_SixPistols"),
					TEXT("Six shots at random enemies in sight, 15 damage each. The same enemy can be hit more than once."), TEXT("DW-206") } },
				{ DwTags::Rule_Captain_Surrender, DwTags::Rule_Captain_DreadReputation }, {}, {} },
			{ TEXT("DA_Captain_Conqueror"), TEXT("DA_Unit_Captain_Conqueror"), TEXT("The Conqueror"), TEXT("Conqueror"),
				TEXT("Strikes twice a turn and fights with a sword only. His crew make one choice each turn: move or act.\nEvery enemy he kills himself gives him +1% damage for the run (up to 20%)."),
				{ { TEXT("DA_Ability_Captain_Charge"), TEXT("Charge"), DwTags::Ability_Captain_Charge, TEXT("Captain_Charge"),
					TEXT("Dash up to six tiles and strike the first enemy hit for heavy damage."), TEXT("DW-208") },
				  { TEXT("DA_Ability_Captain_EyeOfTheTiger"), TEXT("Eye of the Tiger"), DwTags::Ability_Captain_EyeOfTheTiger, TEXT("Captain_EyeOfTheTiger"),
					TEXT("Freeze one enemy anywhere on the board: it can't move or act for one turn."), TEXT("DW-208") } },
				{ DwTags::Rule_Captain_KillBonus }, { DwTags::Ability_Weapon_Firearm }, { DwTags::Unit_Kraken_Tentacle }, {},
				{ UDwGE_ConquerorExtraMove::StaticClass() }, { UDwGE_SingleChoice::StaticClass() } },
		};
	}

	/** A captain ability's real class: a GA_Captain_* Blueprint of Parent whose defaults Configure sets. */
	struct FCaptainAbilityClass
	{
		const TCHAR* Blueprint;
		UClass* Parent;
		TFunction<void(UDwGameplayAbility&)> Configure;
	};

	/**
	 * The captain abilities that have their behaviour (DW-206..208), by DA_Ability_* name. Abilities not listed keep the
	 * do-nothing placeholder. Numbers are DT_AbilityTuning's.
	 */
	TMap<FString, FCaptainAbilityClass> CaptainAbilityClasses()
	{
		TMap<FString, FCaptainAbilityClass> Out;

		// DW-206 Smoke and Fire (05): every enemy; the already afraid take damage first, then everyone gains Fear.
		Out.Add(TEXT("DA_Ability_Captain_SmokeAndFire"), { TEXT("GA_Captain_SmokeAndFire"), UDwGA_EffectPayload::StaticClass(),
			[](UDwGameplayAbility& Ability)
			{
				UDwGA_EffectPayload& Payload = static_cast<UDwGA_EffectPayload&>(Ability);
				const FDwSharedEffects& Effects = DwGas::GetSharedEffects();
				Payload.Targeting = FDwTargetingRules();
				Payload.Targeting.Pattern = EDwTargetPattern::AllEnemies;
				Payload.Targeting.Side = EDwTargetSide::Enemy;
				Payload.Targeting.bIgnoreRange = true;
				FDwEffectApplication Smoke;
				Smoke.Effect = Effects.Damage;
				Smoke.RequiredTargetTags.AddTag(DwTags::Status_Afraid);
				FDwEffectApplication Fear;
				Fear.Effect = Effects.AddFear;
				Payload.Applications = { Smoke, Fear };
				Payload.PlaceholderSeconds = 0.6f;
			} });

		// DW-206 Six Pistols (05, K11 a): six hits on random enemies in sight, repeats allowed.
		Out.Add(TEXT("DA_Ability_Captain_SixPistols"), { TEXT("GA_Captain_SixPistols"), UDwGA_EffectPayload::StaticClass(),
			[](UDwGameplayAbility& Ability)
			{
				UDwGA_EffectPayload& Payload = static_cast<UDwGA_EffectPayload&>(Ability);
				const FDwSharedEffects& Effects = DwGas::GetSharedEffects();
				Payload.Targeting = FDwTargetingRules();
				Payload.Targeting.Pattern = EDwTargetPattern::RandomEnemies;
				Payload.Targeting.Side = EDwTargetSide::Enemy;
				Payload.Targeting.bIgnoreRange = true;
				Payload.Targeting.bRequiresLineOfSight = true;
				FDwEffectApplication Shot;
				Shot.Effect = Effects.Damage;
				Payload.Applications = { Shot };
				Payload.PlaceholderSeconds = 0.8f;
			} });
		// DW-207 Rally Cry (05): every ally heals, then loses its negative statuses.
		Out.Add(TEXT("DA_Ability_Captain_RallyCry"), { TEXT("GA_Captain_RallyCry"), UDwGA_EffectPayload::StaticClass(),
			[](UDwGameplayAbility& Ability)
			{
				UDwGA_EffectPayload& Payload = static_cast<UDwGA_EffectPayload&>(Ability);
				const FDwSharedEffects& Effects = DwGas::GetSharedEffects();
				Payload.Targeting = FDwTargetingRules();
				Payload.Targeting.Pattern = EDwTargetPattern::AllAllies;
				Payload.Targeting.Side = EDwTargetSide::Ally;
				Payload.Targeting.bIgnoreRange = true;
				FDwEffectApplication Heal;
				Heal.Effect = Effects.Heal;
				FDwEffectApplication Cleanse;
				Cleanse.Effect = Effects.Cleanse;
				Payload.Applications = { Heal, Cleanse };
				Payload.PlaceholderSeconds = 0.6f;
			} });

		// DW-207 Grapple Hook (05, K6 a): UDwGA_Grapple sets its own targeting; range from DT_AbilityTuning.
		Out.Add(TEXT("DA_Ability_Captain_GrappleHook"), { TEXT("GA_Captain_GrappleHook"), UDwGA_Grapple::StaticClass(),
			[](UDwGameplayAbility& Ability)
			{
				UDwGA_Grapple& Grapple = static_cast<UDwGA_Grapple&>(Ability);
				Grapple.Targeting = GetDefault<UDwGA_Grapple>()->Targeting;
				Grapple.Applications.Reset();
			} });

		// DW-207 Defend (05, K5): guard an adjacent crewmate until your next turn (GE_Guarded, instigator = the captain);
		// single-target hits on it go to the captain at GuardDamageMultiplier. Given by Rule.Captain.Defend.
		Out.Add(TEXT("DA_Ability_Captain_Defend"), { TEXT("GA_Captain_Defend"), UDwGA_EffectPayload::StaticClass(),
			[](UDwGameplayAbility& Ability)
			{
				UDwGA_EffectPayload& Payload = static_cast<UDwGA_EffectPayload&>(Ability);
				Payload.Targeting = FDwTargetingRules();
				Payload.Targeting.Pattern = EDwTargetPattern::SingleUnit;
				Payload.Targeting.Side = EDwTargetSide::Ally;
				Payload.Targeting.BlockedTargetTags.AddTag(DwTags::Unit_Role_Captain); // not himself
				FDwEffectApplication Guard;
				Guard.Effect = DwGas::GetSharedEffects().Guarded;
				Payload.Applications = { Guard };
				Payload.PlaceholderSeconds = 0.4f;
			} });
		// DW-208 Charge (05, K12 a): UDwGA_Charge finds the straight line; the hit is GE_Damage from the tuning row.
		Out.Add(TEXT("DA_Ability_Captain_Charge"), { TEXT("GA_Captain_Charge"), UDwGA_Charge::StaticClass(),
			[](UDwGameplayAbility& Ability)
			{
				UDwGA_Charge& Charge = static_cast<UDwGA_Charge&>(Ability);
				Charge.Targeting = GetDefault<UDwGA_Charge>()->Targeting;
				FDwEffectApplication Hit;
				Hit.Effect = DwGas::GetSharedEffects().Damage;
				Charge.Applications = { Hit };
			} });

		// DW-208 Eye of the Tiger (05, K12 a): one enemy anywhere, no line of sight needed, frozen for a turn. Not the
		// Kraken body (K10: freeze works on tentacles only).
		Out.Add(TEXT("DA_Ability_Captain_EyeOfTheTiger"), { TEXT("GA_Captain_EyeOfTheTiger"), UDwGA_EffectPayload::StaticClass(),
			[](UDwGameplayAbility& Ability)
			{
				UDwGA_EffectPayload& Payload = static_cast<UDwGA_EffectPayload&>(Ability);
				Payload.Targeting = FDwTargetingRules();
				Payload.Targeting.Pattern = EDwTargetPattern::SingleUnit;
				Payload.Targeting.Side = EDwTargetSide::Enemy;
				Payload.Targeting.bIgnoreRange = true;
				Payload.Targeting.BlockedTargetTags.AddTag(DwTags::Unit_Immune_Freeze);
				FDwEffectApplication Freeze;
				Freeze.Effect = DwGas::GetSharedEffects().Frozen;
				Payload.Applications = { Freeze };
				Payload.PlaceholderSeconds = 0.5f;
			} });
		return Out;
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

	const FString ShipData = TEXT("/Game/DeadwaterRun/Data/Ship");

	struct FUpgradeSpec
	{
		const TCHAR* Asset;
		const TCHAR* Name;
		const TCHAR* TuningRow;
		const TCHAR* Description;
		FGameplayTag GrantedTag;
	};

	/** P9 a) (Brandon, 2026-11-11). Names and text are placeholders; numbers live in Tuning/ShipUpgradeTuning.json. */
	TArray<FUpgradeSpec> ShipUpgrades()
	{
		return {
			{ TEXT("DA_ShipUpgrade_ReinforcedHull"), TEXT("Reinforced Hull"), TEXT("ReinforcedHull"),
				TEXT("+{MaxHealthBonus} max hull. (Placeholder name and text.)"), FGameplayTag() },
			{ TEXT("DA_ShipUpgrade_HeavyShot"), TEXT("Heavy Shot"), TEXT("HeavyShot"),
				TEXT("+{CannonDamageBonus} cannon damage. (Placeholder name and text.)"), FGameplayTag() },
			{ TEXT("DA_ShipUpgrade_ExtraCannon"), TEXT("Third Cannon"), TEXT("ExtraCannon"),
				TEXT("Mounts a third cannon on your deck. (Placeholder name and text.)"), DwTags::Ship_Upgrade_ExtraCannon },
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

		// 4. Its two combat abilities (and any a rule gives him): the real GA_Captain_* class once its ticket has built
		//    it, else the placeholder.
		const TMap<FString, FCaptainAbilityClass> RealClasses = CaptainAbilityClasses();
		auto MakeAbility = [&](const FAbilitySpec& AbilitySpec) -> UDwAbilityDefinition*
		{
			UDwAbilityDefinition* Ability = FindOrCreate<UDwAbilityDefinition>(AbilityData, AbilitySpec.Asset, Log);
			Ability->DisplayName = FText::FromString(AbilitySpec.Name);
			UClass* AbilityClass = PlaceholderBp->GeneratedClass.Get();
			if (const FCaptainAbilityClass* Real = RealClasses.Find(AbilitySpec.Asset))
			{
				UBlueprint* Bp = FindOrCreateBlueprint(Abilities, Real->Blueprint, Real->Parent, Log);
				UDwGameplayAbility* Defaults = Bp ? Cast<UDwGameplayAbility>(Bp->GeneratedClass->GetDefaultObject()) : nullptr;
				if (!Defaults)
				{
					return nullptr;
				}
				Real->Configure(*Defaults);
				bOk &= Save(Bp, Log);
				AbilityClass = Bp->GeneratedClass.Get();
				Ability->Description = FText::FromString(AbilitySpec.Description);
			}
			else
			{
				Ability->Description = FText::FromString(FString::Printf(TEXT("%s\n(Placeholder: no effect until %s.)"), AbilitySpec.Description, AbilitySpec.Ticket));
			}
			Ability->AbilityClass = TSubclassOf<UDwGameplayAbility>(AbilityClass);
			Ability->AbilityTag = AbilitySpec.Tag;
			Ability->AbilityTags = FGameplayTagContainer();
			Ability->Tuning = Row(AbilityTable, AbilitySpec.TuningRow, Log, bOk);
			bOk &= Save(Ability, Log);
			return Ability;
		};
		TArray<TObjectPtr<UDwAbilityDefinition>> CaptainAbilities;
		for (const FAbilitySpec& AbilitySpec : Spec.Abilities)
		{
			UDwAbilityDefinition* Ability = MakeAbility(AbilitySpec);
			if (!Ability)
			{
				return Fail();
			}
			CaptainAbilities.Add(Ability);
		}
		TArray<TObjectPtr<UDwAbilityDefinition>> RuleAbilities;
		for (const FAbilitySpec& AbilitySpec : Spec.RuleAbilities)
		{
			UDwAbilityDefinition* Ability = MakeAbility(AbilitySpec);
			if (!Ability)
			{
				return Fail();
			}
			RuleAbilities.Add(Ability);
		}

		// 5. The captain.
		UDwCaptainDefinition* Captain = FindOrCreate<UDwCaptainDefinition>(CaptainData, Spec.Asset, Log);
		Captain->DisplayName = FText::FromString(Spec.Name);
		Captain->Description = FText::FromString(Spec.Description);
		Captain->CaptainUnit = Unit;
		Captain->StartingCrew = { Swordsman, Sniper }; // K14 placeholder: the same crew for every captain
		Captain->CaptainAbilities = CaptainAbilities;
		Captain->RuleAbilities = RuleAbilities;
		Captain->RuleTags = Container(Spec.RuleTags);
		Captain->BlockedAbilityTags = Container(Spec.BlockedAbilityTags);
		Captain->KillBonusIgnoredUnitTags = Container(Spec.KillBonusIgnoredUnitTags);
		// Rule GEs (DW-208): the Conqueror's second action (K1) and his crew's one choice per turn.
		Captain->CaptainCombatEffects.Reset();
		for (UClass* Effect : Spec.CaptainEffects) { Captain->CaptainCombatEffects.Add(TSubclassOf<UGameplayEffect>(Effect)); }
		Captain->CrewCombatEffects.Reset();
		for (UClass* Effect : Spec.CrewEffects) { Captain->CrewCombatEffects.Add(TSubclassOf<UGameplayEffect>(Effect)); }
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

	// 8. DW-204 ship upgrades (P9): DT_ShipUpgradeTuning from Tuning/ShipUpgradeTuning.json (price and bonuses), one
	//    definition per row. Names and text are placeholders until the content pass (DW-304).
	UDataTable* UpgradeTable = FindOrCreate<UDataTable>(Tuning, TEXT("DT_ShipUpgradeTuning"), Log);
	if (UpgradeTable->RowStruct != FDwShipUpgradeTuningRow::StaticStruct())
	{
		UpgradeTable->RowStruct = FDwShipUpgradeTuningRow::StaticStruct();
		bOk &= Save(UpgradeTable, Log);
	}
	bOk &= UDwCombatContentLibrary::SyncTuningTable(TEXT("ShipUpgrade"), Log);
	GameData->TuningTables.ShipUpgrade = UpgradeTable;
	TArray<TObjectPtr<UDwShipUpgradeDefinition>> Upgrades;
	for (const FUpgradeSpec& Spec : ShipUpgrades())
	{
		UDwShipUpgradeDefinition* Upgrade = FindOrCreate<UDwShipUpgradeDefinition>(ShipData, Spec.Asset, Log);
		Upgrade->DisplayName = FText::FromString(Spec.Name);
		Upgrade->Description = FText::FromString(Spec.Description);
		Upgrade->Effect = nullptr; // the stat bonuses are the tuning row's
		Upgrade->GrantedShipTags = Spec.GrantedTag.IsValid() ? FGameplayTagContainer(Spec.GrantedTag) : FGameplayTagContainer();
		Upgrade->Tuning = Row(UpgradeTable, Spec.TuningRow, Log, bOk);
		bOk &= Save(Upgrade, Log);
		Upgrades.Add(Upgrade);
	}

	// 9. The third cannon's slot on the player deck (P9): a 'U' tile, a cannon only once Ship.Upgrade.ExtraCannon is
	//    bought. Placeholder spot (layout art pass decides): row 0, next to the first cannon's column.
	if (UDwDeckLayout* PlayerDeck = GameData->DefaultPlayerDeck)
	{
		const bool bHasSlot = PlayerDeck->Tiles.ContainsByPredicate([](const FDwTile& Tile) { return FDwBoardBuilder::IsUpgradeCannonSlot(Tile); });
		const FIntPoint Slot(1, 0);
		const int32 Index = Slot.Y * PlayerDeck->Size.X + Slot.X;
		if (!bHasSlot && PlayerDeck->Tiles.IsValidIndex(Index) && PlayerDeck->Tiles[Index].Kind == EDwTileKind::Deck)
		{
			PlayerDeck->Tiles[Index] = FDwDeckAscii::Parse({ TEXT("U") }).Tiles[0];
			bOk &= Save(PlayerDeck, Log);
			Log.Add(FString::Printf(TEXT("%s: third cannon slot at (%d, %d)"), *PlayerDeck->GetName(), Slot.X, Slot.Y));
		}
		for (const FString& Problem : PlayerDeck->Validate())
		{
			Log.Add(FString::Printf(TEXT("ERROR %s: %s"), *PlayerDeck->GetName(), *Problem));
			bOk = false;
		}
	}
	else
	{
		Log.Add(TEXT("ERROR DA_GameData.DefaultPlayerDeck is not set"));
		bOk = false;
	}

	// 10. DA_Shop_Default (10 "Shop"): every item (plunder drops one of them, P6), the P8 recruits, the P9 upgrades.
	UDwShopDefinition* Shop = FindOrCreate<UDwShopDefinition>(RulesData, TEXT("DA_Shop_Default"), Log);
	Shop->DisplayName = FText::FromString(TEXT("Shop"));
	Shop->OfferTypes = GetDefault<UDwShopDefinition>()->OfferTypes;
	Shop->RecruitPool = { Swordsman, Sniper };
	Shop->UpgradePool = Upgrades;
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

	// 11. DW-202 DA_MapRules: the design numbers (UDwMapRules defaults: 8-9 nodes, 5 fights, 3 lanes, M1-M5 recommended
	//    defaults) plus placeholder type weights for the free slots - two fights for each shop or event.
	UDwMapRules* MapRules = FindOrCreate<UDwMapRules>(RulesData, TEXT("DA_MapRules"), Log);
	MapRules->DisplayName = FText::FromString(TEXT("Sea chart rules"));
	MapRules->TypeWeights.Fight = 2.f;
	MapRules->TypeWeights.Shop = 1.f;
	MapRules->TypeWeights.Event = 1.f;
	bOk &= Save(MapRules, Log);
	GameData->MapRules = MapRules;

	// 12. DW-205 events: DA_Event_* and DA_GameData.Events from Tuning/Events.json (after the captains and items they name).
	bOk &= FDwEventImporter::Import(*GameData, Log);

	// 13. DW-210 shell travel: Set Sail opens L_Sea, Main Menu opens L_MainMenu (16-Shell D1).
	GameData->MainMenuLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/DeadwaterRun/Maps/L_MainMenu.L_MainMenu")));
	GameData->SeaLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/DeadwaterRun/Maps/L_Sea.L_Sea")));
	bOk &= Save(GameData, Log);

	Log.Add(bOk ? TEXT("CONTENT PASS") : TEXT("CONTENT FAIL"));
	return Log;
}
