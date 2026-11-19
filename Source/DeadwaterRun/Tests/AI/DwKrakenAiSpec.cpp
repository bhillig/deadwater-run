// Copyright Brandon Hillig 2026

// DW-302: the Kraken's brain (Docs/Architecture/08-Kraken-Boss.md "UDwKrakenAiBrain"). Played on the real content
// (Tools/CreateCombatContent.ps1): DA_Encounter_Kraken -> DA_AiProfile_Kraken -> UDwKrakenAiBrain. It telegraphs every
// move (B6) at combat start and after each of its turns, carries out what it showed, aims Slam and Grab at tiles so
// stepping off dodges (Brandon, 2026-11-19), plans two moves after Eyes of the Sea, and re-plans when it enrages (B7).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AI/DwAiProfile.h"
#include "AI/DwAiSideController.h"
#include "AI/DwKrakenAiBrain.h"
#include "Abilities/DwAttributeSets.h"
#include "Abilities/DwGameplayAbility.h"
#include "Combat/DwCombatRules.h"
#include "Combat/DwEncounterSetupBuilder.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwKrakenDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Tests/Combat/DwCombatTestFixture.h"
#include "Tests/DwTestAbilities.h"

BEGIN_DEFINE_SPEC(FDwKrakenAiSpec, "DeadwaterRun.AI.Kraken",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	FDwCombatFixture F; // world only; the fight comes from content
	const UDwGameData* GameData = nullptr;

	/** Handles in spawn order: crew 1-3 (1 = captain), then the body, then the four tentacles. */
	static FDwUnitHandle Captain() { return FDwCombatFixture::CrewHandle(0); }
	static FDwUnitHandle Swordsman() { return FDwCombatFixture::CrewHandle(1); }
	static FDwUnitHandle Sniper() { return FDwCombatFixture::CrewHandle(2); }
	static FDwUnitHandle Body() { return FDwUnitHandle(4); }

	const UDwEncounterDefinition* Kraken() const { return GameData ? GameData->KrakenEncounter.Get() : nullptr; }

	TArray<FDwCombatantSetup> FreshCrew() const
	{
		TArray<FDwCombatantSetup> Crew;
		for (const UDwUnitDefinition* Unit : GameData->DevCrew)
		{
			FDwCombatantSetup& Member = Crew.AddDefaulted_GetRef();
			Member.Definition = Unit;
			Member.RosterId = FGuid::NewGuid();
			Member.bIsCaptain = Crew.Num() == 1;
		}
		return Crew;
	}

	/** The Kraken fight with its real brain. The listener is reset after the start (the opening intents are in storage). */
	bool StartFight(FDwEncounterSetup& Setup, int32 Seed = 7)
	{
		if (!TestNotNull(TEXT("DA_GameData"), GameData) || !TestNotNull(TEXT("KrakenEncounter"), Kraken()))
		{
			return false;
		}
		TArray<FString> Errors;
		const bool bBuilt = FDwEncounterSetupBuilder::Build(*GameData, *Kraken(), FreshCrew(), Seed, Setup, Errors);
		const bool bStarted = TestTrue(FString::Printf(TEXT("Built: %s"), *FString::Join(Errors, TEXT("; "))), bBuilt)
			&& TestTrue(TEXT("Started"), F.Combat->StartEncounter(Setup));
		F.Listener->Reset();
		return bStarted;
	}

	const UDwKrakenAiBrain* FightBrain() const
	{
		const IDwCombatSideController* Controller = F.Combat->GetSideController(EDwSide::Enemy);
		const UDwAiSideController* Ai = Cast<const UDwAiSideController>(Controller);
		return Ai ? Cast<UDwKrakenAiBrain>(Ai->GetBrain(Body())) : nullptr;
	}

	/** A standalone brain configured like the fight's (pure decisions on a snapshot). */
	UDwKrakenAiBrain* MakeBrain(const FDwEncounterSetup& Setup)
	{
		UDwKrakenAiBrain* Brain = F.Root(NewObject<UDwKrakenAiBrain>());
		Brain->Configure(Kraken()->AiProfile, Setup);
		return Brain;
	}

	TArray<FDwAiIntent> Declared() const
	{
		const TArray<FDwAiIntent>* Intents = F.Combat->GetIntents(Body());
		return Intents ? *Intents : TArray<FDwAiIntent>();
	}

	void Declare(const FGameplayTag& Move, const FDwAbilityTarget& Target = FDwAbilityTarget())
	{
		FDwAiIntent Intent;
		Intent.AbilityTag = Move;
		Intent.Target = Target;
		Intent.bTelegraph = true;
		F.Combat->DeclareIntents(Body(), { Intent });
	}

	FIntPoint TileOf(FDwUnitHandle Unit) const { return F.Unit(Unit)->GetTile(); }

	/** An empty standable tile on the player's deck. */
	FIntPoint FreePlayerTile() const
	{
		const FDwGridModel& Grid = F.Combat->GetGrid();
		for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
		{
			const FIntPoint Tile = Grid.ToTile(Index);
			const FDwTile* Data = Grid.GetTile(Tile);
			if (Data && Data->Owner == EDwSide::Player && Data->Elevation == 0 && Grid.IsStandable(Tile) && !Grid.IsOccupied(Tile))
			{
				return Tile;
			}
		}
		return FIntPoint(-1, -1);
	}

	void SetHealth(FDwUnitHandle Unit, float Health) { F.Asc(Unit)->SetNumericAttributeBase(UDwUnitAttributeSet::GetHealthAttribute(), Health); }

	void ToughenCrew()
	{
		for (int32 Index = 0; Index < 3; ++Index)
		{
			F.Asc(FDwCombatFixture::CrewHandle(Index))->SetNumericAttributeBase(UDwUnitAttributeSet::GetMaxHealthAttribute(), 5000.f);
			SetHealth(FDwCombatFixture::CrewHandle(Index), 5000.f);
		}
		F.Combat->GetShip(EDwSide::Player)->GetDwAbilitySystem()->SetNumericAttributeBase(UDwShipAttributeSet::GetMaxHealthAttribute(), 5000.f);
		F.Combat->GetShip(EDwSide::Player)->GetDwAbilitySystem()->SetNumericAttributeBase(UDwShipAttributeSet::GetHealthAttribute(), 5000.f);
	}

	/** The body's moves since the listener was last reset, in order. */
	TArray<FGameplayTag> MovesUsed() const
	{
		TArray<FGameplayTag> Tags;
		for (const FDwCombatEvent& Event : F.Listener->Events)
		{
			if (Event.Type == EDwCombatEventType::AbilityUsed && Event.Source == Body())
			{
				Tags.Add(Event.Tag);
			}
		}
		return Tags;
	}

	FGameplayTagContainer PhaseMoves(int32 Phase) const
	{
		FGameplayTagContainer Tags;
		for (const FDwWeightedAbility& Move : Kraken()->Kraken->Phases[Phase].Moves)
		{
			Tags.AddTag(Move.Ability->AbilityTag);
		}
		return Tags;
	}

	static bool IsTileAim(const FDwAiIntent& Intent) { return !Intent.Target.IsUnit() && !Intent.Target.bShip; }

END_DEFINE_SPEC(FDwKrakenAiSpec)

void FDwKrakenAiSpec::Define()
{
	BeforeEach([this]()
	{
		F.Create();
		GameData = UDwAssetManager::Get().GetGameData();
	});
	AfterEach([this]() { F.Destroy(); });

	Describe("Content", [this]()
	{
		It("plays the boss with DA_AiProfile_Kraken's UDwKrakenAiBrain, and aims Slam and Grab at tiles", [this]()
		{
			if (!TestNotNull(TEXT("KrakenEncounter"), Kraken())) { return; }
			const UDwAiProfile* Profile = Kraken()->AiProfile;
			TestTrue(TEXT("DA_AiProfile_Kraken"), Profile && Profile->GetFName() == TEXT("DA_AiProfile_Kraken"));
			TestTrue(TEXT("Kraken brain"), Profile && Profile->BrainClass == UDwKrakenAiBrain::StaticClass());
			TestTrue(TEXT("Tuning row Kraken"), Profile && Profile->Tuning.RowName == TEXT("Kraken") && Profile->GetTuningValues().Weights.Win > 0.f);

			FDwEncounterSetup Setup;
			if (!StartFight(Setup)) { return; }
			TestNotNull(TEXT("The fight's body is played by the Kraken brain"), FightBrain());
			const FDwCombatSnapshot Snapshot = F.Combat->GetSnapshot();
			const FDwUnitSnapshot* Self = Snapshot.FindUnit(Body());
			for (const FGameplayTag& Tag : TArray<FGameplayTag>{ DwTags::Ability_Kraken_TentacleSlam, DwTags::Ability_Kraken_TentacleGrab })
			{
				const UDwAbilityDefinition* Move = Self ? FDwCombatQueries::FindAbility(*Self, Tag) : nullptr;
				const UDwGameplayAbility* CDO = FDwCombatQueries::GetAbilityCDO(Move);
				TestTrue(FString::Printf(TEXT("%s aims at tiles"), *Tag.ToString()), CDO && CDO->Targeting.bAimAtTile);
			}
		});
	});

	Describe("Intents", [this]()
	{
		It("declares one telegraphed move from the initial phase before the player's first turn", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFight(Setup)) { return; }
			const TArray<FDwAiIntent> Intents = Declared();
			if (!TestEqual(TEXT("One intent"), Intents.Num(), 1)) { return; }
			TestTrue(TEXT("Telegraphed"), Intents[0].bTelegraph);
			TestTrue(TEXT("An initial-phase move"), PhaseMoves(0).HasTagExact(Intents[0].AbilityTag));
			TestEqual(TEXT("Player turn"), F.Combat->GetActiveSide(), EDwSide::Player);
		});

		It("is the same per seed, honours the phase's weights and aims single-target moves at a crew member's tile", [this]()
		{
			TMap<FGameplayTag, int32> Seen;
			for (int32 Seed = 1; Seed <= 30; ++Seed)
			{
				FDwEncounterSetup Setup;
				if (!StartFight(Setup, Seed)) { return; }
				const TArray<FDwAiIntent> First = Declared();
				F.Combat->EndEncounter();
				if (!StartFight(Setup, Seed)) { return; }
				const TArray<FDwAiIntent> Again = Declared();
				if (!TestTrue(FString::Printf(TEXT("Seed %d: one intent each time"), Seed), First.Num() == 1 && Again.Num() == 1)) { return; }
				TestTrue(FString::Printf(TEXT("Seed %d: same move and target"), Seed),
					First[0].AbilityTag == Again[0].AbilityTag && First[0].Target == Again[0].Target);
				++Seen.FindOrAdd(First[0].AbilityTag);

				const FDwAiIntent& Intent = First[0];
				if (Intent.AbilityTag == DwTags::Ability_Kraken_TentacleSlam || Intent.AbilityTag == DwTags::Ability_Kraken_TentacleGrab)
				{
					const FDwOccupant Occupant = F.Combat->GetGrid().GetOccupant(Intent.Target.Tile);
					const ADwCombatUnit* Victim = Occupant.IsSet() ? F.Combat->FindUnit(Occupant.Unit) : nullptr;
					TestTrue(FString::Printf(TEXT("Seed %d: aimed at a crew member's tile (or the ship)"), Seed),
						Intent.Target.bShip || (IsTileAim(Intent) && Victim && Victim->GetSide() == EDwSide::Player));
				}
				F.Combat->EndEncounter();
			}
			for (const FGameplayTag& Tag : PhaseMoves(0))
			{
				TestTrue(FString::Printf(TEXT("%s drawn in 30 seeds"), *Tag.ToString()), Seen.FindRef(Tag) > 0);
			}
			for (const FGameplayTag& Tag : PhaseMoves(1))
			{
				TestEqual(FString::Printf(TEXT("%s never drawn while calm"), *Tag.ToString()), Seen.FindRef(Tag), 0);
			}
		});

		It("carries out what it showed, then shows its next move after its turn", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFight(Setup)) { return; }
			ToughenCrew();
			for (int32 Round = 0; Round < 4; ++Round)
			{
				const TArray<FDwAiIntent> Shown = Declared();
				F.Listener->Reset();
				F.EndTurn();
				const TArray<FGameplayTag> Used = MovesUsed();
				TestTrue(FString::Printf(TEXT("Round %d: used the declared move"), Round),
					Shown.Num() == 1 && Used.Num() == 1 && Used[0] == Shown[0].AbilityTag);
				const int32 UsedAt = F.Listener->Events.IndexOfByPredicate([this](const FDwCombatEvent& E) { return E.Type == EDwCombatEventType::AbilityUsed && E.Source == Body(); });
				const int32 DeclaredAt = F.Listener->Events.FindLastByPredicate([this](const FDwCombatEvent& E) { return E.Type == EDwCombatEventType::IntentDeclared && E.Source == Body(); });
				TestTrue(FString::Printf(TEXT("Round %d: next intent announced after acting"), Round), DeclaredAt > UsedAt && UsedAt != INDEX_NONE);
				TestEqual(TEXT("Back to the player"), F.Combat->GetActiveSide(), EDwSide::Player);
			}
		});
	});

	Describe("Dodging", [this]()
	{
		It("misses a slammed tile the target stepped off, and spends the action", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFight(Setup)) { return; }
			const FIntPoint Aimed = TileOf(Swordsman());
			Declare(DwTags::Ability_Kraken_TentacleSlam, FDwAbilityTarget::MakeTile(Aimed));
			const FIntPoint Free = FreePlayerTile();
			if (!TestTrue(TEXT("A free tile"), Free.X >= 0)) { return; }
			F.Combat->MoveUnit(Swordsman(), Free, true);
			const float Before = F.Health(Swordsman());
			F.Listener->Reset();
			F.EndTurn();
			TestEqual(TEXT("Dodged: no damage"), F.Health(Swordsman()), Before);
			const FDwCombatEvent* Missed = F.Listener->Events.FindByPredicate([](const FDwCombatEvent& E) { return E.Type == EDwCombatEventType::AttackMissed; });
			TestTrue(TEXT("AttackMissed at the aimed tile"), Missed && Missed->Source == Body() && Missed->Tile == Aimed
				&& Missed->Tag == DwTags::Ability_Kraken_TentacleSlam);
			TestEqual(TEXT("The slam was its move"), MovesUsed().Num(), 1);
			TestEqual(TEXT("No Kraken damage"), F.Listener->Count(EDwCombatEventType::UnitDamaged), 0);
		});

		It("hits whoever stands on the slammed tile when it lands", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFight(Setup)) { return; }
			const FIntPoint Aimed = TileOf(Swordsman());
			Declare(DwTags::Ability_Kraken_TentacleSlam, FDwAbilityTarget::MakeTile(Aimed));
			const FIntPoint Free = FreePlayerTile();
			F.Combat->MoveUnit(Swordsman(), Free, true);
			F.Combat->MoveUnit(Sniper(), Aimed, true);
			const float Sniper = F.Health(FDwKrakenAiSpec::Sniper());
			F.EndTurn();
			TestEqual(TEXT("The sniper took the 50"), F.Health(FDwKrakenAiSpec::Sniper()), Sniper - 50.f);
		});

		It("roots whoever is on the grabbed tile, and nobody if it is empty", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFight(Setup)) { return; }
			Declare(DwTags::Ability_Kraken_TentacleGrab, FDwAbilityTarget::MakeTile(TileOf(Captain())));
			F.EndTurn();
			TestTrue(TEXT("Captain rooted"), F.Asc(Captain())->HasMatchingGameplayTag(DwTags::Status_Rooted));

			F.EndTurn(); // the root wears off; the Kraken acts once more
			const FIntPoint Aimed = TileOf(Swordsman());
			Declare(DwTags::Ability_Kraken_TentacleGrab, FDwAbilityTarget::MakeTile(Aimed));
			F.Combat->MoveUnit(Swordsman(), FreePlayerTile(), true);
			F.Listener->Reset();
			F.EndTurn();
			TestFalse(TEXT("Swordsman not rooted"), F.Asc(Swordsman())->HasMatchingGameplayTag(DwTags::Status_Rooted));
			TestEqual(TEXT("A miss"), F.Listener->Count(EDwCombatEventType::AttackMissed), 1);
		});
	});

	Describe("Turns", [this]()
	{
		It("plans two moves after Eyes of the Sea, never Eyes again while the extra action is pending, then one", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFight(Setup)) { return; }
			ToughenCrew();
			SetHealth(Body(), 140.f); // Eyes of the Sea is an enraged move: enraged from its first turn
			Declare(DwTags::Ability_Kraken_EyesOfTheSea, FDwAbilityTarget::MakeUnit(Body(), TileOf(Body())));
			F.EndTurn();
			const TArray<FDwAiIntent> Double = Declared();
			TestEqual(TEXT("Two intents for the double turn"), Double.Num(), 2);
			TestFalse(TEXT("No Eyes in it"), Double.ContainsByPredicate([](const FDwAiIntent& I) { return I.AbilityTag == DwTags::Ability_Kraken_EyesOfTheSea; }));
			F.Listener->Reset();
			F.EndTurn();
			const TArray<FGameplayTag> Used = MovesUsed();
			TestTrue(TEXT("Both carried out, in order"), Used.Num() == 2 && Double.Num() == 2
				&& Used[0] == Double[0].AbilityTag && Used[1] == Double[1].AbilityTag);
			TestEqual(TEXT("Back to one"), Declared().Num(), 1);
		});

		It("re-plans with enraged moves when it enrages at the start of its turn (B7)", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFight(Setup)) { return; }
			ToughenCrew();
			Declare(DwTags::Ability_Kraken_TentacleSlam, FDwAbilityTarget::MakeShip(EDwSide::Player));
			SetHealth(Body(), 100.f); // 20%: enraged from its next turn
			F.Listener->Reset();
			F.EndTurn();
			TestEqual(TEXT("Enraged"), F.Listener->Count(EDwCombatEventType::PhaseChanged), 1);
			const TArray<FGameplayTag> Used = MovesUsed();
			TestTrue(TEXT("Used an enraged move instead of the stale slam"), Used.Num() >= 1 && PhaseMoves(1).HasTagExact(Used[0]));
			for (const FDwAiIntent& Intent : Declared())
			{
				TestTrue(TEXT("Next intents are enraged moves"), PhaseMoves(1).HasTagExact(Intent.AbilityTag));
			}
		});

		It("plays ten rounds against a passive crew without a refused command", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFight(Setup, 21)) { return; }
			ToughenCrew();
			int32 Actions = 0;
			for (int32 Round = 0; Round < 10 && F.Combat->GetOutcome() == EDwCombatOutcome::None; ++Round)
			{
				const int32 Expected = Declared().Num();
				F.Listener->Reset();
				F.EndTurn();
				TestEqual(FString::Printf(TEXT("Round %d: one move per declared intent"), Round), MovesUsed().Num(), Expected);
				Actions += MovesUsed().Num();
				if (Round == 5) { SetHealth(Body(), 120.f); } // the second half is enraged
			}
			TestTrue(TEXT("It acted every round"), Actions >= 10);
		});
	});

	Describe("Targets", [this]()
	{
		It("slams the pirate it can finish", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFight(Setup)) { return; }
			SetHealth(Sniper(), 30.f);
			const FDwCombatSnapshot Snapshot = F.Combat->GetSnapshot();
			const UDwKrakenAiBrain* Brain = MakeBrain(Setup);
			const FDwUnitSnapshot* Self = Snapshot.FindUnit(Body());
			FRandomStream Rng(3);
			FDwAbilityTarget Target;
			TestTrue(TEXT("Chose"), Brain->ChooseTarget(Snapshot, *Self, *FDwCombatQueries::FindAbility(*Self, DwTags::Ability_Kraken_TentacleSlam), Rng, Target));
			TestTrue(TEXT("The sniper's tile"), !Target.IsUnit() && !Target.bShip && Target.Tile == TileOf(Sniper()));
		});

		It("slams the captain when the hit would end the run, over a cheaper kill", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFight(Setup)) { return; }
			SetHealth(Sniper(), 10.f);
			SetHealth(Captain(), 40.f);
			const FDwCombatSnapshot Snapshot = F.Combat->GetSnapshot();
			const FDwUnitSnapshot* Self = Snapshot.FindUnit(Body());
			FRandomStream Rng(3);
			FDwAbilityTarget Target;
			MakeBrain(Setup)->ChooseTarget(Snapshot, *Self, *FDwCombatQueries::FindAbility(*Self, DwTags::Ability_Kraken_TentacleSlam), Rng, Target);
			TestEqual(TEXT("The captain's tile"), Target.Tile, TileOf(Captain()));
		});

		It("slams the ship when the hit would sink it", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFight(Setup)) { return; }
			F.Combat->GetShip(EDwSide::Player)->GetDwAbilitySystem()->SetNumericAttributeBase(UDwShipAttributeSet::GetHealthAttribute(), 40.f);
			const FDwCombatSnapshot Snapshot = F.Combat->GetSnapshot();
			const FDwUnitSnapshot* Self = Snapshot.FindUnit(Body());
			FRandomStream Rng(3);
			FDwAbilityTarget Target;
			MakeBrain(Setup)->ChooseTarget(Snapshot, *Self, *FDwCombatQueries::FindAbility(*Self, DwTags::Ability_Kraken_TentacleSlam), Rng, Target);
			TestTrue(TEXT("The ship"), Target.bShip && Target.ShipSide == EDwSide::Player);
		});

		It("grabs the hardest hitter who isn't already rooted", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFight(Setup)) { return; }
			F.Asc(Swordsman())->SetNumericAttributeBase(UDwUnitAttributeSet::GetAttackDamageAttribute(), 90.f);
			F.Asc(Sniper())->SetNumericAttributeBase(UDwUnitAttributeSet::GetAttackDamageAttribute(), 60.f);
			const UDwKrakenAiBrain* Brain = MakeBrain(Setup);
			FRandomStream Rng(3);
			FDwAbilityTarget Target;
			{
				const FDwCombatSnapshot Snapshot = F.Combat->GetSnapshot();
				const FDwUnitSnapshot* Self = Snapshot.FindUnit(Body());
				Brain->ChooseTarget(Snapshot, *Self, *FDwCombatQueries::FindAbility(*Self, DwTags::Ability_Kraken_TentacleGrab), Rng, Target);
				TestEqual(TEXT("The swordsman"), Target.Tile, TileOf(Swordsman()));
			}
			F.ApplyEffect(DwGas::GetSharedEffects().Rooted, Swordsman(), Body());
			const FDwCombatSnapshot Snapshot = F.Combat->GetSnapshot();
			const FDwUnitSnapshot* Self = Snapshot.FindUnit(Body());
			Brain->ChooseTarget(Snapshot, *Self, *FDwCombatQueries::FindAbility(*Self, DwTags::Ability_Kraken_TentacleGrab), Rng, Target);
			TestEqual(TEXT("Then the sniper"), Target.Tile, TileOf(Sniper()));
		});

		It("doesn't spend a second hit of the same plan on a pirate the first one kills", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFight(Setup)) { return; }
			SetHealth(Sniper(), 30.f);
			SetHealth(Swordsman(), 45.f);
			F.ApplyEffect(DwGas::GetSharedEffects().Rooted, Captain(), Body()); // Grab has nobody new worth pinning first
			const FDwCombatSnapshot Snapshot = F.Combat->GetSnapshot();
			const UDwKrakenAiBrain* Brain = MakeBrain(Setup);
			for (int32 Seed = 0; Seed < 20; ++Seed)
			{
				FRandomStream Rng(Seed);
				const TArray<FDwAiIntent> Plan = Brain->PlanMoves(Snapshot, Body(), 2, Rng);
				TArray<FIntPoint> Slammed;
				for (const FDwAiIntent& Intent : Plan)
				{
					if (Intent.AbilityTag == DwTags::Ability_Kraken_TentacleSlam && IsTileAim(Intent))
					{
						Slammed.Add(Intent.Target.Tile);
					}
				}
				TestFalse(FString::Printf(TEXT("Seed %d: no double slam on one tile"), Seed), Slammed.Num() == 2 && Slammed[0] == Slammed[1]);
			}
		});
	});
}

#endif
