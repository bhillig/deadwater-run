// Copyright Brandon Hillig 2026

// DW-304: the crew abilities (N1 = Set A of Docs/Balance/N1-AbilityDrafts.md, crew only) and the perks (P2 a: one shared
// pool, archetype perks filtered by tag). The content is the real content (Tools/CreateCombatContent.ps1); fights use
// the combat fixture's open decks with the real Swordsman and Sniper carrying their whole pool, against fixture grunts
// (100 HP, no damage reduction), so every number below is the tuning row's.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/DwAttributeSets.h"
#include "Abilities/DwGameplayEffects.h"
#include "Combat/DwShip.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwPerkDefinition.h"
#include "Data/DwPools.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Misc/DataValidation.h"
#include "Run/DwProgressionRules.h"
#include "Run/DwRunRandom.h"
#include "Run/Economy/DwRewardRules.h"
#include "Tests/Combat/DwCombatTestFixture.h"

BEGIN_DEFINE_SPEC(FDwCrewAbilitySpec, "DeadwaterRun.Run.CrewAbilities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	FDwCombatFixture F;
	const UDwGameData* GameData = nullptr;

	/** Open decks: player x 0-5 with a ladder (1,0) and the nest (2,0); enemy deck x 6-11. */
	static TArray<FString> PlayerDeck() { return { TEXT("SLN..E"), TEXT("S....E"), TEXT("S....E") }; }
	static TArray<FString> EnemyDeck() { return { TEXT("S....E"), TEXT("S....E"), TEXT("S....E") }; }

	static FDwUnitHandle Swordsman() { return FDwCombatFixture::CrewHandle(1); }
	static FDwUnitHandle Sniper() { return FDwCombatFixture::CrewHandle(2); }
	FDwUnitHandle Enemy(int32 Index) const { return F.EnemyHandle(Index); }

	const UDwUnitDefinition* CrewUnit(const FGameplayTag& Archetype) const
	{
		if (!GameData || GameData->Captains.IsEmpty())
		{
			return nullptr;
		}
		for (const UDwUnitDefinition* Unit : GameData->Captains[0]->StartingCrew)
		{
			if (Unit && Unit->UnitTags.HasTagExact(Archetype))
			{
				return Unit;
			}
		}
		return nullptr;
	}

	const UDwAbilityDefinition* Ability(const FGameplayTag& Tag) const
	{
		for (const FGameplayTag& Archetype : { FGameplayTag(DwTags::Unit_Archetype_Swordsman), FGameplayTag(DwTags::Unit_Archetype_Sniper) })
		{
			const UDwUnitDefinition* Unit = CrewUnit(Archetype);
			const UDwAbilityPool* Pool = Unit ? Unit->LearnablePool.Get() : nullptr;
			const FDwWeightedAbility* Entry = Pool ? Pool->Abilities.FindByPredicate([&Tag](const FDwWeightedAbility& E) { return E.Ability && E.Ability->AbilityTag == Tag; }) : nullptr;
			if (Entry)
			{
				return Entry->Ability;
			}
		}
		return nullptr;
	}

	const UDwPerkDefinition* Perk(const TCHAR* Name) const
	{
		const UDwUnitDefinition* Unit = CrewUnit(DwTags::Unit_Archetype_Swordsman);
		const UDwPerkPool* Pool = Unit ? Unit->PerkPool.Get() : nullptr;
		const TObjectPtr<UDwPerkDefinition>* Found = Pool ? Pool->Perks.FindByPredicate([Name](const UDwPerkDefinition* P) { return P && P->GetName() == Name; }) : nullptr;
		return Found ? Found->Get() : nullptr;
	}

	/** Grunt captain, the real Swordsman and Sniper with their whole pool learned; three grunts on the enemy side. */
	bool StartFight(TFunction<void(FDwEncounterSetup&)> Tweak = nullptr)
	{
		const UDwUnitDefinition* SwordUnit = CrewUnit(DwTags::Unit_Archetype_Swordsman);
		const UDwUnitDefinition* SniperUnit = CrewUnit(DwTags::Unit_Archetype_Sniper);
		if (!TestNotNull(TEXT("Swordsman"), SwordUnit) || !TestNotNull(TEXT("Sniper"), SniperUnit)
			|| !TestNotNull(TEXT("Swordsman pool"), SwordUnit->LearnablePool.Get()) || !TestNotNull(TEXT("Sniper pool"), SniperUnit->LearnablePool.Get()))
		{
			return false;
		}
		F.UseRealActions();
		F.SetDecks(PlayerDeck(), EnemyDeck());
		F.Setup.Crew[0].Definition = F.Grunt;
		F.Setup.Crew[1].Definition = SwordUnit;
		F.Setup.Crew[2].Definition = SniperUnit;
		for (int32 Index = 1; Index <= 2; ++Index)
		{
			for (const FDwWeightedAbility& Entry : F.Setup.Crew[Index].Definition->LearnablePool->Abilities)
			{
				F.Setup.Crew[Index].ExtraAbilities.Add(Entry.Ability);
			}
		}
		for (FDwCombatantSetup& Grunt : F.Setup.Enemies)
		{
			Grunt.Definition = F.Grunt;
		}
		if (Tweak)
		{
			Tweak(F.Setup);
		}
		return TestTrue(TEXT("Started"), F.Start());
	}

	/** Plenty of MP and a fresh action, so one test can cast several abilities in a row. */
	void Ready(FDwUnitHandle Unit)
	{
		UDwAbilitySystemComponent* Asc = F.Asc(Unit);
		Asc->SetNumericAttributeBase(UDwUnitAttributeSet::GetMaxManaAttribute(), 100.f);
		Asc->SetNumericAttributeBase(UDwUnitAttributeSet::GetManaAttribute(), 100.f);
		Asc->SetNumericAttributeBase(UDwUnitAttributeSet::GetActionPointsAttribute(), 1.f);
	}

	void Place(FDwUnitHandle Unit, FIntPoint Tile) { F.Combat->MoveUnit(Unit, Tile, /*bSnapActor*/ true); }

	float Mana(FDwUnitHandle Unit) const { return F.Attr(Unit, UDwUnitAttributeSet::GetManaAttribute()); }

	EDwCommandResult Cast(FDwUnitHandle Source, const FGameplayTag& Tag, FDwUnitHandle Target)
	{
		return F.Combat->SubmitCommand(FDwCombatCommand::MakeAbility(Source, Tag, FDwAbilityTarget::MakeUnit(Target, F.Unit(Target)->GetTile())));
	}

	int32 HitsBy(FDwUnitHandle Source) const
	{
		return F.Listener->Events.FilterByPredicate([Source](const FDwCombatEvent& E) { return E.Type == EDwCombatEventType::UnitDamaged && E.Source == Source; }).Num();
	}

END_DEFINE_SPEC(FDwCrewAbilitySpec)

void FDwCrewAbilitySpec::Define()
{
	BeforeEach([this]()
	{
		F.Create();
		GameData = UDwAssetManager::Get().GetGameData();
	});
	AfterEach([this]() { F.Destroy(); });

	Describe("Content", [this]()
	{
#if WITH_EDITOR
		It("has the twelve Set A abilities, valid, 50 gold each, in the Swordsman and Sniper pools; captains learn none", [this]()
		{
			const FGameplayTag Sword[] = { DwTags::Ability_Crew_HeavyBlow, DwTags::Ability_Crew_Cleave, DwTags::Ability_Crew_Hamstring,
				DwTags::Ability_Crew_PommelStrike, DwTags::Ability_Crew_BoardingCry };
			const FGameplayTag Gun[] = { DwTags::Ability_Crew_AimedShot, DwTags::Ability_Crew_CrowsNestShot, DwTags::Ability_Crew_PinningShot,
				DwTags::Ability_Crew_Scattershot, DwTags::Ability_Crew_ChainShot };
			const FGameplayTag Any[] = { DwTags::Ability_Crew_FieldDressing, DwTags::Ability_Crew_GrogCourage };
			const UDwUnitDefinition* SwordUnit = CrewUnit(DwTags::Unit_Archetype_Swordsman);
			const UDwUnitDefinition* SniperUnit = CrewUnit(DwTags::Unit_Archetype_Sniper);
			if (!TestNotNull(TEXT("Swordsman"), SwordUnit) || !TestNotNull(TEXT("Sniper"), SniperUnit)) { return; }
			const UDwAbilityPool* SwordPool = SwordUnit->LearnablePool;
			const UDwAbilityPool* SniperPool = SniperUnit->LearnablePool;
			if (!TestNotNull(TEXT("Swordsman pool"), SwordPool) || !TestNotNull(TEXT("Sniper pool"), SniperPool)) { return; }
			auto InPool = [](const UDwAbilityPool* Pool, const FGameplayTag& Tag)
			{
				return Pool->Abilities.ContainsByPredicate([&Tag](const FDwWeightedAbility& E) { return E.Ability && E.Ability->AbilityTag == Tag && E.Weight > 0.f; });
			};
			TestEqual(TEXT("Swordsman pool: 5 + 2"), SwordPool->Abilities.Num(), 7);
			TestEqual(TEXT("Sniper pool: 5 + 2"), SniperPool->Abilities.Num(), 7);
			for (const FGameplayTag& Tag : Sword) { TestTrue(*Tag.ToString(), InPool(SwordPool, Tag) && !InPool(SniperPool, Tag)); }
			for (const FGameplayTag& Tag : Gun) { TestTrue(*Tag.ToString(), InPool(SniperPool, Tag) && !InPool(SwordPool, Tag)); }
			for (const FGameplayTag& Tag : Any) { TestTrue(*Tag.ToString(), InPool(SniperPool, Tag) && InPool(SwordPool, Tag)); }

			for (const UDwAbilityPool* Pool : { SwordPool, SniperPool })
			{
				FDataValidationContext PoolContext;
				TestEqual(*FString::Printf(TEXT("%s is valid"), *Pool->GetName()), Pool->IsDataValid(PoolContext), EDataValidationResult::Valid);
				for (const FDwWeightedAbility& Entry : Pool->Abilities)
				{
					FDataValidationContext Context;
					TestEqual(*FString::Printf(TEXT("%s is valid"), *GetNameSafe(Entry.Ability)), Entry.Ability ? Entry.Ability->IsDataValid(Context) : EDataValidationResult::Invalid,
						EDataValidationResult::Valid);
					const FDwAbilityTuningRow* Row = Entry.Ability ? Entry.Ability->GetTuningRow<FDwAbilityTuningRow>() : nullptr;
					TestTrue(*FString::Printf(TEXT("%s: 50 gold, costs MP"), *GetNameSafe(Entry.Ability)), Row && Row->ShopPrice == 50 && Row->ManaCost > 0);
				}
			}
			for (const UDwCaptainDefinition* Captain : GameData->Captains)
			{
				TestNull(*FString::Printf(TEXT("%s learns no crew abilities"), *Captain->GetName()), Captain->CaptainUnit->LearnablePool.Get());
			}
		});
#endif

		It("offers abilities only to the crew whose archetype pool has them, never to the captain", [this]()
		{
			const UDwUnitDefinition* SwordUnit = CrewUnit(DwTags::Unit_Archetype_Swordsman);
			const UDwUnitDefinition* SniperUnit = CrewUnit(DwTags::Unit_Archetype_Sniper);
			if (!TestNotNull(TEXT("Captains"), GameData && !GameData->Captains.IsEmpty() ? GameData->Captains[0].Get() : nullptr)
				|| !TestNotNull(TEXT("Swordsman"), SwordUnit) || !TestNotNull(TEXT("Sniper"), SniperUnit)) { return; }
			FDwRunState State;
			auto Add = [&State](const UDwUnitDefinition* Unit) { FDwCrewMemberState& M = State.Crew.AddDefaulted_GetRef(); M.Id = FGuid::NewGuid(); M.Unit = Unit; M.Level = 1; M.Health = 100.f; return M.Id; };
			const FGuid CaptainId = Add(GameData->Captains[0]->CaptainUnit);
			const FGuid SwordId = Add(SwordUnit);
			const FGuid SniperId = Add(SniperUnit);
			FRandomStream Random(3);
			const TArray<FDwAbilityOffer> Offers = FDwRewardRules::RollAbilityChoices(State, 20, Random);
			TestEqual(TEXT("All twelve on offer"), Offers.Num(), 12);
			for (const FDwAbilityOffer& Offer : Offers)
			{
				const FString Name = GetNameSafe(Offer.Ability);
				TestFalse(*FString::Printf(TEXT("%s: never the captain"), *Name), Offer.EligibleCrew.Contains(CaptainId));
				const bool bSword = SwordUnit->LearnablePool->Abilities.ContainsByPredicate([&Offer](const FDwWeightedAbility& E) { return E.Ability == Offer.Ability; });
				const bool bGun = SniperUnit->LearnablePool->Abilities.ContainsByPredicate([&Offer](const FDwWeightedAbility& E) { return E.Ability == Offer.Ability; });
				TestEqual(*FString::Printf(TEXT("%s: swordsman"), *Name), Offer.EligibleCrew.Contains(SwordId), bSword);
				TestEqual(*FString::Printf(TEXT("%s: sniper"), *Name), Offer.EligibleCrew.Contains(SniperId), bGun);
			}
		});
	});

	Describe("Swordsman", [this]()
	{
		It("Heavy Blow 45, Cleave 20 to every adjacent enemy, Hamstring 15 + rooted, Pommel Strike 10 + frozen, each paying its MP", [this]()
		{
			if (!StartFight()) { return; }
			Place(Swordsman(), FIntPoint(3, 1));
			Place(Enemy(0), FIntPoint(4, 1));
			Place(Enemy(1), FIntPoint(3, 2));
			Place(Enemy(2), FIntPoint(10, 0));

			Ready(Swordsman());
			TestEqual(TEXT("Heavy Blow"), Cast(Swordsman(), DwTags::Ability_Crew_HeavyBlow, Enemy(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("100 - 45"), F.Health(Enemy(0)), 55.f);
			TestEqual(TEXT("10 MP"), Mana(Swordsman()), 90.f);

			Ready(Swordsman());
			TestEqual(TEXT("Cleave"), Cast(Swordsman(), DwTags::Ability_Crew_Cleave, Enemy(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("Cleave: the one in front"), F.Health(Enemy(0)), 35.f);
			TestEqual(TEXT("Cleave: the one beside"), F.Health(Enemy(1)), 80.f);
			TestEqual(TEXT("Cleave: not the one far away"), F.Health(Enemy(2)), 100.f);
			TestEqual(TEXT("8 MP"), Mana(Swordsman()), 92.f);

			Ready(Swordsman());
			TestEqual(TEXT("Hamstring"), Cast(Swordsman(), DwTags::Ability_Crew_Hamstring, Enemy(1)), EDwCommandResult::Accepted);
			TestEqual(TEXT("80 - 15"), F.Health(Enemy(1)), 65.f);
			TestTrue(TEXT("Rooted"), F.Asc(Enemy(1))->HasMatchingGameplayTag(DwTags::Status_Rooted));

			Ready(Swordsman());
			TestEqual(TEXT("Pommel Strike"), Cast(Swordsman(), DwTags::Ability_Crew_PommelStrike, Enemy(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("35 - 10"), F.Health(Enemy(0)), 25.f);
			TestTrue(TEXT("Frozen"), F.Asc(Enemy(0))->HasMatchingGameplayTag(DwTags::Status_Incapacitated_Frozen));
			TestEqual(TEXT("12 MP"), Mana(Swordsman()), 88.f);

			Ready(Swordsman());
			TestEqual(TEXT("Heavy Blow can't reach two tiles"), Cast(Swordsman(), DwTags::Ability_Crew_HeavyBlow, Enemy(2)), EDwCommandResult::InvalidTarget);
		});

		It("Boarding Cry: 2 Fear on one enemy within 3", [this]()
		{
			if (!StartFight()) { return; }
			Place(Swordsman(), FIntPoint(3, 1));
			Place(Enemy(0), FIntPoint(7, 1));
			Ready(Swordsman());
			TestEqual(TEXT("4 tiles: out of range"), Cast(Swordsman(), DwTags::Ability_Crew_BoardingCry, Enemy(0)), EDwCommandResult::InvalidTarget);
			Place(Enemy(0), FIntPoint(6, 1));
			TestEqual(TEXT("3 tiles"), Cast(Swordsman(), DwTags::Ability_Crew_BoardingCry, Enemy(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("2 Fear"), F.Attr(Enemy(0), UDwUnitAttributeSet::GetFearAttribute()), 2.f);
			TestEqual(TEXT("No damage"), F.Health(Enemy(0)), 100.f);
			TestEqual(TEXT("6 MP"), Mana(Swordsman()), 94.f);
		});
	});

	Describe("Sniper", [this]()
	{
		It("Aimed Shot 40 at 8 tiles (not 9); Crow's Nest Shot 55, only from the nest; Pinning Shot 15 + rooted", [this]()
		{
			if (!StartFight()) { return; }
			Place(Sniper(), FIntPoint(0, 1));
			Place(Enemy(0), FIntPoint(9, 1));
			Place(Enemy(1), FIntPoint(6, 0));
			Ready(Sniper());
			TestEqual(TEXT("Aimed Shot at 9: out of range"), Cast(Sniper(), DwTags::Ability_Crew_AimedShot, Enemy(0)), EDwCommandResult::InvalidTarget);
			Place(Enemy(0), FIntPoint(8, 1));
			TestEqual(TEXT("Aimed Shot at 8"), Cast(Sniper(), DwTags::Ability_Crew_AimedShot, Enemy(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("100 - 40"), F.Health(Enemy(0)), 60.f);

			Ready(Sniper());
			TestEqual(TEXT("Crow's Nest Shot from the deck: refused"), Cast(Sniper(), DwTags::Ability_Crew_CrowsNestShot, Enemy(0)), EDwCommandResult::InvalidTarget);
			Place(Sniper(), FIntPoint(2, 0)); // the nest
			TestEqual(TEXT("Crow's Nest Shot from the nest"), Cast(Sniper(), DwTags::Ability_Crew_CrowsNestShot, Enemy(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("60 - 55"), F.Health(Enemy(0)), 5.f);
			TestEqual(TEXT("14 MP"), Mana(Sniper()), 86.f);

			Ready(Sniper());
			TestEqual(TEXT("Pinning Shot"), Cast(Sniper(), DwTags::Ability_Crew_PinningShot, Enemy(1)), EDwCommandResult::Accepted);
			TestEqual(TEXT("100 - 15"), F.Health(Enemy(1)), 85.f);
			TestTrue(TEXT("Rooted"), F.Asc(Enemy(1))->HasMatchingGameplayTag(DwTags::Status_Rooted));
		});

		It("Scattershot: three hits of 12 among enemies within 5; Chain Shot: 50 to the enemy hull", [this]()
		{
			if (!StartFight()) { return; }
			Place(Sniper(), FIntPoint(0, 1));
			Place(Enemy(0), FIntPoint(5, 1)); // 5 tiles: the only one in range
			Place(Enemy(1), FIntPoint(11, 0));
			Place(Enemy(2), FIntPoint(11, 2));
			Ready(Sniper());
			F.Listener->Reset();
			TestEqual(TEXT("Scattershot"), Cast(Sniper(), DwTags::Ability_Crew_Scattershot, Enemy(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("Three hits"), HitsBy(Sniper()), 3);
			TestEqual(TEXT("100 - 3 x 12"), F.Health(Enemy(0)), 64.f);
			TestEqual(TEXT("Out of range: untouched"), F.Health(Enemy(1)) + F.Health(Enemy(2)), 200.f);

			Ready(Sniper());
			TestEqual(TEXT("Chain Shot"), F.Combat->SubmitCommand(FDwCombatCommand::MakeAbility(Sniper(), DwTags::Ability_Crew_ChainShot,
				FDwAbilityTarget::MakeShip(EDwSide::Enemy))), EDwCommandResult::Accepted);
			TestEqual(TEXT("250 - 50"), F.ShipHealth(EDwSide::Enemy), 200.f);
		});
	});

	Describe("Any crew", [this]()
	{
		It("Field Dressing heals 30 on an adjacent crewmate or yourself; Grog Courage shakes off a root and heals 10", [this]()
		{
			if (!StartFight()) { return; }
			Place(Swordsman(), FIntPoint(3, 1));
			Place(Sniper(), FIntPoint(3, 2));
			F.Asc(Swordsman())->SetNumericAttributeBase(UDwUnitAttributeSet::GetHealthAttribute(), 40.f);
			F.Asc(Sniper())->SetNumericAttributeBase(UDwUnitAttributeSet::GetHealthAttribute(), 40.f);

			Ready(Sniper());
			TestEqual(TEXT("Field Dressing on the swordsman"), Cast(Sniper(), DwTags::Ability_Crew_FieldDressing, Swordsman()), EDwCommandResult::Accepted);
			TestEqual(TEXT("40 + 30"), F.Health(Swordsman()), 70.f);
			Ready(Sniper());
			TestEqual(TEXT("On yourself"), Cast(Sniper(), DwTags::Ability_Crew_FieldDressing, Sniper()), EDwCommandResult::Accepted);
			TestEqual(TEXT("40 + 30"), F.Health(Sniper()), 70.f);
			Place(Sniper(), FIntPoint(0, 2));
			Ready(Sniper());
			TestEqual(TEXT("Not from three tiles away"), Cast(Sniper(), DwTags::Ability_Crew_FieldDressing, Swordsman()), EDwCommandResult::InvalidTarget);
			TestEqual(TEXT("Never on an enemy"), Cast(Sniper(), DwTags::Ability_Crew_FieldDressing, Enemy(0)), EDwCommandResult::InvalidTarget);

			F.ApplyEffect(DwGas::GetSharedEffects().Rooted, Swordsman(), Enemy(0));
			TestTrue(TEXT("Rooted"), F.Asc(Swordsman())->HasMatchingGameplayTag(DwTags::Status_Rooted));
			Ready(Swordsman());
			TestEqual(TEXT("Grog Courage"), Cast(Swordsman(), DwTags::Ability_Crew_GrogCourage, Swordsman()), EDwCommandResult::Accepted);
			TestFalse(TEXT("Free"), F.Asc(Swordsman())->HasMatchingGameplayTag(DwTags::Status_Rooted));
			TestEqual(TEXT("70 + 10"), F.Health(Swordsman()), 80.f);
			TestEqual(TEXT("5 MP"), Mana(Swordsman()), 95.f);
		});
	});

	Describe("Perks", [this]()
	{
		It("offers archetype perks only to their archetype; shared perks to everyone", [this]()
		{
			const UDwUnitDefinition* SwordUnit = CrewUnit(DwTags::Unit_Archetype_Swordsman);
			const UDwUnitDefinition* SniperUnit = CrewUnit(DwTags::Unit_Archetype_Sniper);
			if (!TestNotNull(TEXT("Swordsman"), SwordUnit) || !TestNotNull(TEXT("Sniper"), SniperUnit)) { return; }
			FDwCrewMemberState Sword; Sword.Unit = SwordUnit;
			FDwCrewMemberState Gun; Gun.Unit = SniperUnit;
			FDwCrewMemberState Captain; Captain.Unit = GameData->Captains[0]->CaptainUnit;
			for (const TCHAR* Name : { TEXT("DA_Perk_IronHide"), TEXT("DA_Perk_Brawler") })
			{
				const UDwPerkDefinition* P = Perk(Name);
				if (!TestNotNull(Name, P)) { continue; }
				TestTrue(*FString::Printf(TEXT("%s: swordsman"), Name), FDwProgressionRules::CanTakePerk(Sword, *P));
				TestFalse(*FString::Printf(TEXT("%s: not the sniper"), Name), FDwProgressionRules::CanTakePerk(Gun, *P));
				TestFalse(*FString::Printf(TEXT("%s: not the captain"), Name), FDwProgressionRules::CanTakePerk(Captain, *P));
			}
			for (const TCHAR* Name : { TEXT("DA_Perk_SteadyHand"), TEXT("DA_Perk_DeepBreath") })
			{
				const UDwPerkDefinition* P = Perk(Name);
				if (!TestNotNull(Name, P)) { continue; }
				TestTrue(*FString::Printf(TEXT("%s: sniper"), Name), FDwProgressionRules::CanTakePerk(Gun, *P));
				TestFalse(*FString::Printf(TEXT("%s: not the swordsman"), Name), FDwProgressionRules::CanTakePerk(Sword, *P));
				TestFalse(*FString::Printf(TEXT("%s: not the captain"), Name), FDwProgressionRules::CanTakePerk(Captain, *P));
			}
			for (const TCHAR* Name : { TEXT("DA_Perk_ThickSkin"), TEXT("DA_Perk_RumRation"), TEXT("DA_Perk_SeaLegs") })
			{
				const UDwPerkDefinition* P = Perk(Name);
				TestTrue(*FString::Printf(TEXT("%s: everyone"), Name), P && FDwProgressionRules::CanTakePerk(Sword, *P)
					&& FDwProgressionRules::CanTakePerk(Gun, *P) && FDwProgressionRules::CanTakePerk(Captain, *P));
			}
			// The roll respects it: 50 seeds of sniper choices never hold a swordsman perk.
			int32 Wrong = 0;
			for (int32 Seed = 1; Seed <= 50; ++Seed)
			{
				FRandomStream Random(Seed);
				for (const UDwPerkDefinition* P : FDwProgressionRules::RollPerkChoices(Gun, SniperUnit->PerkPool, 3, Random))
				{
					Wrong += P && P->RequiredUnitTags.HasTagExact(DwTags::Unit_Archetype_Swordsman) ? 1 : 0;
				}
			}
			TestEqual(TEXT("No swordsman perk offered to a sniper"), Wrong, 0);
		});

		It("Thick Skin raises max Health in the run and in the fight, and the new Health is usable at once", [this]()
		{
			const UDwPerkDefinition* ThickSkin = Perk(TEXT("DA_Perk_ThickSkin"));
			const UDwUnitDefinition* SwordUnit = CrewUnit(DwTags::Unit_Archetype_Swordsman);
			if (!TestNotNull(TEXT("Thick Skin"), ThickSkin) || !TestNotNull(TEXT("Swordsman"), SwordUnit)) { return; }
			FDwCrewMemberState Member;
			Member.Unit = SwordUnit;
			Member.Level = 1;
			Member.Health = 100.f;
			TestTrue(TEXT("Applied"), FDwProgressionRules::ApplyPerk(Member, *ThickSkin));
			TestEqual(TEXT("Run max 120"), Member.GetMaxHealth(), 120.f);
			TestEqual(TEXT("Health 120"), Member.Health, 120.f);
			TestTrue(TEXT("Stacks twice"), FDwProgressionRules::ApplyPerk(Member, *ThickSkin));
			TestEqual(TEXT("Run max 140"), Member.GetMaxHealth(), 140.f);
			TestFalse(TEXT("Not a third time"), FDwProgressionRules::ApplyPerk(Member, *ThickSkin));

			// In a fight: fresh = full at the perked max; carried Health above the base max is kept.
			if (!StartFight([ThickSkin](FDwEncounterSetup& Setup)
				{
					Setup.Crew[1].Perks = { ThickSkin };
					Setup.Crew[2].Perks = { ThickSkin };
					Setup.Crew[2].Health = 115.f;
				})) { return; }
			TestEqual(TEXT("Fresh swordsman: max 120"), F.Attr(Swordsman(), UDwUnitAttributeSet::GetMaxHealthAttribute()), 120.f);
			TestEqual(TEXT("Fresh swordsman: 120"), F.Health(Swordsman()), 120.f);
			TestEqual(TEXT("Carried 115 stays 115"), F.Health(Sniper()), 115.f);
		});

		It("Rum Ration and Deep Breath raise max MP, and the fight starts with full MP", [this]()
		{
			const UDwPerkDefinition* Rum = Perk(TEXT("DA_Perk_RumRation"));
			const UDwPerkDefinition* Breath = Perk(TEXT("DA_Perk_DeepBreath"));
			if (!TestNotNull(TEXT("Rum Ration"), Rum) || !TestNotNull(TEXT("Deep Breath"), Breath)) { return; }
			if (!StartFight([Rum, Breath](FDwEncounterSetup& Setup)
				{
					Setup.Crew[1].Perks = { Rum };
					Setup.Crew[2].Perks = { Rum, Breath };
				})) { return; }
			TestEqual(TEXT("Swordsman 20 + 5"), Mana(Swordsman()), 25.f);
			TestEqual(TEXT("Sniper 20 + 5 + 10"), Mana(Sniper()), 35.f);
			TestEqual(TEXT("Sniper max 35"), F.Attr(Sniper(), UDwUnitAttributeSet::GetMaxManaAttribute()), 35.f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
