// Copyright Brandon Hillig 2026

// DW-103: pure outcome and fear rules (Docs/Architecture/03 "Outcome priority", 05 "Fear").

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/DwCombatRules.h"
#include "Combat/DwCombatSnapshot.h"
#include "Core/DwGameplayTags.h"

namespace DwOutcomeSpec
{
	/** Captain + one crew vs two enemies, both ships afloat. */
	static FDwCombatSnapshot Fight()
	{
		FDwCombatSnapshot S;
		auto Add = [&S](int32 Id, EDwSide Side, bool bCaptain)
		{
			FDwUnitSnapshot& U = S.Units.AddDefaulted_GetRef();
			U.Handle = FDwUnitHandle(Id);
			U.Side = Side;
			U.Health = U.MaxHealth = 100.f;
			U.MaxFear = 3.f;
			if (bCaptain)
			{
				U.Tags.AddTag(DwTags::Unit_Role_Captain);
			}
		};
		Add(1, EDwSide::Player, true);
		Add(2, EDwSide::Player, false);
		Add(3, EDwSide::Enemy, false);
		Add(4, EDwSide::Enemy, false);
		for (const EDwSide Side : { EDwSide::Player, EDwSide::Enemy })
		{
			FDwShipSnapshot& Ship = S.Ships.AddDefaulted_GetRef();
			Ship.Side = Side;
			Ship.Health = Ship.MaxHealth = 250.f;
		}
		return S;
	}

	static void Kill(FDwCombatSnapshot& S, int32 Id)
	{
		FDwUnitSnapshot& U = S.Units[Id - 1];
		U.bAlive = false;
		U.Health = 0.f;
		U.Tags.AddTag(DwTags::State_Dead);
	}

	static void Sink(FDwCombatSnapshot& S, EDwSide Side)
	{
		S.Ships.FindByPredicate([Side](const FDwShipSnapshot& Ship) { return Ship.Side == Side; })->Health = 0.f;
	}
}

BEGIN_DEFINE_SPEC(FDwOutcomeSpec, "DeadwaterRun.Rules.Outcome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FDwOutcomeConfig Config;
	EDwCombatOutcome Eval(const FDwCombatSnapshot& S) const { return FDwCombatOutcomeRules::Evaluate(S, Config); }
END_DEFINE_SPEC(FDwOutcomeSpec)

void FDwOutcomeSpec::Define()
{
	using namespace DwOutcomeSpec;

	BeforeEach([this]() { Config = FDwOutcomeConfig(); });

	Describe("Priority list", [this]()
	{
		It("is None while both sides stand", [this]()
		{
			TestEqual(TEXT("No outcome"), Eval(Fight()), EDwCombatOutcome::None);
		});

		It("1. player ship sunk -> DefeatShipSunk, even if the enemy ship sank too", [this]()
		{
			FDwCombatSnapshot S = Fight();
			Sink(S, EDwSide::Player);
			Sink(S, EDwSide::Enemy);
			TestEqual(TEXT("Defeat first"), Eval(S), EDwCombatOutcome::DefeatShipSunk);
		});

		It("2. no living crew -> DefeatCrewDead (before captain death)", [this]()
		{
			FDwCombatSnapshot S = Fight();
			Kill(S, 1);
			Kill(S, 2);
			TestEqual(TEXT("Crew dead"), Eval(S), EDwCombatOutcome::DefeatCrewDead);
		});

		It("3. captain dead with other crew alive -> DefeatCaptainDead (C10)", [this]()
		{
			FDwCombatSnapshot S = Fight();
			Kill(S, 1);
			TestEqual(TEXT("Captain dead"), Eval(S), EDwCombatOutcome::DefeatCaptainDead);
		});

		It("4. Kraken body dead in a boss fight -> VictoryBoss", [this]()
		{
			FDwCombatSnapshot S = Fight();
			S.Units[2].Tags.AddTag(DwTags::Unit_Kraken_Body);
			Kill(S, 3);
			TestEqual(TEXT("Not a boss fight: tentacle still alive, no outcome"), Eval(S), EDwCombatOutcome::None);
			Config.bIsBossFight = true;
			TestEqual(TEXT("Boss fight"), Eval(S), EDwCombatOutcome::VictoryBoss);
		});

		It("5. enemy ship sunk -> VictorySunk, even with enemies alive", [this]()
		{
			FDwCombatSnapshot S = Fight();
			Sink(S, EDwSide::Enemy);
			TestEqual(TEXT("Sunk"), Eval(S), EDwCombatOutcome::VictorySunk);
		});

		It("6. no living enemies -> VictoryCrewDefeated", [this]()
		{
			FDwCombatSnapshot S = Fight();
			Kill(S, 3);
			Kill(S, 4);
			TestEqual(TEXT("Plunder"), Eval(S), EDwCombatOutcome::VictoryCrewDefeated);
		});

		It("7. surrender only when enabled and every fear-capable enemy is at MaxFear", [this]()
		{
			FDwCombatSnapshot S = Fight();
			S.Units[2].Fear = 3.f;
			S.Units[3].Fear = 3.f;
			TestEqual(TEXT("Disabled: no surrender"), Eval(S), EDwCombatOutcome::None);
			Config.bSurrenderEnabled = true;
			TestEqual(TEXT("Enabled"), Eval(S), EDwCombatOutcome::VictorySurrender);
			S.Units[3].Fear = 2.f;
			TestEqual(TEXT("One enemy below max"), Eval(S), EDwCombatOutcome::None);
		});
	});

	Describe("Combined cases", [this]()
	{
		It("sinking the enemy ship with the last crew aboard is a defeat (C12)", [this]()
		{
			FDwCombatSnapshot S = Fight();
			Sink(S, EDwSide::Enemy);
			Kill(S, 1); // boarders died with the ship
			Kill(S, 2);
			TestEqual(TEXT("Defeat beats VictorySunk"), Eval(S), EDwCombatOutcome::DefeatCrewDead);
		});

		It("sinking the enemy ship while the captain boards is DefeatCaptainDead", [this]()
		{
			FDwCombatSnapshot S = Fight();
			Sink(S, EDwSide::Enemy);
			Kill(S, 1);
			TestEqual(TEXT("Captain went down with it"), Eval(S), EDwCombatOutcome::DefeatCaptainDead);
		});

		It("surrender ignores a fear-immune enemy, but not when it is the only one left", [this]()
		{
			Config.bSurrenderEnabled = true;
			FDwCombatSnapshot S = Fight();
			S.Units[2].Fear = 3.f;
			S.Units[3].Tags.AddTag(DwTags::Unit_Immune_Fear);
			TestEqual(TEXT("Immune enemy ignored"), Eval(S), EDwCombatOutcome::VictorySurrender);
			Kill(S, 3);
			TestEqual(TEXT("Only the immune enemy remains"), Eval(S), EDwCombatOutcome::None);
		});

		It("a dead enemy at max fear doesn't count toward surrender", [this]()
		{
			Config.bSurrenderEnabled = true;
			FDwCombatSnapshot S = Fight();
			S.Units[2].Fear = 3.f;
			Kill(S, 3);
			TestEqual(TEXT("Living enemy not afraid"), Eval(S), EDwCombatOutcome::None);
		});
	});

	Describe("Fear rules", [this]()
	{
		It("cowers at MaxFear unless immune", [this]()
		{
			TestFalse(TEXT("2 of 3"), FDwFearRules::ShouldCower(2.f, 3.f, false));
			TestTrue(TEXT("3 of 3"), FDwFearRules::ShouldCower(3.f, 3.f, false));
			TestFalse(TEXT("Immune"), FDwFearRules::ShouldCower(3.f, 3.f, true));
			TestFalse(TEXT("No fear track"), FDwFearRules::ShouldCower(0.f, 0.f, false));
		});

		It("applies each after-cower policy", [this]()
		{
			TestEqual(TEXT("Reset"), FDwFearRules::FearAfterCower(3.f, EDwFearCowerPolicy::ResetToZero), 0.f);
			TestEqual(TEXT("Stay"), FDwFearRules::FearAfterCower(3.f, EDwFearCowerPolicy::StayAtMax), 3.f);
			TestEqual(TEXT("Drop to one"), FDwFearRules::FearAfterCower(3.f, EDwFearCowerPolicy::DropToOne), 1.f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
