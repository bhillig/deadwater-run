// Copyright Brandon Hillig 2026

// DW-305: the five shipped enemy crews (Open-Questions N2, N3, C10b; Tools/CreateCombatContent.ps1 ->
// UDwEnemyCrewContentLibrary). Each crew loads and validates, sits in its map tier, and plays its personality through
// the real setup builder, crew AI and tuning rows.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AI/DwAiProfile.h"
#include "Combat/DwEncounterSetupBuilder.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwMapRules.h"
#include "Data/DwUnitDefinition.h"
#include "Misc/DataValidation.h"
#include "Run/DwRunRandom.h"
#include "Run/DwRunSubsystem.h"
#include "Run/Map/DwMapGenerator.h"
#include "Misc/ScopeExit.h"
#include "Tests/AI/DwAiTestBench.h"
#include "Tests/Combat/DwCombatTestFixture.h"

namespace DwEnemyCrewTest
{
	/** N2 rosters (W = Swordsman, N = Sniper) and N3 tiers on the 0-based map layers. */
	struct FExpected
	{
		const TCHAR* Id;
		const TCHAR* Roster;
		int32 MinLayer;
		int32 MaxLayer;
	};
	const FExpected Crews[] = {
		{ TEXT("Deckhands"), TEXT("WWW"), 0, 2 },
		{ TEXT("PowderMonkeys"), TEXT("WWN"), 0, 5 },
		{ TEXT("Sharpshooters"), TEXT("NNN"), 3, 5 },
		{ TEXT("Cutthroats"), TEXT("WWN"), 3, 99 },
		{ TEXT("DreadCrew"), TEXT("WWNN"), 6, 99 },
	};

	const UDwEncounterDefinition* Load(const FString& Id)
	{
		const FString Name = TEXT("DA_Encounter_") + Id;
		return LoadObject<UDwEncounterDefinition>(nullptr, *FString::Printf(TEXT("/Game/DeadwaterRun/Data/Encounters/%s.%s"), *Name, *Name));
	}
}

BEGIN_DEFINE_SPEC(FDwEnemyCrewSpec, "DeadwaterRun.AI.EnemyCrews",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	FDwCombatFixture F; // world only; the fights come from real content
	const UDwGameData* GameData = nullptr;

	/** What a crew's enemies did in a fight the dev crew sat out. */
	struct FBehaviour
	{
		int32 Moves = 0;
		int32 Abilities = 0;
		int32 Boardings = 0;    // moves onto the player's deck
		int32 CannonShots = 0;
		int32 NestClimbs = 0;   // moves onto high ground
		FString Log;
	};

	/** Plays Rounds rounds of Id's fight (seed 7) with the dev crew passing every turn. Can run several per test. */
	FBehaviour PlayPassive(const TCHAR* Id, int32 Rounds)
	{
		FBehaviour B;
		const UDwEncounterDefinition* Encounter = DwEnemyCrewTest::Load(Id);
		if (!TestNotNull(*FString::Printf(TEXT("DA_Encounter_%s"), Id), Encounter) || !TestNotNull(TEXT("DA_GameData"), GameData))
		{
			return B;
		}
		TArray<FDwCombatantSetup> Crew;
		for (const UDwUnitDefinition* Unit : GameData->DevCrew)
		{
			FDwCombatantSetup& Member = Crew.AddDefaulted_GetRef();
			Member.Definition = Unit;
			Member.RosterId = FGuid::NewGuid();
		}
		Crew[0].bIsCaptain = true;
		FDwEncounterSetup Setup;
		TArray<FString> Errors;
		F.Combat->EndEncounter();
		if (!TestTrue(TEXT("Setup built"), FDwEncounterSetupBuilder::BuildShipBattle(*GameData, *Encounter, Crew, 7, Setup, Errors))
			|| !TestTrue(TEXT("Started"), F.Combat->StartEncounter(Setup)))
		{
			return B;
		}
		for (int32 Round = 0; Round < Rounds && F.Combat->GetOutcome() == EDwCombatOutcome::None; ++Round)
		{
			F.EndTurn();
		}
		TArray<FString> Lines;
		for (const FDwCombatCommand& C : F.Combat->GetRecord().Commands)
		{
			const ADwCombatUnit* Unit = C.Type == EDwCommandType::EndTurn ? nullptr : F.Combat->FindUnit(C.Unit);
			if (!Unit || Unit->GetSide() != EDwSide::Enemy || (C.Type != EDwCommandType::Move && C.Type != EDwCommandType::Ability))
			{
				continue;
			}
			Lines.Add(C.ToString());
			if (C.Type == EDwCommandType::Move)
			{
				++B.Moves;
				const FDwTile* Tile = F.Combat->GetGrid().GetTile(C.Target.Tile);
				B.Boardings += Tile && Tile->Owner == EDwSide::Player ? 1 : 0;
				B.NestClimbs += Tile && Tile->Elevation > 0 ? 1 : 0;
			}
			else
			{
				++B.Abilities;
				B.CannonShots += C.AbilityTag == DwTags::Ability_Cannon ? 1 : 0;
			}
		}
		B.Log = FString::Printf(TEXT("%s: %d moves (%d boarding, %d to the nest), %d abilities (%d cannon) | %s"), Id, B.Moves, B.Boardings,
			B.NestClimbs, B.Abilities, B.CannonShots, *FString::Join(Lines, TEXT(" | ")));
		AddInfo(B.Log);
		return B;
	}

END_DEFINE_SPEC(FDwEnemyCrewSpec)

void FDwEnemyCrewSpec::Define()
{
	BeforeEach([this]()
	{
		F.Create();
		GameData = UDwAssetManager::Get().GetGameData();
	});
	AfterEach([this]() { F.Destroy(); });

	Describe("Content", [this]()
	{
		It("ships the five N2 crews: roster, tier, one shared deck, own profile and tuning rows, all valid", [this]()
		{
			const UDwDeckLayout* SharedDeck = nullptr;
			for (const DwEnemyCrewTest::FExpected& Expected : DwEnemyCrewTest::Crews)
			{
				const UDwEncounterDefinition* Encounter = DwEnemyCrewTest::Load(Expected.Id);
				if (!TestNotNull(Expected.Id, Encounter)) { continue; }
				const FString What = Encounter->GetName();
				TestEqual(What + TEXT(" is a ship battle"), Encounter->Kind, EDwEncounterKind::ShipBattle);
				TestTrue(What + TEXT(" is in the fight pool"), Encounter->Weight > 0.f && UDwRunSubsystem::GatherEncounterPool().Contains(Encounter));
				TestEqual(What + TEXT(" MinLayer"), Encounter->MinLayer, Expected.MinLayer);
				TestEqual(What + TEXT(" MaxLayer"), Encounter->MaxLayer, Expected.MaxLayer);

				FString Roster;
				for (const FDwEnemySlot& Slot : Encounter->Units)
				{
					const FString Unit = GetNameSafe(Slot.UnitDefinition);
					Roster += Unit == TEXT("DA_Unit_Sniper") ? TEXT("N") : Unit == TEXT("DA_Unit_Swordsman") ? TEXT("W") : TEXT("?");
					TestFalse(What + TEXT(" slot has a raider look"), Slot.UnitClassOverride.IsNull());
				}
				TestEqual(What + TEXT(" roster"), Roster, FString(Expected.Roster));

				SharedDeck = SharedDeck ? SharedDeck : Encounter->ShipDeck.Get();
				TestTrue(What + TEXT(" sails the shared enemy deck"), Encounter->ShipDeck == SharedDeck);
				TestEqual(What + TEXT(" tuning row"), Encounter->Tuning.RowName, FName(Expected.Id));
				TestTrue(What + TEXT(" pays gold and XP"), Encounter->GetTuningRow<FDwEncounterTuningRow>() != nullptr
					&& Encounter->GetTuningRow<FDwEncounterTuningRow>()->BaseGold > 0 && Encounter->GetTuningRow<FDwEncounterTuningRow>()->ClearXp > 0);

				const UDwAiProfile* Profile = Encounter->AiProfile;
				if (TestNotNull(What + TEXT(" AI profile"), Profile))
				{
					TestEqual(What + TEXT(" profile"), Profile->GetName(), FString(TEXT("DA_AiProfile_")) + Expected.Id);
					TestEqual(What + TEXT(" AI row"), Profile->Tuning.RowName, FName(Expected.Id));
					TestTrue(What + TEXT(" AI row imported"), Profile->GetTuningValues().Weights.Kill > 0.f);
				}
#if WITH_EDITOR
				FDataValidationContext Context;
				TestNotEqual(What + TEXT(" passes data validation"), Encounter->IsDataValid(Context), EDataValidationResult::Invalid);
				FDataValidationContext ProfileContext;
				TestNotEqual(What + TEXT(" profile passes data validation"), Profile ? Profile->IsDataValid(ProfileContext) : EDataValidationResult::Invalid,
					EDataValidationResult::Invalid);
#endif
			}
			const UDwEncounterDefinition* Placeholder = DwEnemyCrewTest::Load(TEXT("Placeholder"));
			if (TestNotNull(TEXT("DA_Encounter_Placeholder"), Placeholder))
			{
				TestEqual(TEXT("The placeholder is never drawn in a run"), Placeholder->Weight, 0.f);
			}
		});

		It("gives each crew its personality switches, and only the Cutthroats hunt the captain (C10b)", [this]()
		{
			auto Profile = [](const TCHAR* Id)
			{
				const UDwEncounterDefinition* Encounter = DwEnemyCrewTest::Load(Id);
				return Encounter ? Encounter->AiProfile.Get() : nullptr;
			};
			for (const DwEnemyCrewTest::FExpected& Expected : DwEnemyCrewTest::Crews)
			{
				const UDwAiProfile* P = Profile(Expected.Id);
				if (!TestNotNull(Expected.Id, P)) { return; }
				const bool bCutthroats = FCString::Strcmp(Expected.Id, TEXT("Cutthroats")) == 0;
				TestEqual(FString(Expected.Id) + TEXT(" CaptainPressure > 0 only for the Cutthroats"), P->GetTuningValues().Weights.CaptainPressure > 0.f, bCutthroats);
			}
			if (Profile(TEXT("Deckhands")) && Profile(TEXT("PowderMonkeys")) && Profile(TEXT("Sharpshooters")))
			{
				TestTrue(TEXT("Deckhands board"), Profile(TEXT("Deckhands"))->bBoarders);
				TestTrue(TEXT("Powder Monkeys work the guns"), Profile(TEXT("PowderMonkeys"))->bPreferCannons);
				TestFalse(TEXT("Powder Monkeys stay on their deck"), Profile(TEXT("PowderMonkeys"))->bBoarders);
				TestFalse(TEXT("Sharpshooters stay on their deck"), Profile(TEXT("Sharpshooters"))->bBoarders);
				TestTrue(TEXT("Sharpshooters value the nest above the default crew"),
					Profile(TEXT("Sharpshooters"))->GetTuningValues().Weights.HighGround > Profile(TEXT("DreadCrew"))->GetTuningValues().Weights.HighGround);
			}
		});
	});

	Describe("Map tiers", [this]()
	{
		It("puts only tier-legal crews on fight nodes over 2,000 seeds, and every crew appears", [this]()
		{
			if (!TestNotNull(TEXT("DA_GameData"), GameData) || !TestNotNull(TEXT("DA_GameData.MapRules"), GameData->MapRules.Get())) { return; }
			const TArray<const UDwEncounterDefinition*> Pool = UDwRunSubsystem::GatherEncounterPool();
			TMap<FString, int32> Seen;
			int32 Bad = 0;
			for (int32 Seed = 0; Seed < 2000; ++Seed)
			{
				const FDwMapGraph Graph = FDwMapGenerator::Build(Seed, *GameData->MapRules, Pool, GameData->KrakenEncounter);
				for (const FDwMapNode& Node : Graph.Nodes)
				{
					if (Node.Type != EDwNodeType::Fight) { continue; }
					const UDwEncounterDefinition* E = Node.Encounter;
					const bool bLegal = E && E->Weight > 0.f && Node.Layer >= E->MinLayer && Node.Layer <= E->MaxLayer;
					if (!bLegal && Bad++ < 3)
					{
						AddError(FString::Printf(TEXT("Seed %d layer %d: %s"), Seed, Node.Layer, *GetNameSafe(E)));
					}
					if (E) { ++Seen.FindOrAdd(E->GetName()); }
				}
			}
			TestEqual(TEXT("Illegal fight nodes"), Bad, 0);
			TArray<FString> Counts;
			for (const TPair<FString, int32>& Pair : Seen) { Counts.Add(FString::Printf(TEXT("%s=%d"), *Pair.Key, Pair.Value)); }
			AddInfo(FString::Join(Counts, TEXT(" ")));
			for (const DwEnemyCrewTest::FExpected& Expected : DwEnemyCrewTest::Crews)
			{
				TestTrue(FString(Expected.Id) + TEXT(" appears on some chart"), Seen.Contains(FString(TEXT("DA_Encounter_")) + Expected.Id));
			}
			TestFalse(TEXT("The placeholder never appears"), Seen.Contains(TEXT("DA_Encounter_Placeholder")));
		});
	});

	// Same seed, same dev crew sitting out three rounds: each crew is told apart from the others by what it does.
	Describe("Personality in a fight", [this]()
	{
		It("Deckhands board; the Powder Monkeys and Sharpshooters never leave their deck", [this]()
		{
			const FBehaviour Deckhands = PlayPassive(TEXT("Deckhands"), 3);
			const FBehaviour Monkeys = PlayPassive(TEXT("PowderMonkeys"), 3);
			const FBehaviour Sharps = PlayPassive(TEXT("Sharpshooters"), 3);
			TestTrue(TEXT("Deckhands board your ship"), Deckhands.Boardings > 0);
			TestEqual(TEXT("Powder Monkeys stay back"), Monkeys.Boardings, 0);
			TestEqual(TEXT("Sharpshooters keep their distance"), Sharps.Boardings, 0);
		});

		It("Powder Monkeys keep working the guns when the Deckhands have left them", [this]()
		{
			const FBehaviour Deckhands = PlayPassive(TEXT("Deckhands"), 3);
			const FBehaviour Monkeys = PlayPassive(TEXT("PowderMonkeys"), 3);
			TestTrue(*FString::Printf(TEXT("More cannon shots (%d) than the Deckhands (%d)"), Monkeys.CannonShots, Deckhands.CannonShots),
				Monkeys.CannonShots > Deckhands.CannonShots);
		});

		It("Sharpshooters take the nest", [this]()
		{
			TestTrue(TEXT("A sniper climbed to high ground"), PlayPassive(TEXT("Sharpshooters"), 3).NestClimbs > 0);
		});

		It("the Dread Crew boards, shoots and works the guns", [this]()
		{
			const FBehaviour Dread = PlayPassive(TEXT("DreadCrew"), 3);
			TestTrue(TEXT("Boards"), Dread.Boardings > 0);
			TestTrue(TEXT("Fires a cannon"), Dread.CannonShots > 0);
			TestTrue(TEXT("Attacks"), Dread.Abilities > Dread.CannonShots);
		});

		// C10b: with a fresh captain and a barely wounded mate both in its sights, a crew normally finishes the wounded;
		// the Cutthroats shoot the captain. Shipped weights (Tuning/AiTuning.json) on the AI test bench.
		It("Cutthroats shoot the captain where every other crew shoots the wounded mate", [this]()
		{
			for (const DwEnemyCrewTest::FExpected& Expected : DwEnemyCrewTest::Crews)
			{
				FDwAiTestBench Bench;
				Bench.Create();
				ON_SCOPE_EXIT { Bench.Destroy(); };
				Bench.Profile->Tuning.RowName = Expected.Id;
				const TArray<FString> NoGuns = { TEXT("S....E"), TEXT("S....E"), TEXT("S....E"), TEXT("S....E"), TEXT("S....E") }; // a target choice, not a cannon one
				Bench.SetBoard(NoGuns, NoGuns);
				FDwUnitSnapshot& Captain = Bench.AddUnit(EDwSide::Player, FIntPoint(3, 1), Bench.Sword);
				Captain.Tags.AddTag(DwTags::Unit_Role_Captain);
				const FDwUnitHandle CaptainHandle = Captain.Handle;
				const FDwUnitHandle Mate = Bench.AddUnit(EDwSide::Player, FIntPoint(3, 3), Bench.Sword, 90.f).Handle;
				const FDwUnitHandle Shooter = Bench.AddUnit(EDwSide::Enemy, FIntPoint(7, 2), Bench.Rifle).Handle;

				FRandomStream Rng(1);
				const TArray<FDwCombatCommand> Turn = Bench.PlayTurn(Shooter, Rng);
				const FDwCombatCommand* Shot = Turn.FindByPredicate([&Bench](const FDwCombatCommand& C) { return C.AbilityTag == Bench.Rifle->AbilityTag; });
				const bool bCutthroats = FCString::Strcmp(Expected.Id, TEXT("Cutthroats")) == 0;
				TestTrue(FString::Printf(TEXT("%s shoots the %s"), Expected.Id, bCutthroats ? TEXT("captain") : TEXT("wounded mate")),
					Shot && Shot->Target.Unit == (bCutthroats ? CaptainHandle : Mate));
			}
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
