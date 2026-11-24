// Copyright Brandon Hillig 2026

// DW-201: the real captains (Tools/CreateCombatContent.ps1) start runs, and a run's fights are played through the real
// combat subsystem: Health and permadeath carry over, Mana always starts full (C9). DW-203: real rewards, XP and perks.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Count.h"
#include "Abilities/DwAttributeSets.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwEventDefinition.h"
#include "Data/DwMapRules.h"
#include "Run/Events/DwEventResolver.h"
#include "Abilities/DwGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Board/DwBoardBuilder.h"
#include "Board/DwDeckLayout.h"
#include "Combat/DwShip.h"
#include "Data/DwShipUpgradeDefinition.h"
#include "Abilities/DwGameplayEffects.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwPerkDefinition.h"
#include "Data/DwPools.h"
#include "Data/DwShopDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Misc/DataValidation.h"
#include "Run/DwProgressionRules.h"
#include "Run/DwRun.h"
#include "Run/DwRunRandom.h"
#include "Run/DwRunSubsystem.h"
#include "Tests/Combat/DwCombatTestFixture.h"
#include "Tests/Run/DwRunTestContent.h"

BEGIN_DEFINE_SPEC(FDwRunContentSpec, "DeadwaterRun.Run.Content",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	FDwCombatFixture F; // world only; fights come from the run
	const UDwGameData* GameData = nullptr;
	UDwRun* Run = nullptr;

	const UDwCaptainDefinition* Captain(const TCHAR* Name) const
	{
		return UDwRunSubsystem::FindCaptain(Name);
	}

	bool StartRun(const UDwCaptainDefinition* CaptainDef, int32 Seed = 7)
	{
		if (!TestNotNull(TEXT("DA_GameData"), GameData) || !TestNotNull(TEXT("Captain"), CaptainDef))
		{
			return false;
		}
		Run = F.Root(NewObject<UDwRun>());
		TArray<FString> Errors;
		const bool bOk = Run->Initialize(*GameData, *CaptainDef, Seed, UDwRunSubsystem::GatherEncounterPool(), &Errors);
		return TestTrue(FString::Printf(TEXT("Run initialized: %s"), *FString::Join(Errors, TEXT("; "))), bOk);
	}

	/** Sails to the first route and starts its fight in the fixture world. */
	bool StartNextFight(FDwEncounterSetup* OutSetup = nullptr)
	{
		// A fight route if one is offered; shops and events on the way are left at once (DW-204/205 fill them).
		while (Run->GetPhase() != EDwRunPhase::Combat)
		{
			const TArray<int32> Choices = Run->GetChoices();
			const int32* Fight = Choices.FindByPredicate([this](int32 Index) { return Run->GetState().Map.Nodes[Index].Type == EDwNodeType::Fight; });
			if (!TestTrue(TEXT("A route"), !Choices.IsEmpty()) || !TestTrue(TEXT("Sailed"), Run->ChooseNode(Fight ? *Fight : Choices[0])))
			{
				return false;
			}
			if (Run->GetPhase() == EDwRunPhase::Shop) { Run->LeaveShop(); }
			if (Run->GetPhase() == EDwRunPhase::Event) { TestTrue(TEXT("Passed the event"), DwRunTest::PassEvent(*Run)); }
		}
		FDwEncounterSetup Setup;
		TArray<FString> Errors;
		const bool bBuilt = Run->BuildEncounterSetup(Setup, Errors);
		if (!TestTrue(FString::Printf(TEXT("Setup: %s"), *FString::Join(Errors, TEXT("; "))), bBuilt))
		{
			return false;
		}
		if (OutSetup)
		{
			*OutSetup = Setup;
		}
		return TestTrue(TEXT("Fight started"), F.Combat->StartEncounter(Setup));
	}

	/** The current fight's DT_EncounterTuning row: which crew a seed draws is the map's choice (DW-305 tiers). */
	const FDwEncounterTuningRow* FightTuning() const
	{
		const FDwMapNode* Node = Run ? Run->GetCurrentNode() : nullptr;
		return Node && Node->Encounter ? Node->Encounter->GetTuningRow<FDwEncounterTuningRow>() : nullptr;
	}

	FDwUnitHandle FindCrewUnit(const FGuid& RosterId) const
	{
		for (const ADwCombatUnit* Unit : F.Combat->GetUnits())
		{
			if (Unit->GetRosterId() == RosterId)
			{
				return Unit->GetHandle();
			}
		}
		return FDwUnitHandle();
	}

	void KillAllEnemies()
	{
		for (const ADwCombatUnit* Unit : F.Combat->GetUnits())
		{
			if (Unit->GetSide() == EDwSide::Enemy && Unit->IsAlive())
			{
				F.ApplyEffect(DwGas::GetSharedEffects().Kill, Unit->GetHandle());
			}
		}
	}

	/** Deaths outside a command count at the next turn boundary (03-Combat-Turns.md), so the player ends the turn. */
	void ResolveOutsideKills()
	{
		F.EndTurn(EDwSide::Player);
	}

	/** The enemy crew dies by Killer's hand (kill XP), the fight ends, the result goes to the run. */
	bool WinBy(const FGuid& Killer)
	{
		const FDwUnitHandle Source = FindCrewUnit(Killer);
		for (const ADwCombatUnit* Unit : F.Combat->GetUnits())
		{
			if (Unit->GetSide() == EDwSide::Enemy && Unit->IsAlive())
			{
				F.ApplyEffect(DwGas::GetSharedEffects().Kill, Unit->GetHandle(), Source);
			}
		}
		ResolveOutsideKills();
		const FDwCombatResult Result = F.Combat->GetResult();
		F.Combat->EndEncounter();
		return TestEqual(TEXT("Plundered"), Result.Outcome, EDwCombatOutcome::VictoryCrewDefeated) && TestTrue(TEXT("Applied"), Run->ApplyCombatResult(Result));
	}

	/** Claims without a new move; each level-up takes the perk named Pick if offered, else the first. */
	bool FinishRewards(const UDwPerkDefinition* Pick = nullptr)
	{
		if (!TestEqual(TEXT("Rewards"), Run->GetPhase(), EDwRunPhase::Rewards) || !Run->ClaimRewards(INDEX_NONE, FGuid()))
		{
			return false;
		}
		while (const FDwPendingLevelUp* LevelUp = Run->GetPhase() == EDwRunPhase::LevelUp ? Run->GetCurrentLevelUp() : nullptr)
		{
			const int32 Index = LevelUp->PerkChoices.IndexOfByKey(Pick);
			Run->ChoosePerk(LevelUp->PerkChoices.IsEmpty() ? INDEX_NONE : FMath::Max(Index, 0));
		}
		return TestEqual(TEXT("Map"), Run->GetPhase(), EDwRunPhase::Map);
	}

	const UDwPerkDefinition* FindPerk(const TCHAR* Name) const
	{
		const UDwUnitDefinition* Unit = GameData && !GameData->Captains.IsEmpty() ? GameData->Captains[0]->CaptainUnit.Get() : nullptr;
		const UDwPerkPool* Pool = Unit ? Unit->PerkPool.Get() : nullptr;
		const TObjectPtr<UDwPerkDefinition>* Found = Pool ? Pool->Perks.FindByPredicate([Name](const UDwPerkDefinition* P) { return P && P->GetName() == Name; }) : nullptr;
		return Found ? Found->Get() : nullptr;
	}

END_DEFINE_SPEC(FDwRunContentSpec)

void FDwRunContentSpec::Define()
{
	BeforeEach([this]()
	{
		F.Create();
		GameData = UDwAssetManager::Get().GetGameData();
	});
	AfterEach([this]() { F.Destroy(); Run = nullptr; });

#if WITH_EDITOR
	It("has three valid captains in select order, each with a captain unit and two abilities", [this]()
	{
		if (!TestNotNull(TEXT("DA_GameData"), GameData) || !TestEqual(TEXT("Three captains"), GameData->Captains.Num(), 3)) { return; }
		const TCHAR* Order[] = { TEXT("DA_Captain_TeamLeader"), TEXT("DA_Captain_Fearmongerer"), TEXT("DA_Captain_Conqueror") };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const UDwCaptainDefinition* Def = GameData->Captains[Index];
			if (!TestNotNull(TEXT("Captain set"), Def)) { continue; }
			TestEqual(TEXT("Order"), Def->GetName(), FString(Order[Index]));
			FDataValidationContext Context;
			TestEqual(*FString::Printf(TEXT("%s is valid"), *Def->GetName()), Def->IsDataValid(Context), EDataValidationResult::Valid);
			TestTrue(TEXT("Captain tuning row"), Def->GetTuningRow<FDwCaptainTuningRow>() != nullptr);
			TestTrue(TEXT("Rule tags"), !Def->RuleTags.IsEmpty());
			TestTrue(TEXT("Swordsman + Sniper"), Def->StartingCrew.Num() == 2);
			for (const UDwDefinitionBase* Part : { static_cast<const UDwDefinitionBase*>(Def->CaptainUnit.Get()),
				static_cast<const UDwDefinitionBase*>(Def->CaptainAbilities[0].Get()), static_cast<const UDwDefinitionBase*>(Def->CaptainAbilities[1].Get()) })
			{
				FDataValidationContext PartContext;
				TestEqual(*FString::Printf(TEXT("%s is valid"), *GetNameSafe(Part)), Part ? Part->IsDataValid(PartContext) : EDataValidationResult::Invalid,
					EDataValidationResult::Valid);
			}
		}
		const UDwCaptainDefinition* Conqueror = Captain(TEXT("Conqueror"));
		TestTrue(TEXT("Conqueror can't learn firearms (K2)"), Conqueror && Conqueror->BlockedAbilityTags.HasTagExact(DwTags::Ability_Weapon_Firearm));
		TestTrue(TEXT("Fearmongerer can win by surrender"), Captain(TEXT("Fearmongerer")) && Captain(TEXT("Fearmongerer"))->RuleTags.HasTagExact(DwTags::Rule_Captain_Surrender));
	});
#endif

	It("finds captains by short or asset name", [this]()
	{
		TestNotNull(TEXT("Short"), Captain(TEXT("TeamLeader")));
		TestEqual(TEXT("Asset name"), Captain(TEXT("DA_Captain_TeamLeader")), Captain(TEXT("TeamLeader")));
		TestNull(TEXT("Unknown"), Captain(TEXT("Blackbeard")));
	});

	It("starts every captain's run with 100 gold, a full 250 HP ship and placeholder encounters on every fight", [this]()
	{
		for (const TCHAR* Name : { TEXT("TeamLeader"), TEXT("Fearmongerer"), TEXT("Conqueror") })
		{
			if (!StartRun(Captain(Name))) { return; }
			const FDwRunState& S = Run->GetState();
			TestEqual(TEXT("100 gold"), S.Gold, 100);
			TestEqual(TEXT("Ship"), S.Ship.Health, 250.f);
			TestEqual(TEXT("Captain + Swordsman + Sniper"), S.Crew.Num(), 3);
			TestTrue(TEXT("Captain's own unit"), S.Crew[0].bIsCaptain && S.Crew[0].Unit == S.Captain->CaptainUnit);
			for (const FDwMapNode& Node : S.Map.Nodes)
			{
				TestTrue(TEXT("Fights have a crew to fight"), Node.Type != EDwNodeType::Fight || Node.Encounter != nullptr);
			}
		}
	});

	It("builds the node's fight: living crew, captain abilities, ship and seed from the run", [this]()
	{
		if (!StartRun(Captain(TEXT("Fearmongerer")))) { return; }
		FDwEncounterSetup Setup;
		if (!StartNextFight(&Setup)) { return; }
		TestEqual(TEXT("Seed = ForNode"), Setup.Seed, FDwRunRandom::ForNode(7, Run->GetState().CurrentNode));
		TestEqual(TEXT("3 crew"), Setup.Crew.Num(), 3);
		TestTrue(TEXT("Surrender enabled"), Setup.OutcomeConfig.bSurrenderEnabled);
		TestFalse(TEXT("Not the boss"), Setup.OutcomeConfig.bIsBossFight);
		TestTrue(TEXT("Rule tags"), Setup.CaptainRules.Has(DwTags::Rule_Captain_DreadReputation));
		const FDwCombatSnapshot Snap = F.Combat->GetSnapshot();
		const FDwUnitSnapshot* CaptainUnit = Snap.FindUnit(FindCrewUnit(Run->GetState().Crew[0].Id));
		if (!TestNotNull(TEXT("Captain spawned"), CaptainUnit)) { return; }
		TestTrue(TEXT("Smoke and Fire granted"), CaptainUnit->GrantedAbilityTags.HasTagExact(DwTags::Ability_Captain_SmokeAndFire));
		TestTrue(TEXT("Six Pistols granted"), CaptainUnit->GrantedAbilityTags.HasTagExact(DwTags::Ability_Captain_SixPistols));
		const FDwUnitSnapshot* Swordsman = Snap.FindUnit(FindCrewUnit(Run->GetState().Crew[1].Id));
		TestTrue(TEXT("Crew don't get captain abilities"), Swordsman && !Swordsman->GrantedAbilityTags.HasTag(DwTags::Ability_Captain_SmokeAndFire));
	});

	It("carries Health and permadeath into the next fight, with Mana full again (C9)", [this]()
	{
		if (!StartRun(Captain(TEXT("TeamLeader"))) || !StartNextFight()) { return; }
		const FGuid CaptainId = Run->GetState().Crew[0].Id;
		const FGuid SniperId = Run->GetState().Crew[2].Id;

		// Fight 1: the captain is hurt and spends all his Mana, the sniper dies, the enemy crew is wiped.
		const FDwUnitHandle CaptainHandle = FindCrewUnit(CaptainId);
		F.Asc(CaptainHandle)->SetNumericAttributeBase(UDwUnitAttributeSet::GetHealthAttribute(), 60.f);
		F.Asc(CaptainHandle)->SetNumericAttributeBase(UDwUnitAttributeSet::GetManaAttribute(), 0.f);
		F.ApplyEffect(DwGas::GetSharedEffects().Kill, FindCrewUnit(SniperId));
		KillAllEnemies();
		ResolveOutsideKills();
		TestEqual(TEXT("Plundered"), F.Combat->GetOutcome(), EDwCombatOutcome::VictoryCrewDefeated);
		const FDwCombatResult Result = F.Combat->GetResult();
		F.Combat->EndEncounter();
		TestTrue(TEXT("Applied"), Run->ApplyCombatResult(Result));

		const FDwRunState& S = Run->GetState();
		if (!FinishRewards()) { return; }
		// Veteran Crew: a tier-1 crew's clear XP (80-90) x 1.5 reaches level 2: +10 max HP, and the new 10 HP are usable (12-Progression.md).
		TestEqual(TEXT("Captain at level 2"), S.FindCrew(CaptainId)->Level, 2);
		TestEqual(TEXT("Captain kept 60 HP + 10 from the level"), S.FindCrew(CaptainId)->Health, 70.f);
		TestFalse(TEXT("Sniper lost"), S.FindCrew(SniperId)->bAlive);
		TestEqual(TEXT("One cleared"), S.Stats.NodesCleared, 1);

		// Fight 2: two crew; the captain starts at 70 HP and full Mana.
		FDwEncounterSetup Setup;
		if (!StartNextFight(&Setup)) { return; }
		TestEqual(TEXT("Only the living sail on"), Setup.Crew.Num(), 2);
		TestEqual(TEXT("Five units"), F.Combat->GetUnits().Num(), 5);
		TestFalse(TEXT("No sniper aboard"), FindCrewUnit(SniperId).IsValid());
		const FDwUnitHandle Again = FindCrewUnit(CaptainId);
		if (!TestTrue(TEXT("Captain aboard"), Again.IsValid())) { return; }
		TestEqual(TEXT("Health carried"), F.Health(Again), 70.f);
		TestEqual(TEXT("Level 2 max HP"), F.Attr(Again, UDwUnitAttributeSet::GetMaxHealthAttribute()), 110.f);
		TestEqual(TEXT("Mana full"), F.Attr(Again, UDwUnitAttributeSet::GetManaAttribute()), F.Attr(Again, UDwUnitAttributeSet::GetMaxManaAttribute()));
		TestTrue(TEXT("Max Mana is 20"), F.Attr(Again, UDwUnitAttributeSet::GetMaxManaAttribute()) == 20.f);
	});

#if WITH_EDITOR
	It("has the nine perks (P2 a: shared plus archetype perks) in one valid shared pool on every crew archetype and captain, and a plunder item list", [this]()
	{
		if (!TestNotNull(TEXT("DA_GameData"), GameData)) { return; }
		const UDwPerkPool* Pool = GameData->Captains[0]->CaptainUnit->PerkPool;
		if (!TestNotNull(TEXT("DA_PerkPool_Shared"), Pool) || !TestEqual(TEXT("9 perks"), Pool->Perks.Num(), 9)) { return; }
		for (const UDwPerkDefinition* Perk : Pool->Perks)
		{
			FDataValidationContext Context;
			TestEqual(*FString::Printf(TEXT("%s is valid"), *GetNameSafe(Perk)), Perk ? Perk->IsDataValid(Context) : EDataValidationResult::Invalid,
				EDataValidationResult::Valid);
			if (!Perk) { continue; }
			TestTrue(TEXT("Tuning row"), Perk->GetTuningRow<FDwPerkTuningRow>() != nullptr);
			TestTrue(TEXT("Effect is a GE_Perk"), Perk->Effect && Perk->Effect->IsChildOf(UDwGE_Perk::StaticClass()));
			const UGameplayEffect* Effect = Perk->Effect ? Perk->Effect->GetDefaultObject<UGameplayEffect>() : nullptr;
			TestTrue(TEXT("Infinite"), Effect && Effect->DurationPolicy == EGameplayEffectDurationType::Infinite);
			TestTrue(TEXT("Tagged Effect.Perk"), Effect && Effect->GetAssetTags().HasTagExact(DwTags::Effect_Perk));
			TestTrue(TEXT("One Data.Value modifier"), Effect && Effect->Modifiers.Num() == 1
				&& Effect->Modifiers[0].ModifierMagnitude.GetSetByCallerFloat().DataTag == DwTags::Data_Value);
			TestFalse(TEXT("Description tokens filled"), Perk->GetFormattedDescription().ToString().Contains(TEXT("{")));
		}
		const UDwPerkDefinition* Hardened = FindPerk(TEXT("DA_Perk_Hardened"));
		TestEqual(TEXT("Hardened text"), Hardened ? Hardened->GetFormattedDescription().ToString() : FString(), FString(TEXT("Takes 10% less damage.")));
		auto Requires = [this](const TCHAR* Name) { const UDwPerkDefinition* P = FindPerk(Name); return P ? P->RequiredUnitTags : FGameplayTagContainer(); };
		TestTrue(TEXT("Shared perks have no requirement"), Requires(TEXT("DA_Perk_ThickSkin")).IsEmpty() && Requires(TEXT("DA_Perk_RumRation")).IsEmpty());
		TestTrue(TEXT("Iron Hide and Brawler: swordsmen"), Requires(TEXT("DA_Perk_IronHide")).HasTagExact(DwTags::Unit_Archetype_Swordsman)
			&& Requires(TEXT("DA_Perk_Brawler")).HasTagExact(DwTags::Unit_Archetype_Swordsman));
		TestTrue(TEXT("Steady Hand and Deep Breath: snipers"), Requires(TEXT("DA_Perk_SteadyHand")).HasTagExact(DwTags::Unit_Archetype_Sniper)
			&& Requires(TEXT("DA_Perk_DeepBreath")).HasTagExact(DwTags::Unit_Archetype_Sniper));
		for (const UDwCaptainDefinition* Def : GameData->Captains)
		{
			TestTrue(TEXT("Captain unit uses the shared pool"), Def->CaptainUnit->PerkPool.Get() == Pool);
			for (const UDwUnitDefinition* Unit : Def->StartingCrew)
			{
				TestTrue(TEXT("Crew uses the shared pool"), Unit->PerkPool.Get() == Pool);
			}
		}
		TestTrue(TEXT("DA_Shop_Default lists the two items"), GameData->DefaultShop && GameData->DefaultShop->Items.Num() == 2);
	});
#endif

	It("pays a real plunder: base gold x 1.5, one item, kill XP to the killer, and Veteran Crew levels everyone up", [this]()
	{
		if (!StartRun(Captain(TEXT("TeamLeader"))) || !StartNextFight()) { return; }
		const FDwRunState& S = Run->GetState();
		const FDwEncounterTuningRow* Fight = FightTuning();
		if (!TestNotNull(TEXT("Fight 1's DT_EncounterTuning row"), Fight)) { return; }
		if (!WinBy(S.Crew[1].Id)) { return; } // the swordsman cuts down all three
		TestEqual(TEXT("Rewards"), S.Phase, EDwRunPhase::Rewards);
		const int32 Plunder = FMath::RoundToInt(Fight->BaseGold * 1.5f);
		TestEqual(*FString::Printf(TEXT("%d x 1.5"), Fight->BaseGold), S.PendingRewards.Gold, Plunder);
		TestEqual(TEXT("One plunder item (P6: always)"), S.PendingRewards.Items.Num(), 1);
		// N1: abilities from the crew's archetype pools, never for the captain (crew only).
		TestFalse(TEXT("Ability choices from the crew pools"), S.PendingRewards.AbilityChoices.IsEmpty());
		for (const FDwAbilityOffer& Offer : S.PendingRewards.AbilityChoices)
		{
			TestFalse(TEXT("Never the captain"), Offer.EligibleCrew.Contains(S.Crew[0].Id));
		}
		// Clear XP for all; the swordsman also has 3 x 20 kill XP (tier 1 crews are three). Team Leader: x1.5.
		TestEqual(TEXT("Captain: clear x 1.5"), S.PendingRewards.Xp[0].Xp, FMath::RoundToInt(Fight->ClearXp * 1.5f));
		TestEqual(TEXT("Swordsman: (clear + 60) x 1.5"), S.PendingRewards.Xp[1].Xp, FMath::RoundToInt((Fight->ClearXp + 60) * 1.5f));
		TestEqual(TEXT("Swordsman level 3 (100 + 110)"), S.Crew[1].Level, 3);
		TestEqual(TEXT("Four level-ups"), S.PendingLevelUps.Num(), 4);
		const int32 GoldBefore = S.Gold;
		if (!FinishRewards()) { return; }
		TestEqual(TEXT("Plunder banked"), S.Gold, GoldBefore + Plunder);
		TestEqual(TEXT("The item is aboard"), S.Inventory.Num(), 1);
		TestEqual(TEXT("A perk per level"), S.Crew[0].Perks.Num() + S.Crew[1].Perks.Num() + S.Crew[2].Perks.Num(), 4);
	});

#if WITH_EDITOR
	It("sells from a valid DA_Shop_Default: repair 100 for +100, items 30, Swordsman/Sniper 80, the three P9 upgrades", [this]()
	{
		const UDwShopDefinition* Shop = GameData ? GameData->DefaultShop.Get() : nullptr;
		if (!TestNotNull(TEXT("DA_Shop_Default"), Shop)) { return; }
		FDataValidationContext Context;
		TestEqual(TEXT("Valid (every price set)"), Shop->IsDataValid(Context), EDataValidationResult::Valid);
		TestEqual(TEXT("Recruit pool"), Shop->RecruitPool.Num(), 2);
		TestEqual(TEXT("Upgrade pool"), Shop->UpgradePool.Num(), 3);
		for (const UDwShipUpgradeDefinition* Upgrade : Shop->UpgradePool)
		{
			FDataValidationContext UpgradeContext;
			TestEqual(*FString::Printf(TEXT("%s valid"), *GetNameSafe(Upgrade)), Upgrade ? Upgrade->IsDataValid(UpgradeContext) : EDataValidationResult::Invalid,
				EDataValidationResult::Valid);
			TestFalse(TEXT("Description tokens filled"), Upgrade && Upgrade->GetFormattedDescription().ToString().Contains(TEXT("{")));
		}
		const UDwDeckLayout* Deck = GameData->DefaultPlayerDeck;
		TestTrue(TEXT("The player deck has the third cannon's slot"), Deck && Deck->Tiles.ContainsByPredicate([](const FDwTile& T) { return FDwBoardBuilder::IsUpgradeCannonSlot(T); }));
		TestTrue(TEXT("...and still validates"), Deck && Deck->Validate().IsEmpty());
	});

	It("has the event pool from Tuning/Events.json: 12 valid events, a Good and a Bad one on every layer after the first", [this]()
	{
		if (!TestNotNull(TEXT("DA_GameData"), GameData)) { return; }
		TestEqual(TEXT("12 events"), GameData->Events.Num(), 12);
		for (const UDwEventDefinition* Event : GameData->Events)
		{
			FDataValidationContext Context;
			if (!TestEqual(*FString::Printf(TEXT("%s valid"), *GetNameSafe(Event)), Event ? Event->IsDataValid(Context) : EDataValidationResult::Invalid,
				EDataValidationResult::Valid))
			{
				for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
				{
					AddInfo(Issue.Message.ToString());
				}
			}
		}
		// Layer 0 is always a fight (M3); events can sit on 1..MaxPathNodes-1.
		const int32 LastLayer = GameData->MapRules ? GameData->MapRules->MaxPathNodes - 1 : 8;
		for (int32 Layer = 1; Layer <= LastLayer; ++Layer)
		{
			for (const EDwEventValence Valence : { EDwEventValence::Good, EDwEventValence::Bad })
			{
				const bool bAny = GameData->Events.ContainsByPredicate([Layer, Valence](const UDwEventDefinition* Event)
					{ return Event && Event->Valence == Valence && FDwEventResolver::IsAvailable(*Event, Layer); });
				TestTrue(*FString::Printf(TEXT("Layer %d has a %s event"), Layer, Valence == EDwEventValence::Good ? TEXT("Good") : TEXT("Bad")), bAny);
			}
		}
	});
#endif

	It("shops for real: the recruit and the third cannon are in the next fight, the hull repaired", [this]()
	{
		if (!StartRun(Captain(TEXT("TeamLeader")))) { return; }
		const FDwRunState& S = Run->GetState();
		// Prices and gold are balance numbers (DW-401): read what the three buys cost instead of assuming two fights pay for them.
		const FDwTuningTables& T = GameData->TuningTables;
		const FDwEconomyTuningRow* Eco = T.Economy ? T.Economy->FindRow<FDwEconomyTuningRow>(TEXT("Default"), TEXT("Shop test")) : nullptr;
		const FDwShipUpgradeTuningRow* CannonRow = T.ShipUpgrade ? T.ShipUpgrade->FindRow<FDwShipUpgradeTuningRow>(TEXT("ExtraCannon"), TEXT("Shop test")) : nullptr;
		const FDwUnitTuningRow* SwordRow = T.Unit ? T.Unit->FindRow<FDwUnitTuningRow>(TEXT("Swordsman"), TEXT("Shop test")) : nullptr;
		const FDwUnitTuningRow* SniperRow = T.Unit ? T.Unit->FindRow<FDwUnitTuningRow>(TEXT("Sniper"), TEXT("Shop test")) : nullptr;
		if (!TestTrue(TEXT("Economy, ExtraCannon and recruit rows"), Eco && CannonRow && SwordRow && SniperRow)) { return; }
		const int32 Needed = Eco->RepairCost + FMath::Max(SwordRow->RecruitCost, SniperRow->RecruitCost) + CannonRow->Price;
		const float Repaired = FMath::Min(150.f + Eco->RepairAmount, 250.f);
		// Win at least two fights (the hull knocked down to 150 each time) and enough gold for all three, then take the next shop offered.
		for (int32 Wins = 0; Wins < 12 && Run->GetPhase() != EDwRunPhase::Shop; ++Wins)
		{
			const TArray<int32> Choices = Run->GetChoices();
			const int32* Shop = Choices.FindByPredicate([&S](int32 Index) { return S.Map.Nodes[Index].Type == EDwNodeType::Shop; });
			if (Shop && Wins >= 2 && S.Gold >= Needed)
			{
				Run->ChooseNode(*Shop);
				break;
			}
			if (!StartNextFight()) { return; }
			F.Combat->GetShip(EDwSide::Player)->GetAbilitySystemComponent()->SetNumericAttributeBase(UDwShipAttributeSet::GetHealthAttribute(), 150.f);
			if (!WinBy(S.Crew[0].Id) || !FinishRewards()) { return; }
		}
		if (!TestEqual(TEXT("At a shop"), Run->GetPhase(), EDwRunPhase::Shop)) { return; }
		TestEqual(TEXT("Hull 150 from the fight"), S.Ship.Health, 150.f);

		auto Offer = [this](EDwShopOfferType Type, TFunction<bool(const FDwShopOffer&)> Where = nullptr)
		{
			const FDwShopOffer* Found = Run->GetShop().Offers.FindByPredicate([Type, &Where](const FDwShopOffer& O) { return O.Type == Type && (!Where || Where(O)); });
			return Found ? Found->OfferId : INDEX_NONE;
		};
		const int32 Repair = Offer(EDwShopOfferType::Repair);
		const int32 Cannon = Offer(EDwShopOfferType::ShipUpgrade, [](const FDwShopOffer& O) { return O.Upgrade && O.Upgrade->GrantedShipTags.HasTagExact(DwTags::Ship_Upgrade_ExtraCannon); });
		const int32 Recruit = Offer(EDwShopOfferType::Recruit);
		if (!TestTrue(TEXT("Repair, third cannon and a recruit on sale"), Repair != INDEX_NONE && Cannon != INDEX_NONE && Recruit != INDEX_NONE)) { return; }
		TestEqual(TEXT("Repair 100 gold"), Run->GetShop().Find(Repair)->Price, 100);
		TestEqual(TEXT("Recruit 80 gold"), Run->GetShop().Find(Recruit)->Price, 80);
		const int32 Gold = S.Gold;
		if (!TestTrue(TEXT("Enough gold for all three"), Gold >= 100 + 80 + Run->GetShop().Find(Cannon)->Price)) { return; }
		TestEqual(TEXT("Repair"), Run->TryPurchase(Repair, FGuid()), EDwPurchaseResult::Ok);
		TestEqual(TEXT("Third cannon"), Run->TryPurchase(Cannon, FGuid()), EDwPurchaseResult::Ok);
		TestEqual(TEXT("Recruit"), Run->TryPurchase(Recruit, FGuid()), EDwPurchaseResult::Ok);
		TestEqual(TEXT("Hull + RepairAmount"), S.Ship.Health, Repaired);
		TestEqual(TEXT("Four crew"), S.CountLivingCrew(), 4);
		const FGuid RecruitId = S.Crew.Last().Id;
		TestTrue(TEXT("Left"), Run->LeaveShop());

		FDwEncounterSetup Setup;
		if (!StartNextFight(&Setup)) { return; }
		TestTrue(TEXT("The recruit is in the fight"), FindCrewUnit(RecruitId).IsValid());
		const ADwShip* Ship = F.Combat->GetShip(EDwSide::Player);
		TestEqual(TEXT("Three player cannons"), Ship ? Ship->GetCannonTiles().Num() : 0, 3);
		TestEqual(TEXT("Ship spawned with the repaired hull"), Ship ? Ship->GetHealth() : 0.f, Repaired);
		// The upgrade is the player's: the enemy ship keeps exactly its own layout's cannons. With DW-401's prices the
		// purse may only cover all three at the pre-boss shop; the Kraken has no ship, so there is nothing to compare.
		if (!Setup.bHasEnemyShip)
		{
			TestTrue(TEXT("No enemy ship only at the boss"), Setup.OutcomeConfig.bIsBossFight);
			return;
		}
		const ADwShip* Enemy = F.Combat->GetShip(EDwSide::Enemy);
		const UDwDeckLayout* EnemyDeck = Run->GetCurrentNode() && Run->GetCurrentNode()->Encounter ? Run->GetCurrentNode()->Encounter->ShipDeck.Get() : nullptr;
		const int32 EnemyCannons = EnemyDeck ? static_cast<int32>(Algo::CountIf(EnemyDeck->Tiles, [](const FDwTile& T) { return T.Tags.HasTagExact(DwTags::Tile_Cannon); })) : -1;
		TestEqual(TEXT("The enemy ship keeps its own cannons"), Enemy ? Enemy->GetCannonTiles().Num() : 0, EnemyCannons);
	});

	It("gives the Conqueror's crew plain XP (no Veteran Crew)", [this]()
	{
		if (!StartRun(Captain(TEXT("Conqueror"))) || !StartNextFight()) { return; }
		const FDwEncounterTuningRow* Fight = FightTuning();
		if (!TestNotNull(TEXT("Fight 1's DT_EncounterTuning row"), Fight)) { return; }
		if (!WinBy(Run->GetState().Crew[0].Id)) { return; }
		TestEqual(TEXT("Captain: clear + 3 x 20"), Run->GetState().PendingRewards.Xp[0].Xp, Fight->ClearXp + 60);
		TestEqual(TEXT("Sniper: clear"), Run->GetState().PendingRewards.Xp[2].Xp, Fight->ClearXp);
		TestEqual(TEXT("Sniper still level 1"), Run->GetState().Crew[2].Level, 1);
	});

	It("applies perks in the next fight: Sea Legs +1 move, Hardened 10% less damage", [this]()
	{
		const UDwPerkDefinition* SeaLegs = FindPerk(TEXT("DA_Perk_SeaLegs"));
		const UDwPerkDefinition* Hardened = FindPerk(TEXT("DA_Perk_Hardened"));
		if (!TestNotNull(TEXT("Sea Legs"), SeaLegs) || !TestNotNull(TEXT("Hardened"), Hardened)) { return; }
		if (!StartRun(Captain(TEXT("Fearmongerer"))) || !StartNextFight()) { return; }
		const FGuid CaptainId = Run->GetState().Crew[0].Id;
		const FGuid SwordsmanId = Run->GetState().Crew[1].Id;
		if (!WinBy(CaptainId) || !Run->ClaimRewards(INDEX_NONE, FGuid())) { return; }
		// Captain (clear + 60, at least 140) and swordsman (clear, under 100 on tier 1): only the captain levels. Pick Sea Legs if offered (3 of the 5 perks
		// a captain can take); otherwise any perk that leaves move range and damage taken alone, and give Sea Legs by hand.
		const FDwPendingLevelUp* LevelUp = Run->GetCurrentLevelUp();
		if (!TestTrue(TEXT("Captain levelled"), LevelUp && LevelUp->CrewId == CaptainId)) { return; }
		TestEqual(TEXT("3 choices"), LevelUp->PerkChoices.Num(), 3);
		int32 Pick = LevelUp->PerkChoices.IndexOfByKey(SeaLegs);
		if (Pick == INDEX_NONE)
		{
			Pick = LevelUp->PerkChoices.IndexOfByPredicate([Hardened](const UDwPerkDefinition* P) { return P != Hardened; });
		}
		const bool bTookSeaLegs = LevelUp->PerkChoices.IsValidIndex(Pick) && LevelUp->PerkChoices[Pick] == SeaLegs;
		Run->ChoosePerk(Pick);
		TestEqual(TEXT("Map"), Run->GetPhase(), EDwRunPhase::Map);
		if (!bTookSeaLegs)
		{
			TestTrue(TEXT("Sea Legs by hand"), FDwProgressionRules::ApplyPerk(*const_cast<FDwRunState&>(Run->GetState()).FindCrew(CaptainId), *SeaLegs));
		}
		// Give the swordsman Hardened by hand: the second fight checks both effects.
		FDwCrewMemberState* Swordsman = const_cast<FDwRunState&>(Run->GetState()).FindCrew(SwordsmanId);
		Swordsman->Perks.Add(Hardened);

		FDwEncounterSetup Setup;
		if (!StartNextFight(&Setup)) { return; }
		TestTrue(TEXT("Perks in the setup"), Setup.Crew[0].Perks.Contains(SeaLegs) && Setup.Crew[1].Perks.Contains(Hardened));
		const FDwUnitHandle CaptainUnit = FindCrewUnit(CaptainId);
		const FDwUnitHandle SwordsmanUnit = FindCrewUnit(SwordsmanId);
		TestEqual(TEXT("Move range 5 + 1"), F.Attr(CaptainUnit, UDwUnitAttributeSet::GetMoveRangeAttribute()), 6.f);
		TestEqual(TEXT("Move points 6 on the first turn"), F.Attr(CaptainUnit, UDwUnitAttributeSet::GetMovePointsAttribute()), 6.f);
		TestEqual(TEXT("Swordsman's move unchanged"), F.Attr(SwordsmanUnit, UDwUnitAttributeSet::GetMoveRangeAttribute()), 5.f);
		TestEqual(TEXT("Damage reduction 0.1"), F.Attr(SwordsmanUnit, UDwUnitAttributeSet::GetDamageReductionAttribute()), 0.1f, 1e-4f);
		TestEqual(TEXT("Captain has none"), F.Attr(CaptainUnit, UDwUnitAttributeSet::GetDamageReductionAttribute()), 0.f);
	});

	It("survives garbage collection mid-run: a forced GC in a run fight and on the map leaves everything usable", [this]()
	{
		if (!StartRun(Captain(TEXT("TeamLeader"))) || !StartNextFight()) { return; }
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, /*bPerformFullPurge*/ true);
		const FDwCombatSnapshot Snap = F.Combat->GetSnapshot();
		TestEqual(TEXT("Six units still there"), Snap.Units.Num(), 6);
		if (!WinBy(Run->GetState().Crew[0].Id)) { return; }
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
		if (!FinishRewards()) { return; }
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
		TestTrue(TEXT("Next fight starts"), StartNextFight());
	});

	It("ends the run when the captain dies (C10)", [this]()
	{
		if (!StartRun(Captain(TEXT("Conqueror"))) || !StartNextFight()) { return; }
		F.ApplyEffect(DwGas::GetSharedEffects().Kill, FindCrewUnit(Run->GetState().Crew[0].Id));
		ResolveOutsideKills();
		TestEqual(TEXT("Captain dead"), F.Combat->GetOutcome(), EDwCombatOutcome::DefeatCaptainDead);
		const FDwCombatResult Result = F.Combat->GetResult();
		F.Combat->EndEncounter();
		Run->ApplyCombatResult(Result);
		TestEqual(TEXT("Defeat"), Run->GetPhase(), EDwRunPhase::Defeat);
		TestEqual(TEXT("Reason"), Run->GetState().Stats.EndReason, EDwRunEndReason::CaptainDead);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
