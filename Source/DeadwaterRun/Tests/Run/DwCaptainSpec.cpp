// Copyright Brandon Hillig 2026

// DW-206..208: the three captains with the real content (Docs/Architecture/05-Crew-Captains.md "How it's tested"):
// pure FDwCaptainRules over the real captain definitions, then their abilities and rules in real run fights.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/DwAttributeSets.h"
#include "Abilities/DwDamageRules.h"
#include "Abilities/DwGA_Grapple.h"
#include "Board/DwGridQueries.h"
#include "Combat/DwCombatRules.h"
#include "Combat/DwCombatSnapshot.h"
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

	FIntPoint TileOf(FDwUnitHandle Unit) const { return R.F.Unit(Unit)->GetTile(); }

	/** A free standable tile 4-way next to Of (closest to Toward), or unset. */
	TOptional<FIntPoint> FreeBeside(FDwUnitHandle Of, FIntPoint Toward) const
	{
		return FDwTargetingQuery::FreeTileAdjacentTo(R.F.Combat->GetSnapshot().Grid, TileOf(Of), Toward);
	}

	/** A free standable tile on Side's deck at least MinDistance from every tile in Away. */
	TOptional<FIntPoint> FreeTileAwayFrom(EDwSide Side, TArray<FIntPoint> Away, int32 MinDistance = 2) const
	{
		const FDwGridModel Grid = R.F.Combat->GetSnapshot().Grid;
		for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
		{
			const FIntPoint Tile = Grid.ToTile(Index);
			const FDwTile* Data = Grid.GetTile(Tile);
			if (!Data || Data->Owner != Side || Data->Elevation > 0 || !Grid.IsStandable(Tile) || Grid.IsOccupied(Tile))
			{
				continue;
			}
			if (!Away.ContainsByPredicate([Tile, MinDistance](FIntPoint A) { return FMath::Abs(A.X - Tile.X) + FMath::Abs(A.Y - Tile.Y) < MinDistance; }))
			{
				return Tile;
			}
		}
		return {};
	}

	/** Preview of Source's ability Ability at Target from the live fight. */
	FDwAbilityPreview Preview(FDwUnitHandle Source, const UDwAbilityDefinition* Ability, FDwUnitHandle Target) const
	{
		const FDwCombatSnapshot Snapshot = R.F.Combat->GetSnapshot();
		const FDwTargetingContext Context = R.F.Context(Snapshot, Source, Ability);
		return FDwCombatQueries::GetAbilityCDO(Ability)->PreviewOutcome(Context, FDwAbilityTarget::MakeUnit(Target, TileOf(Target)));
	}

	const UDwAbilityDefinition* BasicAttack(FDwUnitHandle Unit) const
	{
		const ADwCombatUnit* Actor = R.F.Unit(Unit);
		return Actor && Actor->GetDefinition() ? Actor->GetDefinition()->BasicAttack.Get() : nullptr;
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

	Describe("Team Leader (DW-207)", [this]()
	{
		It("synergy rules: a 4-way neighbour ally counts, a diagonal one doesn't; never cannons, the enemy side, or without the rule", [this]()
		{
			FDwCombatSnapshot Snapshot;
			auto Add = [&Snapshot](int32 Id, EDwSide Side, FIntPoint Tile) -> FDwUnitSnapshot&
			{
				FDwUnitSnapshot& U = Snapshot.Units.AddDefaulted_GetRef();
				U.Handle = FDwUnitHandle(Id);
				U.Side = Side;
				U.Tile = Tile;
				return U;
			};
			Add(1, EDwSide::Player, FIntPoint(2, 2));
			FDwUnitSnapshot& Ally = Add(2, EDwSide::Player, FIntPoint(3, 3)); // diagonal
			Add(3, EDwSide::Enemy, FIntPoint(2, 3));
			Add(4, EDwSide::Enemy, FIntPoint(2, 4));
			UDwAbilityDefinition* Sword = R.F.Root(NewObject<UDwAbilityDefinition>());
			Sword->AbilityTag = DwTags::Ability_Attack_Melee;
			UDwAbilityDefinition* Cannon = R.F.Root(NewObject<UDwAbilityDefinition>());
			Cannon->AbilityTag = DwTags::Ability_Cannon;
			Snapshot.CaptainRules.RuleTags.AddTag(DwTags::Rule_Captain_Synergy);
			Snapshot.CaptainRules.SynergyDamageMultiplier = 1.2f;

			TestFalse(TEXT("Diagonal ally"), FDwSynergyRules::IsActive(Snapshot, FDwUnitHandle(1), Sword));
			TestFalse(TEXT("An adjacent enemy is no ally"), FDwSynergyRules::HasAdjacentAlly(Snapshot, Snapshot.Units[0]));
			Ally.Tile = FIntPoint(3, 2);
			TestTrue(TEXT("4-way ally"), FDwSynergyRules::IsActive(Snapshot, FDwUnitHandle(1), Sword));
			TestEqual(TEXT("x1.2"), FDwSynergyRules::GetMultiplier(Snapshot, FDwUnitHandle(1), Sword), 1.2f);
			TestFalse(TEXT("Not cannons (K4a)"), FDwSynergyRules::IsActive(Snapshot, FDwUnitHandle(1), Cannon));
			TestFalse(TEXT("Enemies side by side get nothing"), FDwSynergyRules::IsActive(Snapshot, FDwUnitHandle(3), Sword));
			Snapshot.Units[1].bAlive = false;
			TestFalse(TEXT("A dead neighbour doesn't count"), FDwSynergyRules::IsActive(Snapshot, FDwUnitHandle(1), Sword));
			Snapshot.Units[1].bAlive = true;
			Snapshot.CaptainRules = FDwCaptainCombatRules();
			TestEqual(TEXT("No Team Leader, no synergy"), FDwSynergyRules::GetMultiplier(Snapshot, FDwUnitHandle(1), Sword), 1.f);
		});

		It("synergy in a real fight: x1.2 with an ally beside, the preview is the result, and the HUD tag follows", [this]()
		{
			if (!R.StartRun(TEXT("TeamLeader")) || !R.StartNextFight()) { return; }
			R.MakeEnemiesPassive();
			const FDwUnitHandle Captain = R.CaptainUnit();
			const FDwUnitHandle Swordsman = R.FindCrewUnit(R.Run->GetState().Crew[1].Id);
			const FDwUnitHandle Sniper = R.FindCrewUnit(R.Run->GetState().Crew[2].Id);
			const FDwUnitHandle Enemy = R.Units(EDwSide::Enemy)[0];
			const UDwAbilityDefinition* Cutlass = BasicAttack(Captain);
			if (!TestNotNull(TEXT("Cutlass"), Cutlass)) { return; }

			// Alone first: both crewmates away, the enemy beside the captain.
			const TOptional<FIntPoint> Away1 = FreeTileAwayFrom(EDwSide::Player, { TileOf(Captain) }, 3);
			if (!TestTrue(TEXT("Room away"), Away1.IsSet())) { return; }
			R.F.Combat->MoveUnit(Swordsman, Away1.GetValue(), true);
			const TOptional<FIntPoint> Away2 = FreeTileAwayFrom(EDwSide::Player, { TileOf(Captain), TileOf(Swordsman) }, 3);
			if (!TestTrue(TEXT("Room away 2"), Away2.IsSet())) { return; }
			R.F.Combat->MoveUnit(Sniper, Away2.GetValue(), true);
			const TOptional<FIntPoint> Beside = FreeBeside(Captain, TileOf(Enemy));
			if (!TestTrue(TEXT("Room beside the captain"), Beside.IsSet())) { return; }
			R.F.Combat->MoveUnit(Enemy, Beside.GetValue(), true);
			const int32 Alone = Preview(Captain, Cutlass, Enemy).Targets[0].Damage;

			// Then a crewmate steps up beside him.
			const TOptional<FIntPoint> Ally = FreeBeside(Captain, TileOf(Swordsman));
			if (!TestTrue(TEXT("Room for the ally"), Ally.IsSet())) { return; }
			R.F.Combat->MoveUnit(Swordsman, Ally.GetValue(), true);
			const FDwAbilityPreview With = Preview(Captain, Cutlass, Enemy);
			TestEqual(TEXT("x1.2 (rounded)"), With.Targets[0].Damage, FMath::RoundToInt(Alone * 1.2f));
			const float Before = Health(Enemy);
			TestEqual(TEXT("Attack"), R.Use(Captain, Cutlass, Enemy), EDwCommandResult::Accepted);
			TestEqual(TEXT("Preview = result"), Before - Health(Enemy), static_cast<float>(With.Targets[0].Damage));
			TestTrue(TEXT("HUD tag on the captain"), R.F.Asc(Captain)->HasMatchingGameplayTag(DwTags::State_Synergy_Adjacent));
			TestFalse(TEXT("Never on enemies"), R.F.Asc(Enemy)->HasMatchingGameplayTag(DwTags::State_Synergy_Adjacent));
		});

		It("Defend: the guarded crewmate's single-target hit lands on the captain for half damage", [this]()
		{
			if (!R.StartRun(TEXT("TeamLeader")) || !R.StartNextFight()) { return; }
			const FDwUnitHandle Captain = R.CaptainUnit();
			const FDwUnitHandle Swordsman = R.FindCrewUnit(R.Run->GetState().Crew[1].Id);
			const UDwAbilityDefinition* Defend = nullptr;
			for (const UDwAbilityDefinition* Ability : R.Run->GetState().Captain->RuleAbilities)
			{
				Defend = Ability && Ability->AbilityTag == DwTags::Ability_Captain_Defend ? Ability : Defend;
			}
			if (!TestNotNull(TEXT("Defend (rule ability)"), Defend)) { return; }

			const TOptional<FIntPoint> Beside = FreeBeside(Captain, TileOf(Swordsman));
			if (!TestTrue(TEXT("Room beside"), Beside.IsSet())) { return; }
			R.F.Combat->MoveUnit(Swordsman, Beside.GetValue(), true);
			TestEqual(TEXT("Can't guard himself"), R.Use(Captain, Defend, Captain), EDwCommandResult::InvalidTarget);
			TestEqual(TEXT("Guard"), R.Use(Captain, Defend, Swordsman), EDwCommandResult::Accepted);
			TestTrue(TEXT("Guarded"), R.F.Asc(Swordsman)->HasMatchingGameplayTag(DwTags::Status_Guarded));

			// The first enemy to act steps next to the swordsman and swings at him.
			UDwTestScriptedController* Enemies = R.F.Root(NewObject<UDwTestScriptedController>(R.F.Combat));
			float Expected = -1.f;
			bool bStruck = false;
			Enemies->OnUnitTurn = [&](UDwCombatSubsystem& Combat, FDwUnitHandle Unit)
			{
				if (bStruck)
				{
					return;
				}
				bStruck = true;
				const TOptional<FIntPoint> Next = FreeBeside(Swordsman, TileOf(Unit));
				const UDwAbilityDefinition* Attack = BasicAttack(Unit);
				if (!Next.IsSet() || !Attack)
				{
					return;
				}
				Combat.MoveUnit(Unit, Next.GetValue(), true);
				const FDwAbilityPreview Hit = Preview(Unit, Attack, Swordsman);
				const FDwAbilityTuningRow* Row = Attack->GetTuningRow<FDwAbilityTuningRow>();
				const FDwCombatSnapshot Now = Combat.GetSnapshot();
				const FDwUnitSnapshot* Attacker = Now.FindUnit(Unit);
				const FDwUnitSnapshot* Guardian = Now.FindUnit(Captain);
				if (Row && Attacker && Guardian && Hit.Targets.Num() == 1 && Hit.Targets[0].Target.Unit == Captain && Hit.Targets[0].Target.bGuarded)
				{
					FDwDamageInput Input; // the hit on the captain, at the guard multiplier (K5: half)
					Input.Flat = Row->Damage;
					Input.Scale = Row->DamageScale;
					Input.AttackDamage = Attacker->AttackDamage;
					Input.DamageBonus = Attacker->DamageBonus;
					Input.DamageReduction = Guardian->DamageReduction;
					Input.SituationalMultiplier = 0.5f;
					TestEqual(TEXT("Half damage on the captain"), Hit.Targets[0].Damage, FDwDamageRules::Compute(Input).Damage);
					Expected = static_cast<float>(Hit.Targets[0].Damage);
				}
				R.Use(Unit, Attack, Swordsman);
			};
			R.F.Combat->SetSideController(EDwSide::Enemy, Enemies);
			const float CaptainBefore = Health(Captain);
			const float SwordsmanBefore = Health(Swordsman);
			R.PassRound();
			if (!TestTrue(TEXT("The preview showed the redirect"), Expected > 0.f)) { return; }
			TestEqual(TEXT("The captain took it"), CaptainBefore - Health(Captain), Expected);
			TestEqual(TEXT("The swordsman didn't"), Health(Swordsman), SwordsmanBefore);
		});

		It("Rally Cry heals every crewmate 15 (up to max) and clears their debuffs", [this]()
		{
			if (!R.StartRun(TEXT("TeamLeader")) || !R.StartNextFight()) { return; }
			R.MakeEnemiesPassive();
			const UDwAbilityDefinition* Rally = R.CaptainAbility(DwTags::Ability_Captain_RallyCry);
			if (!TestNotNull(TEXT("Rally Cry"), Rally)) { return; }
			const TArray<FDwUnitHandle> Crew = R.Units(EDwSide::Player);
			for (const FDwUnitHandle Unit : Crew)
			{
				R.F.Asc(Unit)->SetNumericAttributeBase(UDwUnitAttributeSet::GetHealthAttribute(), 50.f);
			}
			R.F.Asc(Crew[0])->SetNumericAttributeBase(UDwUnitAttributeSet::GetHealthAttribute(), R.F.Attr(Crew[0], UDwUnitAttributeSet::GetMaxHealthAttribute()) - 5.f);
			R.F.ApplyEffect(DwGas::GetSharedEffects().Rooted, Crew.Last());
			TestTrue(TEXT("Rooted"), R.F.Asc(Crew.Last())->HasMatchingGameplayTag(DwTags::Status_Rooted));
			TestEqual(TEXT("Rally"), R.Use(R.CaptainUnit(), Rally, Crew.Last()), EDwCommandResult::Accepted);
			TestEqual(TEXT("Capped at max"), Health(Crew[0]), R.F.Attr(Crew[0], UDwUnitAttributeSet::GetMaxHealthAttribute()));
			for (int32 Index = 1; Index < Crew.Num(); ++Index)
			{
				TestEqual(TEXT("+15"), Health(Crew[Index]), 65.f);
			}
			TestFalse(TEXT("Cleansed"), R.F.Asc(Crew.Last())->HasMatchingGameplayTag(DwTags::Status_Rooted));
		});

		It("Grapple Hook pulls an enemy across the ships to the tile beside the captain; never the Kraken", [this]()
		{
			if (!R.StartRun(TEXT("TeamLeader")) || !R.StartNextFight()) { return; }
			R.MakeEnemiesPassive();
			const UDwAbilityDefinition* Hook = R.CaptainAbility(DwTags::Ability_Captain_GrappleHook);
			if (!TestNotNull(TEXT("Grapple Hook"), Hook)) { return; }
			const FDwUnitHandle Captain = R.CaptainUnit();
			// Spawns are out of hook range: put the captain on the free player-deck tile closest to an enemy (not beside it).
			{
				const FIntPoint EnemyTile = TileOf(R.Units(EDwSide::Enemy)[0]);
				const FDwGridModel Grid = R.F.Combat->GetSnapshot().Grid;
				TOptional<FIntPoint> Best;
				int32 BestDistance = MAX_int32;
				for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
				{
					const FIntPoint Tile = Grid.ToTile(Index);
					const FDwTile* Data = Grid.GetTile(Tile);
					const int32 Distance = FMath::Abs(Tile.X - EnemyTile.X) + FMath::Abs(Tile.Y - EnemyTile.Y);
					if (Data && Data->Owner == EDwSide::Player && Data->Elevation == 0 && Grid.IsStandable(Tile) && !Grid.IsOccupied(Tile)
						&& Distance > 2 && Distance < BestDistance)
					{
						Best = Tile;
						BestDistance = Distance;
					}
				}
				if (!TestTrue(TEXT("A tile toward the enemy"), Best.IsSet())) { return; }
				R.F.Combat->MoveUnit(Captain, Best.GetValue(), true);
			}
			const FDwCombatSnapshot Snapshot = R.F.Combat->GetSnapshot();
			TArray<FDwAbilityTarget> Valid;
			FDwCombatQueries::GetAbilityCDO(Hook)->GatherValidTargets(R.F.Context(Snapshot, Captain, Hook), Valid);
			const FDwAbilityTarget* EnemyTarget = Valid.FindByPredicate([&Snapshot](const FDwAbilityTarget& T)
				{ const FDwUnitSnapshot* U = Snapshot.FindUnit(T.Unit); return U && U->Side == EDwSide::Enemy; });
			if (!TestNotNull(TEXT("An enemy in sight and range"), EnemyTarget)) { return; }
			const FDwUnitHandle Pulled = EnemyTarget->Unit;
			const FIntPoint From = TileOf(Pulled);
			const TOptional<FIntPoint> Landing = UDwGA_Grapple::GetLandingTile(Snapshot.Grid, TileOf(Captain), From);
			TestEqual(TEXT("Pulled"), R.Use(Captain, Hook, Pulled), EDwCommandResult::Accepted);
			TestEqual(TEXT("Beside the captain"), TileOf(Pulled), Landing.GetValue());
			TestNotEqual(TEXT("It moved"), TileOf(Pulled), From);
			const FIntPoint Delta = TileOf(Pulled) - TileOf(Captain);
			TestEqual(TEXT("4-way adjacent"), FMath::Abs(Delta.X) + FMath::Abs(Delta.Y), 1);
			TestEqual(TEXT("The grid knows"), R.F.Combat->GetSnapshot().Grid.GetOccupant(TileOf(Pulled)).Unit, Pulled);
			const FDwGridModel After = R.F.Combat->GetSnapshot().Grid;
			TestEqual(TEXT("Landed at the captain's height (never into the nest)"), After.GetTile(TileOf(Pulled))->Elevation, After.GetTile(TileOf(Captain))->Elevation);

			const UDwUnitDefinition* Body = LoadObject<UDwUnitDefinition>(nullptr, TEXT("/Game/DeadwaterRun/Data/Units/DA_Unit_KrakenBody.DA_Unit_KrakenBody"));
			TestTrue(TEXT("Kraken: no grapple, no Fear (K10)"), Body && Body->UnitTags.HasTagExact(DwTags::Unit_Immune_Grapple)
				&& Body->UnitTags.HasTagExact(DwTags::Unit_Immune_Fear));
			TestTrue(TEXT("Grapple blocks immune units"), FDwCombatQueries::GetAbilityCDO(Hook)->Targeting.BlockedTargetTags.HasTagExact(DwTags::Unit_Immune_Grapple));
		});
	});

	Describe("Conqueror (DW-208)", [this]()
	{
		It("kill bonus rules: +1% per own kill up to 20%, tentacles don't count, nothing without the rule", [this]()
		{
			const UDwCaptainDefinition* Conqueror = UDwRunSubsystem::FindCaptain(TEXT("Conqueror"));
			if (!TestNotNull(TEXT("Conqueror"), Conqueror)) { return; }
			FDwCaptainCounters Counters;
			const FDwCaptainCombatRules Rules = FDwCaptainRules::GetCombatRules(Conqueror, Counters);
			const FGameplayTagContainer Pirate;
			const FGameplayTagContainer Tentacle(DwTags::Unit_Kraken_Tentacle);
			TestEqual(TEXT("+1%"), FDwCaptainRules::OnEnemyKilled(0.f, Rules, Pirate), 0.01f, 1e-6f);
			TestEqual(TEXT("Tentacle ignored (K3)"), FDwCaptainRules::OnEnemyKilled(0.05f, Rules, Tentacle), 0.05f, 1e-6f);
			TestEqual(TEXT("Capped at 20%"), FDwCaptainRules::OnEnemyKilled(0.2f, Rules, Pirate), 0.2f, 1e-6f);
			Counters.ConquerorKillBonusPct = 0.07f;
			TestEqual(TEXT("The run's bonus goes into the fight"), FDwCaptainRules::GetCombatRules(Conqueror, Counters).KillBonusPct, 0.07f, 1e-6f);
			const FDwCaptainCombatRules None = FDwCaptainRules::GetCombatRules(UDwRunSubsystem::FindCaptain(TEXT("TeamLeader")), Counters);
			TestEqual(TEXT("No rule, no bonus"), FDwCaptainRules::OnEnemyKilled(0.f, None, Pirate), 0.f);
			TestTrue(TEXT("Never offered firearms (K2)"), Conqueror->BlockedAbilityTags.HasTagExact(DwTags::Ability_Weapon_Firearm));
		});

		It("two actions for him (act, move, act; a third is refused) and one choice per turn for his crew", [this]()
		{
			if (!R.StartRun(TEXT("Conqueror")) || !R.StartNextFight()) { return; }
			R.MakeEnemiesPassive();
			const FDwUnitHandle Captain = R.CaptainUnit();
			const FDwUnitHandle Swordsman = R.FindCrewUnit(R.Run->GetState().Crew[1].Id);
			TestEqual(TEXT("2 actions"), R.F.Attr(Captain, UDwUnitAttributeSet::GetMaxActionPointsAttribute()), 2.f);
			TestTrue(TEXT("Crew: single choice"), R.F.Asc(Swordsman)->HasMatchingGameplayTag(DwTags::Rule_Turn_SingleChoice));
			TestFalse(TEXT("Not the captain"), R.F.Asc(Captain)->HasMatchingGameplayTag(DwTags::Rule_Turn_SingleChoice));

			// The Eye of the Tiger reaches any enemy: a cheap way to spend an action without moving anyone.
			const UDwAbilityDefinition* Eye = R.CaptainAbility(DwTags::Ability_Captain_EyeOfTheTiger);
			const TArray<FDwUnitHandle> Enemies = R.Units(EDwSide::Enemy);
			if (!TestNotNull(TEXT("Eye of the Tiger"), Eye) || !TestTrue(TEXT("3 enemies"), Enemies.Num() >= 3)) { return; }
			TestEqual(TEXT("Act"), R.Use(Captain, Eye, Enemies[0]), EDwCommandResult::Accepted);
			const FDwCombatSnapshot Snap = R.F.Combat->GetSnapshot();
			TOptional<FIntPoint> Near;
			for (const FIntPoint& Dir : FDwGridModel::Directions)
			{
				const FIntPoint Tile = TileOf(Captain) + Dir;
				if (!Near.IsSet() && Snap.Grid.IsStandable(Tile) && !Snap.Grid.IsOccupied(Tile) && Snap.Grid.GetTile(Tile)->Elevation == 0)
				{
					Near = Tile;
				}
			}
			if (!TestTrue(TEXT("A free tile beside him"), Near.IsSet())) { return; }
			TestEqual(TEXT("Move"), R.F.MoveTo(Captain, Near.GetValue()), EDwCommandResult::Accepted);
			TestEqual(TEXT("Act again"), R.Use(Captain, Eye, Enemies[1]), EDwCommandResult::Accepted);
			TestEqual(TEXT("A third action is refused"), R.Use(Captain, Eye, Enemies[2]), EDwCommandResult::CannotAfford);

			// The swordsman moves: his action is gone too.
			TOptional<FIntPoint> SwordStep;
			const FDwCombatSnapshot Now = R.F.Combat->GetSnapshot();
			for (const FIntPoint& Dir : FDwGridModel::Directions)
			{
				const FIntPoint Tile = TileOf(Swordsman) + Dir;
				if (!SwordStep.IsSet() && Now.Grid.IsStandable(Tile) && !Now.Grid.IsOccupied(Tile) && Now.Grid.GetTile(Tile)->Elevation == 0)
				{
					SwordStep = Tile;
				}
			}
			if (!TestTrue(TEXT("Room to step"), SwordStep.IsSet())) { return; }
			TestEqual(TEXT("Crew moves"), R.F.MoveTo(Swordsman, SwordStep.GetValue()), EDwCommandResult::Accepted);
			TestEqual(TEXT("...and can't act after"), R.F.Attr(Swordsman, UDwUnitAttributeSet::GetActionPointsAttribute()), 0.f);
		});

		It("Eye of the Tiger freezes one enemy anywhere for a turn; never the Kraken body (K10)", [this]()
		{
			if (!R.StartRun(TEXT("Conqueror")) || !R.StartNextFight()) { return; }
			R.MakeEnemiesPassive();
			const UDwAbilityDefinition* Eye = R.CaptainAbility(DwTags::Ability_Captain_EyeOfTheTiger);
			const TArray<FDwUnitHandle> Enemies = R.Units(EDwSide::Enemy);
			if (!TestNotNull(TEXT("Eye"), Eye)) { return; }
			// The farthest enemy: range and sight don't matter.
			FDwUnitHandle Far = Enemies[0];
			for (const FDwUnitHandle Enemy : Enemies)
			{
				Far = (TileOf(Enemy) - TileOf(R.CaptainUnit())).Size() > (TileOf(Far) - TileOf(R.CaptainUnit())).Size() ? Enemy : Far;
			}
			TestEqual(TEXT("Freeze"), R.Use(R.CaptainUnit(), Eye, Far), EDwCommandResult::Accepted);
			TestTrue(TEXT("Frozen"), R.F.Asc(Far)->HasMatchingGameplayTag(DwTags::Status_Incapacitated_Frozen));
			const UDwUnitDefinition* Body = LoadObject<UDwUnitDefinition>(nullptr, TEXT("/Game/DeadwaterRun/Data/Units/DA_Unit_KrakenBody.DA_Unit_KrakenBody"));
			TestTrue(TEXT("Kraken body immune to freeze"), Body && Body->UnitTags.HasTagExact(DwTags::Unit_Immune_Freeze));
			TestTrue(TEXT("Eye blocks it"), FDwCombatQueries::GetAbilityCDO(Eye)->Targeting.BlockedTargetTags.HasTagExact(DwTags::Unit_Immune_Freeze));
		});

		It("Charge: dashes the straight line, stops before the first enemy and hits it; a kill raises his damage at once and for the run", [this]()
		{
			if (!R.StartRun(TEXT("Conqueror")) || !R.StartNextFight()) { return; }
			R.MakeEnemiesPassive();
			const FDwUnitHandle Captain = R.CaptainUnit();
			const UDwAbilityDefinition* Charge = R.CaptainAbility(DwTags::Ability_Captain_Charge);
			const TArray<FDwUnitHandle> Enemies = R.Units(EDwSide::Enemy);
			if (!TestNotNull(TEXT("Charge"), Charge)) { return; }

			// Open a lane: the first direction with at least 3 free tiles, the enemy at its end.
			const FDwCombatSnapshot Snap = R.F.Combat->GetSnapshot();
			TOptional<FIntPoint> EnemyTile;
			FIntPoint Expected = FIntPoint::ZeroValue;
			for (const FIntPoint& Dir : FDwGridModel::Directions)
			{
				const FDwLineHit Line = FDwTargetingQuery::FirstUnitAlongLine(Snap.Grid, Snap.GridRules, TileOf(Captain), Dir, 6);
				const int32 Length = FMath::Abs(Line.StopTile.X - TileOf(Captain).X) + FMath::Abs(Line.StopTile.Y - TileOf(Captain).Y);
				if (!EnemyTile.IsSet() && !Line.Unit.IsSet() && Length >= 3)
				{
					EnemyTile = Line.StopTile;
					Expected = Line.StopTile - Dir;
				}
			}
			if (!TestTrue(TEXT("A lane of 3+ free tiles"), EnemyTile.IsSet())) { return; }
			const FDwUnitHandle Victim = Enemies[0];
			R.F.Combat->MoveUnit(Victim, EnemyTile.GetValue(), true);
			R.F.Asc(Victim)->SetNumericAttributeBase(UDwUnitAttributeSet::GetHealthAttribute(), 30.f); // the 40-damage hit kills

			const FDwAbilityPreview ChargePreview = Preview(Captain, Charge, Victim);
			TestTrue(TEXT("Lethal preview"), ChargePreview.bValid && ChargePreview.Targets.Num() == 1 && ChargePreview.Targets[0].bLethal);
			TestEqual(TEXT("Charge"), R.Use(Captain, Charge, Victim), EDwCommandResult::Accepted);
			TestEqual(TEXT("Stopped before the enemy"), TileOf(Captain), Expected);
			TestFalse(TEXT("Killed"), R.F.Unit(Victim)->IsAlive());
			TestEqual(TEXT("+1% damage at once"), R.F.Attr(Captain, UDwUnitAttributeSet::GetDamageBonusAttribute()), 0.01f, 1e-5f);

			// His second action shows the bonus straight away (preview = what lands).
			const FDwUnitHandle Next = Enemies[1];
			const TOptional<FIntPoint> Beside = FreeBeside(Captain, TileOf(Next));
			if (!TestTrue(TEXT("Room beside"), Beside.IsSet())) { return; }
			R.F.Combat->MoveUnit(Next, Beside.GetValue(), true);
			const UDwAbilityDefinition* Sword = BasicAttack(Captain);
			const FDwAbilityPreview Swing = Preview(Captain, Sword, Next);
			const float Before = Health(Next);
			TestEqual(TEXT("Second action"), R.Use(Captain, Sword, Next), EDwCommandResult::Accepted);
			TestEqual(TEXT("Hit with the bonus = preview"), Before - Health(Next), static_cast<float>(Swing.Targets[0].Damage));

			// The fight's end carries it into the run and the next fight.
			for (const FDwUnitHandle Enemy : R.Units(EDwSide::Enemy))
			{
				R.F.ApplyEffect(DwGas::GetSharedEffects().Kill, Enemy); // no instigator: no kill bonus
			}
			R.F.EndTurn(EDwSide::Player);
			TestEqual(TEXT("Result carries 1%"), R.F.Combat->GetResult().CaptainKillBonusPct, 0.01f, 1e-5f);
			if (!R.FinishFight()) { return; }
			TestEqual(TEXT("Run counter"), R.Run->GetState().Counters.ConquerorKillBonusPct, 0.01f, 1e-5f);
			if (!R.StartNextFight()) { return; }
			TestEqual(TEXT("Next fight starts with it"), R.F.Attr(R.CaptainUnit(), UDwUnitAttributeSet::GetDamageBonusAttribute()), 0.01f, 1e-5f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
