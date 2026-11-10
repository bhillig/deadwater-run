// Copyright Brandon Hillig 2026

// DW-203: gold, plunder and "learn a new move" (Docs/Architecture/10-Economy-Shop-Loot.md "How it's tested"). Pure.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Run/Economy/DwRewardRules.h"
#include "Tests/Run/DwRunTestContent.h"

BEGIN_DEFINE_SPEC(FDwRewardRulesSpec, "DeadwaterRun.Rules.Economy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	DwRunTest::FContent C;
	FDwRunState State;

	FDwRewardContext Context(EDwCombatOutcome Outcome)
	{
		FDwRewardContext Out;
		Out.Outcome = Outcome;
		Out.Encounter = &C.EncounterRow();
		Out.Economy = &C.EconomyRow();
		Out.State = &State;
		Out.PlunderItems = { C.Grog, C.Tonic };
		return Out;
	}

	FDwRewardBundle Compute(EDwCombatOutcome Outcome, int32 Seed = 1)
	{
		FRandomStream Random(Seed);
		return FDwRewardRules::ComputeRewards(Context(Outcome), Random);
	}

	TArray<FDwAbilityOffer> Roll(int32 Count, int32 Seed = 1)
	{
		FRandomStream Random(Seed);
		return FDwRewardRules::RollAbilityChoices(State, Count, Random);
	}

	const FDwAbilityOffer* Find(const TArray<FDwAbilityOffer>& Offers, const UDwAbilityDefinition* Ability)
	{
		return Offers.FindByPredicate([Ability](const FDwAbilityOffer& Offer) { return Offer.Ability == Ability; });
	}

	FDwCrewMemberState& Captain() { return State.Crew[0]; }
	FDwCrewMemberState& Hand() { return State.Crew[1]; }
END_DEFINE_SPEC(FDwRewardRulesSpec)

void FDwRewardRulesSpec::Define()
{
	using namespace DwRunTest;

	BeforeEach([this]()
	{
		C.Create();
		State = FDwRunState();
		State.Captain = C.Captain;
		State.Crew = { Member(C.CaptainUnit, 1, /*bIsCaptain*/ true), Member(C.Deckhand) };
	});
	AfterEach([this]() { C.Destroy(); });

	Describe("Gold", [this]()
	{
		It("pays 150 base as 150 sunk, 225 crew defeated, 225 surrender, 0 for the boss and defeats", [this]()
		{
			TestEqual(TEXT("Sunk"), Compute(EDwCombatOutcome::VictorySunk).Gold, 150);
			TestEqual(TEXT("Crew defeated"), Compute(EDwCombatOutcome::VictoryCrewDefeated).Gold, 225);
			TestEqual(TEXT("Surrender"), Compute(EDwCombatOutcome::VictorySurrender).Gold, 225);
			TestEqual(TEXT("Boss"), Compute(EDwCombatOutcome::VictoryBoss).Gold, 0);
			TestEqual(TEXT("Defeat"), Compute(EDwCombatOutcome::DefeatShipSunk).Gold, 0);
		});

		It("rounds odd amounts and pays nothing without an economy row", [this]()
		{
			C.EncounterRow().BaseGold = 101;
			TestEqual(TEXT("151.5 -> 152"), Compute(EDwCombatOutcome::VictoryCrewDefeated).Gold, 152);
			FDwRewardContext NoEconomy = Context(EDwCombatOutcome::VictoryCrewDefeated);
			NoEconomy.Economy = nullptr;
			FRandomStream Random(1);
			const FDwRewardBundle Bundle = FDwRewardRules::ComputeRewards(NoEconomy, Random);
			TestTrue(TEXT("Empty"), Bundle.Gold == 0 && Bundle.Items.IsEmpty() && Bundle.AbilityChoices.IsEmpty());
		});
	});

	Describe("Plunder", [this]()
	{
		It("drops one shop item on crew defeated and surrender, never on a sink (P6: always)", [this]()
		{
			for (const EDwCombatOutcome Outcome : { EDwCombatOutcome::VictoryCrewDefeated, EDwCombatOutcome::VictorySurrender })
			{
				const FDwRewardBundle Bundle = Compute(Outcome);
				if (TestEqual(TEXT("One item"), Bundle.Items.Num(), 1))
				{
					TestTrue(TEXT("A shop item"), Bundle.Items[0].Item == C.Grog || Bundle.Items[0].Item == C.Tonic);
					TestEqual(TEXT("x1"), Bundle.Items[0].Count, 1);
				}
			}
			for (int32 Seed = 0; Seed < 50; ++Seed)
			{
				TestTrue(TEXT("Never on a sink"), Compute(EDwCombatOutcome::VictorySunk, Seed).Items.IsEmpty());
			}
		});

		It("rolls PlunderItemChance: 0 never, 0.5 about half the time, both items appear", [this]()
		{
			C.EconomyRow().PlunderItemChance = 0.f;
			TestTrue(TEXT("Chance 0"), Compute(EDwCombatOutcome::VictoryCrewDefeated).Items.IsEmpty());
			C.EconomyRow().PlunderItemChance = 0.5f;
			int32 Drops = 0, Grogs = 0;
			for (int32 Seed = 0; Seed < 2000; ++Seed)
			{
				const FDwRewardBundle Bundle = Compute(EDwCombatOutcome::VictoryCrewDefeated, Seed);
				Drops += Bundle.Items.Num();
				Grogs += Bundle.Items.Num() == 1 && Bundle.Items[0].Item == C.Grog ? 1 : 0;
			}
			TestTrue(*FString::Printf(TEXT("About half drop (%d of 2000)"), Drops), Drops > 900 && Drops < 1100);
			TestTrue(*FString::Printf(TEXT("Both items drop (%d grog)"), Grogs), Grogs > Drops / 3 && Grogs < Drops * 2 / 3);
		});

		It("is stable per seed", [this]()
		{
			for (int32 Seed = 0; Seed < 20; ++Seed)
			{
				const FDwRewardBundle A = Compute(EDwCombatOutcome::VictorySurrender, Seed);
				const FDwRewardBundle B = Compute(EDwCombatOutcome::VictorySurrender, Seed);
				TestTrue(TEXT("Same item"), A.Items.Num() == B.Items.Num() && (A.Items.IsEmpty() || A.Items[0].Item == B.Items[0].Item));
				TestEqual(TEXT("Same choices"), A.AbilityChoices.Num(), B.AbilityChoices.Num());
				for (int32 Index = 0; Index < FMath::Min(A.AbilityChoices.Num(), B.AbilityChoices.Num()); ++Index)
				{
					TestTrue(TEXT("Same order"), A.AbilityChoices[Index].Ability == B.AbilityChoices[Index].Ability);
				}
			}
		});
	});

	Describe("Ability choices", [this]()
	{
		It("offers RewardAbilityChoices distinct abilities from the living crew's pools, each with who can learn it", [this]()
		{
			const TArray<FDwAbilityOffer> Offers = Roll(3);
			if (!TestEqual(TEXT("3 offers"), Offers.Num(), 3)) { return; }
			TSet<const UDwAbilityDefinition*> Seen;
			for (const FDwAbilityOffer& Offer : Offers)
			{
				TestFalse(TEXT("No repeats"), Seen.Contains(Offer.Ability));
				Seen.Add(Offer.Ability);
				TestTrue(TEXT("Someone can learn it"), !Offer.EligibleCrew.IsEmpty());
			}
			const TArray<FDwAbilityOffer> All = Roll(10);
			TestEqual(TEXT("Only 4 learnable"), All.Num(), 4);
			const FDwAbilityOffer* Parry = Find(All, C.Parry);
			const FDwAbilityOffer* Rigging = Find(All, C.Rigging);
			const FDwAbilityOffer* Lantern = Find(All, C.Lantern);
			TestTrue(TEXT("Parry: both"), Parry && Parry->EligibleCrew.Num() == 2);
			TestTrue(TEXT("Rigging: the deckhand's pool"), Rigging && Rigging->EligibleCrew == TArray<FGuid>{ Hand().Id });
			TestTrue(TEXT("Lantern: the captain's pool"), Lantern && Lantern->EligibleCrew == TArray<FGuid>{ Captain().Id });
			TestEqual(TEXT("Basic attacks are never offered"), Find(All, C.Cutlass), static_cast<const FDwAbilityOffer*>(nullptr));
			TestEqual(TEXT("Count 0 = none"), Roll(0).Num(), 0);
		});

		It("skips abilities a crew member already knows (P5) and the pools of the dead", [this]()
		{
			Hand().Abilities.Add(C.Rigging);
			Captain().Abilities.Add(C.Parry);
			const TArray<FDwAbilityOffer> All = Roll(10);
			TestNull(TEXT("Rigging known by its only learner"), Find(All, C.Rigging));
			const FDwAbilityOffer* Parry = Find(All, C.Parry);
			TestTrue(TEXT("Parry only for the deckhand"), Parry && Parry->EligibleCrew == TArray<FGuid>{ Hand().Id });

			Hand().bAlive = false;
			const TArray<FDwAbilityOffer> CaptainOnly = Roll(10);
			for (const FDwAbilityOffer& Offer : CaptainOnly)
			{
				TestFalse(TEXT("The dead learn nothing"), Offer.EligibleCrew.Contains(Hand().Id));
			}
			TestNull(TEXT("Parry gone (captain knows it, deckhand dead)"), Find(CaptainOnly, C.Parry));
		});

		It("never offers the captain his BlockedAbilityTags (K2), but still offers them to his crew", [this]()
		{
			C.Captain->BlockedAbilityTags.AddTag(DwTags::Ability_Weapon_Firearm);
			const TArray<FDwAbilityOffer> All = Roll(10);
			const FDwAbilityOffer* Musket = Find(All, C.Musket);
			TestTrue(TEXT("Musket for the deckhand only"), Musket && Musket->EligibleCrew == TArray<FGuid>{ Hand().Id });
			TestFalse(TEXT("CanLearn agrees"), FDwRewardRules::CanLearn(State, Captain(), *C.Musket));
			Hand().bAlive = false;
			const TArray<FDwAbilityOffer> AfterDeath = Roll(10);
			TestNull(TEXT("Nobody left to learn it"), Find(AfterDeath, C.Musket));
		});

		It("is skipped for encounters without an ability choice and for the boss", [this]()
		{
			C.EncounterRow().bGrantsAbilityChoice = false;
			TestTrue(TEXT("Flag off"), Compute(EDwCombatOutcome::VictoryCrewDefeated).AbilityChoices.IsEmpty());
			C.EncounterRow().bGrantsAbilityChoice = true;
			TestEqual(TEXT("Flag on"), Compute(EDwCombatOutcome::VictoryCrewDefeated).AbilityChoices.Num(), 3);
			TestTrue(TEXT("Boss"), Compute(EDwCombatOutcome::VictoryBoss).AbilityChoices.IsEmpty());
		});

		It("follows the pool weights", [this]()
		{
			C.HandPool->Abilities = { { C.Parry, 9.f }, { C.Rigging, 1.f } };
			C.CaptainPool->Abilities.Reset();
			int32 Parry = 0;
			for (int32 Seed = 0; Seed < 1000; ++Seed)
			{
				const TArray<FDwAbilityOffer> One = Roll(1, Seed);
				Parry += One.Num() == 1 && One[0].Ability == C.Parry ? 1 : 0;
			}
			TestTrue(*FString::Printf(TEXT("~90%% Parry (%d of 1000)"), Parry), Parry > 850 && Parry < 950);
			C.HandPool->Abilities = { { C.Parry, 0.f } };
			TestTrue(TEXT("Weight 0 is never offered"), Roll(3).IsEmpty());
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
