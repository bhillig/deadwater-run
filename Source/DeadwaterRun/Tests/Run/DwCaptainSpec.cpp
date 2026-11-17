// Copyright Brandon Hillig 2026

// DW-206..208: the three captains with the real content (Docs/Architecture/05-Crew-Captains.md "How it's tested"):
// pure FDwCaptainRules over the real captain definitions, then their abilities and rules in real run fights.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/DwAttributeSets.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Run/DwCaptainRules.h"
#include "Tests/Run/DwRunFightFixture.h"

BEGIN_DEFINE_SPEC(FDwCaptainSpec, "DeadwaterRun.Run.Captains",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	FDwRunFight R;

	float Fear(FDwUnitHandle Unit) const { return R.F.Attr(Unit, UDwUnitAttributeSet::GetFearAttribute()); }
	float Health(FDwUnitHandle Unit) const { return R.F.Health(Unit); }

	float TotalEnemyHealth() const
	{
		float Total = 0.f;
		for (const FDwUnitHandle Enemy : R.Units(EDwSide::Enemy, /*bAliveOnly*/ false))
		{
			Total += Health(Enemy);
		}
		return Total;
	}
END_DEFINE_SPEC(FDwCaptainSpec)

void FDwCaptainSpec::Define()
{
	BeforeEach([this]() { R.Create(*this); });
	AfterEach([this]() { R.Destroy(); });

	Describe("Fearmongerer (DW-206)", [this]()
	{
		It("Dread Reputation: starting Fear = surrender wins x FearPerSurrender, capped at 2 (K9), Fearmongerer only", [this]()
		{
			const UDwCaptainDefinition* Fearmongerer = UDwRunSubsystem::FindCaptain(TEXT("Fearmongerer"));
			const UDwCaptainDefinition* TeamLeader = UDwRunSubsystem::FindCaptain(TEXT("TeamLeader"));
			if (!TestNotNull(TEXT("Fearmongerer"), Fearmongerer) || !TestNotNull(TEXT("Team Leader"), TeamLeader)) { return; }
			FDwCaptainCounters Counters;
			TestEqual(TEXT("No wins"), FDwCaptainRules::GetStartingFear(Counters, Fearmongerer), 0.f);
			FDwCaptainRules::OnCombatWon(Counters, EDwCombatOutcome::VictorySunk);
			FDwCaptainRules::OnCombatWon(Counters, EDwCombatOutcome::VictoryCrewDefeated);
			TestEqual(TEXT("Only surrenders count"), Counters.SurrenderWins, 0);
			FDwCaptainRules::OnCombatWon(Counters, EDwCombatOutcome::VictorySurrender);
			TestEqual(TEXT("1 win"), FDwCaptainRules::GetStartingFear(Counters, Fearmongerer), 1.f);
			Counters.SurrenderWins = 5;
			TestEqual(TEXT("Capped at 2"), FDwCaptainRules::GetStartingFear(Counters, Fearmongerer), 2.f);
			TestEqual(TEXT("Not the Team Leader's rule"), FDwCaptainRules::GetStartingFear(Counters, TeamLeader), 0.f);
		});

		It("Smoke and Fire: every enemy gains 1 Fear; on the next cast the afraid take 20 first", [this]()
		{
			if (!R.StartRun(TEXT("Fearmongerer")) || !R.StartNextFight()) { return; }
			R.MakeEnemiesPassive();
			const UDwAbilityDefinition* Smoke = R.CaptainAbility(DwTags::Ability_Captain_SmokeAndFire);
			const TArray<FDwUnitHandle> Enemies = R.Units(EDwSide::Enemy);
			if (!TestNotNull(TEXT("Smoke and Fire"), Smoke) || !TestTrue(TEXT("Enemies"), Enemies.Num() > 0)) { return; }

			const float Before = TotalEnemyHealth();
			TestEqual(TEXT("Cast"), R.Use(R.CaptainUnit(), Smoke, Enemies[0]), EDwCommandResult::Accepted);
			for (const FDwUnitHandle Enemy : Enemies)
			{
				TestEqual(TEXT("1 Fear each"), Fear(Enemy), 1.f);
			}
			TestEqual(TEXT("Nobody was afraid yet: no damage"), TotalEnemyHealth(), Before);

			R.PassRound();
			TestEqual(TEXT("Cast again"), R.Use(R.CaptainUnit(), Smoke, Enemies[0]), EDwCommandResult::Accepted);
			for (const FDwUnitHandle Enemy : Enemies)
			{
				TestEqual(TEXT("2 Fear each"), Fear(Enemy), 2.f);
				TestEqual(TEXT("20 smoke damage each"), Health(Enemy), R.F.Attr(Enemy, UDwUnitAttributeSet::GetMaxHealthAttribute()) - 20.f);
			}
		});

		It("wins by surrender: a third Smoke and Fire puts every enemy at 3 Fear, nobody dies, and the next fight starts afraid", [this]()
		{
			if (!R.StartRun(TEXT("Fearmongerer")) || !R.StartNextFight()) { return; }
			R.MakeEnemiesPassive();
			const UDwAbilityDefinition* Smoke = R.CaptainAbility(DwTags::Ability_Captain_SmokeAndFire);
			const TArray<FDwUnitHandle> Enemies = R.Units(EDwSide::Enemy);
			if (!TestNotNull(TEXT("Smoke and Fire"), Smoke)) { return; }
			for (int32 Cast = 0; Cast < 3; ++Cast)
			{
				if (Cast > 0)
				{
					R.PassRound();
				}
				TestEqual(*FString::Printf(TEXT("Cast %d"), Cast + 1), R.Use(R.CaptainUnit(), Smoke, Enemies[0]), EDwCommandResult::Accepted);
			}
			TestEqual(TEXT("They surrender"), R.F.Combat->GetOutcome(), EDwCombatOutcome::VictorySurrender);
			TestEqual(TEXT("Every enemy alive"), R.Units(EDwSide::Enemy).Num(), Enemies.Num());
			if (!R.FinishFight()) { return; }
			TestEqual(TEXT("Counted"), R.Run->GetState().Counters.SurrenderWins, 1);
			TestEqual(TEXT("Stats"), R.Run->GetState().Stats.FightsSurrendered, 1);

			FDwEncounterSetup Next;
			if (!R.StartNextFight(&Next)) { return; }
			for (const FDwCombatantSetup& Enemy : Next.Enemies)
			{
				TestEqual(TEXT("Setup: Dread Reputation 1"), Enemy.StartingFear, 1.f);
			}
			for (const FDwUnitHandle Enemy : R.Units(EDwSide::Enemy))
			{
				TestEqual(TEXT("Spawned afraid"), Fear(Enemy), 1.f);
			}
		});

		It("Six Pistols: six 15-damage shots at random enemies in sight (90 in total)", [this]()
		{
			if (!R.StartRun(TEXT("Fearmongerer")) || !R.StartNextFight()) { return; }
			R.MakeEnemiesPassive();
			const UDwAbilityDefinition* Pistols = R.CaptainAbility(DwTags::Ability_Captain_SixPistols);
			const TArray<FDwUnitHandle> Enemies = R.Units(EDwSide::Enemy);
			if (!TestNotNull(TEXT("Six Pistols"), Pistols)) { return; }
			const float Before = TotalEnemyHealth();
			TestEqual(TEXT("Fired"), R.Use(R.CaptainUnit(), Pistols, Enemies[0]), EDwCommandResult::Accepted);
			TestEqual(TEXT("6 x 15"), Before - TotalEnemyHealth(), 90.f);
			for (const FDwUnitHandle Enemy : Enemies)
			{
				const float Lost = R.F.Attr(Enemy, UDwUnitAttributeSet::GetMaxHealthAttribute()) - Health(Enemy);
				TestEqual(TEXT("Each enemy took a multiple of 15"), FMath::Fmod(Lost, 15.f), 0.f);
			}
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
