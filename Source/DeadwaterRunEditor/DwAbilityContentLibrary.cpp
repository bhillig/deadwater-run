// Copyright Brandon Hillig 2026

#include "DwAbilityContentLibrary.h"

#include "Abilities/DwGA_EffectPayload.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwPools.h"
#include "Data/DwUnitDefinition.h"
#include "DwContentAssetUtils.h"
#include "Misc/DataValidation.h"

namespace DwAbilityContent
{
	const FString Abilities = TEXT("/Game/DeadwaterRun/Abilities/Crew");
	const FString AbilityData = TEXT("/Game/DeadwaterRun/Data/Abilities");
	const FString PoolData = TEXT("/Game/DeadwaterRun/Data/Abilities/Pools"); // DefaultGame.ini scans it for DwAbilityPool
	const FString UnitData = TEXT("/Game/DeadwaterRun/Data/Units");
	const FString Tuning = TEXT("/Game/DeadwaterRun/Data/Tuning");

	enum class EArchetype : uint8 { Swordsman, Sniper, Any };

	/** One crew ability (N1 Set A). Numbers (MP, damage, heal, Fear, range, hits, turns, price) are its tuning row. */
	struct FAbilitySpec
	{
		const TCHAR* Name;        // GA_Crew_<Name>, DA_Ability_Crew_<Name>, DT_AbilityTuning row Crew_<Name>
		const TCHAR* DisplayName;
		const TCHAR* Description;
		FGameplayTag AbilityTag;
		EArchetype Archetype;
		TFunction<void(UDwGA_EffectPayload&, FGameplayTagContainer&)> Setup; // targeting + applications, and the AbilityTags
	};

	FDwEffectApplication Apply(TSubclassOf<UGameplayEffect> Effect)
	{
		FDwEffectApplication Application;
		Application.Effect = Effect;
		return Application;
	}

	/** Melee: one enemy within reach, never into (or out of) the sniper's nest (C5). */
	void Melee(UDwGA_EffectPayload& P, FGameplayTagContainer& Tags, EDwTargetPattern Pattern = EDwTargetPattern::SingleUnit)
	{
		P.Targeting.Pattern = Pattern;
		P.Targeting.Side = EDwTargetSide::Enemy;
		P.Targeting.bCanTargetElevated = false;
		P.HitCue = DwTags::GameplayCue_Hit_Melee;
		Tags.AddTag(DwTags::Ability_Attack_Melee);
		Tags.AddTag(DwTags::Ability_Weapon_Sword);
	}

	/** Firearm: needs line of sight. */
	void Shot(UDwGA_EffectPayload& P, FGameplayTagContainer& Tags, EDwTargetPattern Pattern = EDwTargetPattern::SingleUnit)
	{
		P.Targeting.Pattern = Pattern;
		P.Targeting.Side = EDwTargetSide::Enemy;
		P.Targeting.bRequiresLineOfSight = true;
		P.HitCue = DwTags::GameplayCue_Hit_Ranged;
		Tags.AddTag(DwTags::Ability_Attack_Ranged);
		Tags.AddTag(DwTags::Ability_Weapon_Firearm);
	}

	TArray<FAbilitySpec> Specs()
	{
		const FDwSharedEffects& E = DwGas::GetSharedEffects();
		return {
			// Swordsman
			{ TEXT("HeavyBlow"), TEXT("Heavy Blow"), TEXT("One big cutlass hit."), DwTags::Ability_Crew_HeavyBlow, EArchetype::Swordsman,
				[E](UDwGA_EffectPayload& P, FGameplayTagContainer& T) { Melee(P, T); P.Applications = { Apply(E.Damage) }; } },
			{ TEXT("Cleave"), TEXT("Cleave"), TEXT("Hits every adjacent enemy."), DwTags::Ability_Crew_Cleave, EArchetype::Swordsman,
				[E](UDwGA_EffectPayload& P, FGameplayTagContainer& T) { Melee(P, T, EDwTargetPattern::AllEnemies); P.Applications = { Apply(E.Damage) }; } },
			{ TEXT("Hamstring"), TEXT("Hamstring"), TEXT("A cut that leaves the target unable to move next turn."), DwTags::Ability_Crew_Hamstring, EArchetype::Swordsman,
				[E](UDwGA_EffectPayload& P, FGameplayTagContainer& T) { Melee(P, T); P.Applications = { Apply(E.Damage), Apply(E.Rooted) }; } },
			{ TEXT("PommelStrike"), TEXT("Pommel Strike"), TEXT("A blow to the head: the target loses its next turn."), DwTags::Ability_Crew_PommelStrike, EArchetype::Swordsman,
				[E](UDwGA_EffectPayload& P, FGameplayTagContainer& T)
				{
					Melee(P, T);
					P.Targeting.BlockedTargetTags.AddTag(DwTags::Unit_Immune_Freeze);
					P.Applications = { Apply(E.Damage), Apply(E.Frozen) };
				} },
			{ TEXT("BoardingCry"), TEXT("Boarding Cry"), TEXT("A roar that frightens one enemy."), DwTags::Ability_Crew_BoardingCry, EArchetype::Swordsman,
				[E](UDwGA_EffectPayload& P, FGameplayTagContainer&)
				{
					P.Targeting.Pattern = EDwTargetPattern::SingleUnit;
					P.Targeting.Side = EDwTargetSide::Enemy;
					P.Targeting.bRequiresLineOfSight = true;
					P.Targeting.BlockedTargetTags.AddTag(DwTags::Unit_Immune_Fear); // never wasted on the Kraken
					P.HitCue = DwTags::GameplayCue_Status_Fear;
					P.Applications = { Apply(E.AddFear) };
				} },
			// Sniper
			{ TEXT("AimedShot"), TEXT("Aimed Shot"), TEXT("A long, heavy shot."), DwTags::Ability_Crew_AimedShot, EArchetype::Sniper,
				[E](UDwGA_EffectPayload& P, FGameplayTagContainer& T) { Shot(P, T); P.Applications = { Apply(E.Damage) }; } },
			{ TEXT("CrowsNestShot"), TEXT("Crow's Nest Shot"), TEXT("A huge shot, only from the sniper's nest."), DwTags::Ability_Crew_CrowsNestShot, EArchetype::Sniper,
				[E](UDwGA_EffectPayload& P, FGameplayTagContainer& T)
				{
					Shot(P, T);
					P.Targeting.RequiredSourceTileTags.AddTag(DwTags::Tile_Nest);
					P.Applications = { Apply(E.Damage) };
				} },
			{ TEXT("PinningShot"), TEXT("Pinning Shot"), TEXT("Pins a boarder or a runner: it can't move next turn."), DwTags::Ability_Crew_PinningShot, EArchetype::Sniper,
				[E](UDwGA_EffectPayload& P, FGameplayTagContainer& T) { Shot(P, T); P.Applications = { Apply(E.Damage), Apply(E.Rooted) }; } },
			{ TEXT("Scattershot"), TEXT("Scattershot"), TEXT("Sprays shot at random enemies in range."), DwTags::Ability_Crew_Scattershot, EArchetype::Sniper,
				[E](UDwGA_EffectPayload& P, FGameplayTagContainer& T) { Shot(P, T, EDwTargetPattern::RandomEnemies); P.Applications = { Apply(E.Damage) }; } },
			{ TEXT("ChainShot"), TEXT("Chain Shot"), TEXT("Shreds the enemy ship's hull."), DwTags::Ability_Crew_ChainShot, EArchetype::Sniper,
				[E](UDwGA_EffectPayload& P, FGameplayTagContainer& T)
				{
					P.Targeting.Pattern = EDwTargetPattern::EnemyShip;
					P.Targeting.Side = EDwTargetSide::Enemy;
					P.Targeting.bIgnoreRange = true;
					P.HitCue = DwTags::GameplayCue_Cannon_Impact;
					T.AddTag(DwTags::Ability_Weapon_Firearm);
					P.Applications = { Apply(E.Damage) };
				} },
			// Any crew member
			{ TEXT("FieldDressing"), TEXT("Field Dressing"), TEXT("Patches up yourself or an adjacent crewmate."), DwTags::Ability_Crew_FieldDressing, EArchetype::Any,
				[E](UDwGA_EffectPayload& P, FGameplayTagContainer&)
				{
					P.Targeting.Pattern = EDwTargetPattern::SingleUnit;
					P.Targeting.Side = EDwTargetSide::Ally;
					P.HitCue = DwTags::GameplayCue_Unit_Heal;
					P.Applications = { Apply(E.Heal) };
				} },
			{ TEXT("GrogCourage"), TEXT("Grog Courage"), TEXT("A swig that shakes off a root and heals a little."), DwTags::Ability_Crew_GrogCourage, EArchetype::Any,
				[E](UDwGA_EffectPayload& P, FGameplayTagContainer&)
				{
					P.Targeting.Pattern = EDwTargetPattern::Self;
					P.Targeting.Side = EDwTargetSide::Ally;
					P.HitCue = DwTags::GameplayCue_Unit_Heal;
					P.Applications = { Apply(E.Cleanse), Apply(E.Heal) };
				} },
		};
	}

	bool Validate(const UObject* Asset, TArray<FString>& Log)
	{
		FDataValidationContext Context;
		if (Asset->IsDataValid(Context) != EDataValidationResult::Invalid)
		{
			return true;
		}
		for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
		{
			Log.Add(FString::Printf(TEXT("ERROR %s: %s"), *Asset->GetName(), *Issue.Message.ToString()));
		}
		return false;
	}
}

TArray<FString> UDwAbilityContentLibrary::CreateAbilityContent()
{
	using namespace DwContentAssets;
	using namespace DwAbilityContent;
	TArray<FString> Log;
	bool bOk = true;
	auto Fail = [&Log]() { Log.Add(TEXT("CONTENT FAIL")); return Log; };

	UDataTable* AbilityTable = Load<UDataTable>(Tuning, TEXT("DT_AbilityTuning"));
	UDwUnitDefinition* Swordsman = Load<UDwUnitDefinition>(UnitData, TEXT("DA_Unit_Swordsman"));
	UDwUnitDefinition* Sniper = Load<UDwUnitDefinition>(UnitData, TEXT("DA_Unit_Sniper"));
	if (!AbilityTable || !Swordsman || !Sniper)
	{
		Log.Add(TEXT("ERROR DT_AbilityTuning, DA_Unit_Swordsman or DA_Unit_Sniper is missing (run the combat content first)"));
		return Fail();
	}

	// 1. The abilities.
	UDwAbilityPool* SwordPool = FindOrCreate<UDwAbilityPool>(PoolData, TEXT("DA_AbilityPool_Swordsman"), Log);
	UDwAbilityPool* SniperPool = FindOrCreate<UDwAbilityPool>(PoolData, TEXT("DA_AbilityPool_Sniper"), Log);
	SwordPool->DisplayName = FText::FromString(TEXT("Swordsman abilities"));
	SniperPool->DisplayName = FText::FromString(TEXT("Sniper abilities"));
	SwordPool->Abilities.Reset();
	SniperPool->Abilities.Reset();
	for (const FAbilitySpec& Spec : Specs())
	{
		UBlueprint* Bp = FindOrCreateBlueprint(Abilities, FString(TEXT("GA_Crew_")) + Spec.Name, UDwGA_EffectPayload::StaticClass(), Log);
		UDwGA_EffectPayload* Payload = Defaults<UDwGA_EffectPayload>(Bp);
		if (!Payload)
		{
			return Fail();
		}
		Payload->Targeting = FDwTargetingRules();
		Payload->PlaceholderSeconds = 0.5f;
		FGameplayTagContainer AbilityTags;
		Spec.Setup(*Payload, AbilityTags);
		bOk &= Save(Bp, Log);

		UDwAbilityDefinition* Def = FindOrCreate<UDwAbilityDefinition>(AbilityData, FString(TEXT("DA_Ability_Crew_")) + Spec.Name, Log);
		Def->DisplayName = FText::FromString(Spec.DisplayName);
		Def->Description = FText::FromString(Spec.Description);
		Def->AbilityClass = TSubclassOf<UDwGameplayAbility>(Bp->GeneratedClass.Get());
		Def->AbilityTag = Spec.AbilityTag;
		Def->AbilityTags = AbilityTags;
		Def->Tuning = Row(AbilityTable, *(FString(TEXT("Crew_")) + Spec.Name), Log, bOk);
		bOk &= Validate(Def, Log) && Save(Def, Log);

		FDwWeightedAbility Entry;
		Entry.Ability = Def;
		Entry.Weight = 1.f;
		if (Spec.Archetype != EArchetype::Sniper) { SwordPool->Abilities.Add(Entry); }
		if (Spec.Archetype != EArchetype::Swordsman) { SniperPool->Abilities.Add(Entry); }
	}

	// 2. The pools, on the crew archetypes (captains keep none: crew abilities are crew only, N1).
	bOk &= Validate(SwordPool, Log) && Save(SwordPool, Log);
	bOk &= Validate(SniperPool, Log) && Save(SniperPool, Log);
	Swordsman->LearnablePool = SwordPool;
	Sniper->LearnablePool = SniperPool;
	bOk &= Save(Swordsman, Log) && Save(Sniper, Log);

	Log.Add(FString::Printf(TEXT("Pools: Swordsman %d abilities, Sniper %d abilities"), SwordPool->Abilities.Num(), SniperPool->Abilities.Num()));
	Log.Add(bOk ? TEXT("CONTENT PASS") : TEXT("CONTENT FAIL"));
	return Log;
}
