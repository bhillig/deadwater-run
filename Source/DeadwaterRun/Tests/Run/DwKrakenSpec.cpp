// Copyright Brandon Hillig 2026

// DW-209: the placeholder Kraken boss (Docs/Architecture/08 "Placeholder boss"). The arena builder and layout rules are
// pure; the encounter is the real content (Tools/CreateCombatContent.ps1) played through the real run: the boss node at
// the end of a generated chart launches it, killing the body is VictoryBoss and ends the run in Victory, and losing to
// it is a defeat like any fight.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/DwGameplayEffects.h"
#include "Board/DwBoardBuilder.h"
#include "Board/DwDeckLayout.h"
#include "Combat/DwCombatRules.h"
#include "Combat/DwEncounterSetupBuilder.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwKrakenDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Misc/DataValidation.h"
#include "Run/DwRun.h"
#include "Run/DwRunSubsystem.h"
#include "Tests/Combat/DwCombatTestFixture.h"

BEGIN_DEFINE_SPEC(FDwKrakenSpec, "DeadwaterRun.Run.Kraken",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	FDwCombatFixture F; // world only; the fight comes from content
	const UDwGameData* GameData = nullptr;
	UDwRun* Run = nullptr;

	/** The player deck of DA_Deck_PlayerShip (6 x 4, boarding edges in the rightmost column). */
	static TArray<FString> PlayerDeck() { return { TEXT("S...NE"), TEXT("SC.MLE"), TEXT("S.c..E"), TEXT("SC...E") }; }
	static TArray<FString> Arena() { return { TEXT("~~~~"), TEXT("~~~~"), TEXT("~~~B"), TEXT("~~~~") }; }

	const UDwEncounterDefinition* Kraken() const { return GameData ? GameData->KrakenEncounter.Get() : nullptr; }

	UDwDeckLayout* MakeLayout(const TArray<FString>& Rows)
	{
		UDwDeckLayout* Layout = F.Root(NewObject<UDwDeckLayout>(GetTransientPackage()));
		const FDwDeckSource Source = FDwDeckAscii::Parse(Rows);
		Layout->Size = Source.Size;
		Layout->Tiles = Source.Tiles;
		return Layout;
	}

	/** The dev crew at level 1 and full health: the captain first. */
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

	/** The Kraken fight outside a run, with FreshCrew(). Body = the first enemy handle. */
	bool StartFreshKrakenFight(FDwEncounterSetup& Setup)
	{
		if (!TestNotNull(TEXT("DA_GameData"), GameData) || !TestNotNull(TEXT("KrakenEncounter"), Kraken()))
		{
			return false;
		}
		TArray<FString> Errors;
		const bool bBuilt = FDwEncounterSetupBuilder::Build(*GameData, *Kraken(), FreshCrew(), 7, Setup, Errors);
		return TestTrue(FString::Printf(TEXT("Built: %s"), *FString::Join(Errors, TEXT("; "))), bBuilt)
			&& TestTrue(TEXT("Started"), F.Combat->StartEncounter(Setup));
	}

	FDwUnitHandle Body() const
	{
		for (const ADwCombatUnit* Unit : F.Combat->GetUnits())
		{
			if (Unit->GetSide() == EDwSide::Enemy)
			{
				return Unit->GetHandle();
			}
		}
		return FDwUnitHandle();
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

	float PlayerSideHealth() const
	{
		float Total = F.ShipHealth(EDwSide::Player);
		for (const ADwCombatUnit* Unit : F.Combat->GetUnits())
		{
			Total += Unit->GetSide() == EDwSide::Player ? F.Health(Unit->GetHandle()) : 0.f;
		}
		return Total;
	}

	bool StartRun(const TCHAR* CaptainName, int32 Seed)
	{
		const UDwCaptainDefinition* Captain = UDwRunSubsystem::FindCaptain(CaptainName);
		if (!TestNotNull(TEXT("DA_GameData"), GameData) || !TestNotNull(TEXT("Captain"), Captain))
		{
			return false;
		}
		Run = F.Root(NewObject<UDwRun>());
		TArray<FString> Errors;
		const bool bOk = Run->Initialize(*GameData, *Captain, Seed, UDwRunSubsystem::GatherEncounterPool(), &Errors);
		return TestTrue(FString::Printf(TEXT("Run initialized: %s"), *FString::Join(Errors, TEXT("; "))), bOk);
	}

	/** Sails to the next fight (shops and events on the way are left at once) and starts it in the fixture world. */
	bool StartNextFight(FDwEncounterSetup& OutSetup)
	{
		for (int32 Step = 0; Step < 20 && Run->GetPhase() != EDwRunPhase::Combat; ++Step)
		{
			const TArray<int32> Choices = Run->GetChoices();
			const int32* Fight = Choices.FindByPredicate([this](int32 Index) { return Run->GetState().Map.Nodes[Index].Type != EDwNodeType::Shop
				&& Run->GetState().Map.Nodes[Index].Type != EDwNodeType::Event; });
			if (!TestTrue(TEXT("A route"), !Choices.IsEmpty()) || !TestTrue(TEXT("Sailed"), Run->ChooseNode(Fight ? *Fight : Choices[0])))
			{
				return false;
			}
			if (Run->GetPhase() == EDwRunPhase::Shop) { Run->LeaveShop(); }
			if (Run->GetPhase() == EDwRunPhase::Event) { Run->LeaveEvent(); }
		}
		TArray<FString> Errors;
		const bool bBuilt = Run->BuildEncounterSetup(OutSetup, Errors);
		return TestTrue(FString::Printf(TEXT("Setup at node %d: %s"), Run->GetState().CurrentNode, *FString::Join(Errors, TEXT("; "))), bBuilt)
			&& TestTrue(TEXT("Fight started"), F.Combat->StartEncounter(OutSetup));
	}

	/** Every living enemy dies by the captain's hand; deaths outside a command count at the turn boundary. */
	void CaptainKillsEveryEnemy()
	{
		const FDwUnitHandle Captain = FindCrewUnit(Run->GetState().Crew[0].Id);
		for (const ADwCombatUnit* Unit : F.Combat->GetUnits())
		{
			if (Unit->GetSide() == EDwSide::Enemy && Unit->IsAlive())
			{
				F.ApplyEffect(DwGas::GetSharedEffects().Kill, Unit->GetHandle(), Captain);
			}
		}
		F.EndTurn(EDwSide::Player);
	}

	/** Plays the generated chart through the run (every fight won by the captain) until the boss fight has started. */
	bool SailToTheKraken(FDwEncounterSetup& OutSetup)
	{
		for (int32 Stop = 0; Stop < 20; ++Stop)
		{
			if (!StartNextFight(OutSetup))
			{
				return false;
			}
			if (Run->GetState().CurrentNode == Run->GetState().Map.BossNode)
			{
				return true;
			}
			CaptainKillsEveryEnemy();
			const FDwCombatResult Result = F.Combat->GetResult();
			F.Combat->EndEncounter();
			if (!TestTrue(TEXT("Fight won"), DwCombat::IsVictory(Result.Outcome)) || !TestTrue(TEXT("Applied"), Run->ApplyCombatResult(Result))
				|| !TestTrue(TEXT("Claimed"), Run->ClaimRewards(INDEX_NONE, FGuid())))
			{
				return false;
			}
			while (const FDwPendingLevelUp* LevelUp = Run->GetPhase() == EDwRunPhase::LevelUp ? Run->GetCurrentLevelUp() : nullptr)
			{
				Run->ChoosePerk(LevelUp->PerkChoices.IsEmpty() ? INDEX_NONE : 0);
			}
			if (!TestEqual(TEXT("Back on the map"), Run->GetPhase(), EDwRunPhase::Map))
			{
				return false;
			}
		}
		AddError(TEXT("Never reached the boss node."));
		return false;
	}

END_DEFINE_SPEC(FDwKrakenSpec)

void FDwKrakenSpec::Define()
{
	BeforeEach([this]()
	{
		F.Create();
		GameData = UDwAssetManager::Get().GetGameData();
	});
	AfterEach([this]() { F.Destroy(); Run = nullptr; });

	Describe("Arena", [this]()
	{
		It("puts the player deck beside a neutral arena whose body anchor is the only enemy spawn, with nothing to board", [this]()
		{
			const FDwGridModel Grid = FDwBoardBuilder::BuildKrakenArena(FDwDeckAscii::Parse(PlayerDeck()), FDwDeckAscii::Parse(Arena()));
			TestEqual(TEXT("10 x 4"), Grid.Size, FIntPoint(10, 4));
			const FIntPoint Anchor(6, 2); // the arena is mirrored: its rightmost column lies next to the rail
			const FDwTile* Tile = Grid.GetTile(Anchor);
			if (!TestNotNull(TEXT("Anchor"), Tile)) { return; }
			TestTrue(TEXT("Body anchor"), Tile->Tags.HasTagExact(DwTags::Tile_Kraken_Body));
			TestTrue(TEXT("Enemy spawn"), Tile->Tags.HasTagExact(DwTags::Tile_Spawn_Enemy));
			TestTrue(TEXT("The body can stand there"), Grid.IsStandable(Anchor));

			int32 EnemySpawns = 0;
			for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
			{
				const FIntPoint P = Grid.ToTile(Index);
				EnemySpawns += Grid.Tiles[Index].Tags.HasTagExact(DwTags::Tile_Spawn_Enemy) ? 1 : 0;
				if (P.X >= 6)
				{
					TestEqual(TEXT("Arena tiles are nobody's deck"), Grid.Tiles[Index].Owner, EDwSide::Neutral);
					TestEqual(TEXT("Open water elsewhere"), Grid.IsStandable(P), P == Anchor);
				}
			}
			TestEqual(TEXT("One enemy spawn"), EnemySpawns, 1);

			TArray<FDwGridStep> Steps;
			Grid.GetNeighbors(FIntPoint(5, 2), FDwGridRules(), Steps);
			TestFalse(TEXT("No step from the rail onto the Kraken's tile"), Steps.ContainsByPredicate([&Anchor](const FDwGridStep& S) { return S.Tile == Anchor; }));
			Grid.GetNeighbors(Anchor, FDwGridRules(), Steps);
			TestTrue(TEXT("No step off it either"), Steps.IsEmpty());
			TestEqual(TEXT("ASCII round trip"), FString::Join(FDwDeckAscii::Export(FDwDeckAscii::Parse(Arena())), TEXT("|")), FString::Join(Arena(), TEXT("|")));
		});

		It("validates arenas: exactly one body anchor and no boarding edge; ship decks still need one", [this]()
		{
			TestTrue(TEXT("The placeholder arena is valid"), MakeLayout(Arena())->Validate().IsEmpty());
			TestTrue(TEXT("Two anchors"), MakeLayout({ TEXT("~~~B"), TEXT("~~~B") })->Validate().ContainsByPredicate([](const FString& E) { return E.Contains(TEXT("exactly one body anchor")); }));
			TestTrue(TEXT("A boarding edge"), MakeLayout({ TEXT("~~SE"), TEXT("~~~B") })->Validate().ContainsByPredicate([](const FString& E) { return E.Contains(TEXT("nothing to board")); }));
			TestTrue(TEXT("Ship deck without an edge"), MakeLayout({ TEXT("S..."), TEXT("S...") })->Validate().ContainsByPredicate([](const FString& E) { return E.Contains(TEXT("boarding edge")); }));
		});
	});

	Describe("Encounter", [this]()
	{
#if WITH_EDITOR
		It("is DA_GameData.KrakenEncounter: valid, kind Kraken, a 500 HP body with Tentacle Slam only and no tentacles", [this]()
		{
			const UDwEncounterDefinition* Encounter = Kraken();
			if (!TestNotNull(TEXT("KrakenEncounter"), Encounter) || !TestNotNull(TEXT("Kraken definition"), Encounter->Kraken.Get())) { return; }
			const UDwKrakenDefinition* Def = Encounter->Kraken;
			TestEqual(TEXT("Kind"), Encounter->Kind, EDwEncounterKind::Kraken);
			for (const UObject* Asset : { static_cast<const UObject*>(Encounter), static_cast<const UObject*>(Def), static_cast<const UObject*>(Def->Body.Get()),
				static_cast<const UObject*>(Def->Arena.Get()) })
			{
				FDataValidationContext Context;
				TestEqual(*FString::Printf(TEXT("%s is valid"), *GetNameSafe(Asset)), Asset ? Asset->IsDataValid(Context) : EDataValidationResult::Invalid,
					EDataValidationResult::Valid);
			}
			const FDwUnitTuningRow* BodyRow = Def->Body ? Def->Body->GetTuningRow<FDwUnitTuningRow>() : nullptr;
			if (!TestNotNull(TEXT("KrakenBody row"), BodyRow)) { return; }
			TestEqual(TEXT("500 HP"), static_cast<float>(BodyRow->GetMaxHealthAtLevel(1)), 500.f);
			TestEqual(TEXT("One action a turn"), BodyRow->MaxActionPoints, 1);
			TestTrue(TEXT("Unit.Kraken.Body"), Def->Body->UnitTags.HasTagExact(DwTags::Unit_Kraken_Body));
			TestTrue(TEXT("Slams"), Def->Body->BasicAttack && Def->Body->BasicAttack->AbilityTag == DwTags::Ability_Kraken_TentacleSlam);
			TestTrue(TEXT("One phase, Tentacle Slam only"), Def->Phases.Num() == 1 && Def->Phases[0].Moves.Num() == 1
				&& Def->Phases[0].Moves[0].Ability == Def->Body->BasicAttack);
			const FDwKrakenTuningRow* KrakenRow = Def->GetTuningRow<FDwKrakenTuningRow>();
			TestTrue(TEXT("No tentacles"), KrakenRow && KrakenRow->TentacleCount == 0);
			TestTrue(TEXT("The body is driven by an AI profile until DW-302"), Encounter->AiProfile != nullptr);
			TestFalse(TEXT("Never in the fight pool"), UDwRunSubsystem::GatherEncounterPool().Contains(Encounter));
		});
#endif

		It("builds a boss setup: the arena, no enemy ship, the body as the only enemy, and the boss outcome", [this]()
		{
			if (!TestNotNull(TEXT("DA_GameData"), GameData) || !TestNotNull(TEXT("KrakenEncounter"), Kraken())) { return; }
			FDwEncounterSetup Setup;
			TArray<FString> Errors;
			if (!TestTrue(FString::Printf(TEXT("Built: %s"), *FString::Join(Errors, TEXT("; "))),
				FDwEncounterSetupBuilder::Build(*GameData, *Kraken(), FreshCrew(), 3, Setup, Errors))) { return; }
			TestFalse(TEXT("No enemy ship"), Setup.bHasEnemyShip);
			TestTrue(TEXT("Boss fight"), Setup.OutcomeConfig.bIsBossFight);
			TestTrue(TEXT("The body alone"), Setup.Enemies.Num() == 1 && Setup.Enemies[0].Definition == Kraken()->Kraken->Body);
			TestTrue(TEXT("Driven by the encounter's AI profile"), Setup.Enemies[0].AiProfile == Kraken()->AiProfile);
			TestEqual(TEXT("Player deck + arena"), Setup.Grid.Size.X, GameData->DefaultPlayerDeck->Size.X + Kraken()->Kraken->Arena->Size.X);
			TestEqual(TEXT("Player ship from DT_ShipTuning"), Setup.PlayerShip.MaxHealth, 250.f);

			TArray<FString> Refused;
			TestFalse(TEXT("Not a ship battle"), FDwEncounterSetupBuilder::BuildShipBattle(*GameData, *Kraken(), FreshCrew(), 3, Setup, Refused));
			TestFalse(TEXT("A ship battle is not the Kraken"), GameData->DevEncounter && FDwEncounterSetupBuilder::BuildKraken(*GameData, *GameData->DevEncounter, FreshCrew(), 3, Setup, Refused));
			TestEqual(TEXT("Two refusals"), Refused.Num(), 2);
		});

		It("refuses tentacles until DW-301 builds them", [this]()
		{
			if (!TestNotNull(TEXT("KrakenEncounter"), Kraken())) { return; }
			UDataTable* Table = F.Root(NewObject<UDataTable>(GetTransientPackage()));
			Table->RowStruct = FDwKrakenTuningRow::StaticStruct();
			FDwKrakenTuningRow Row;
			Row.TentacleCount = 4;
			Table->AddRow(TEXT("WithTentacles"), Row);
			UDwKrakenDefinition* Def = F.Root(DuplicateObject<UDwKrakenDefinition>(Kraken()->Kraken.Get(), GetTransientPackage()));
			Def->Tuning.DataTable = Table;
			Def->Tuning.RowName = TEXT("WithTentacles");
			UDwEncounterDefinition* Encounter = F.Root(DuplicateObject<UDwEncounterDefinition>(Kraken(), GetTransientPackage()));
			Encounter->Kraken = Def;
			FDwEncounterSetup Setup;
			TArray<FString> Errors;
			TestFalse(TEXT("Refused"), FDwEncounterSetupBuilder::Build(*GameData, *Encounter, FreshCrew(), 3, Setup, Errors));
			TestTrue(TEXT("Says why"), Errors.Num() == 1 && Errors[0].Contains(TEXT("DW-301")));
#if WITH_EDITOR
			FDataValidationContext Context;
			TestEqual(TEXT("Tentacles need a Tentacle unit"), Def->IsDataValid(Context), EDataValidationResult::Invalid);
#endif
		});
	});

	Describe("Fight", [this]()
	{
		It("spawns the 500 HP body on its anchor, beside the player ship, with no enemy ship", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFreshKrakenFight(Setup)) { return; }
			const FDwUnitHandle KrakenUnit = Body();
			if (!TestTrue(TEXT("Body"), KrakenUnit.IsValid())) { return; }
			TestEqual(TEXT("Four units: three crew and the Kraken"), F.Combat->GetUnits().Num(), 4);
			TestEqual(TEXT("500 HP"), F.Health(KrakenUnit), 500.f);
			const FDwTile* Tile = F.Combat->GetSnapshot().Grid.GetTile(F.Unit(KrakenUnit)->GetTile());
			TestTrue(TEXT("On the body anchor"), Tile && Tile->Tags.HasTagExact(DwTags::Tile_Kraken_Body));
			TestNotNull(TEXT("Player ship"), F.Combat->GetShip(EDwSide::Player));
			TestNull(TEXT("No enemy ship"), F.Combat->GetShip(EDwSide::Enemy));
			TestEqual(TEXT("Player first"), F.Combat->GetActiveSide(), EDwSide::Player);
		});

		It("lets the crew hit the body with the normal range rules, and the Kraken slams back once for 50", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFreshKrakenFight(Setup)) { return; }
			const FDwUnitHandle KrakenUnit = Body();
			const FDwUnitHandle Swordsman = FDwCombatFixture::CrewHandle(1);
			const FDwUnitHandle Sniper = FDwCombatFixture::CrewHandle(2);
			const UDwAbilityDefinition* Cutlass = GameData->DevCrew[1]->BasicAttack;
			const UDwAbilityDefinition* Musket = GameData->DevCrew[2]->BasicAttack;
			auto Attack = [this, KrakenUnit](FDwUnitHandle Unit, const UDwAbilityDefinition* Ability)
			{
				return F.Combat->SubmitCommand(FDwCombatCommand::MakeAbility(Unit, Ability->AbilityTag, FDwAbilityTarget::MakeUnit(KrakenUnit, F.Unit(KrakenUnit)->GetTile())));
			};

			TestEqual(TEXT("Out of cutlass reach from the spawn"), Attack(Swordsman, Cutlass), EDwCommandResult::InvalidTarget);
			F.Combat->MoveUnit(Swordsman, FIntPoint(5, 2), /*bSnapActor*/ true); // on the rail, beside the anchor
			F.Combat->MoveUnit(Sniper, FIntPoint(5, 3), true);
			TestEqual(TEXT("Cutlass from the rail"), Attack(Swordsman, Cutlass), EDwCommandResult::Accepted);
			TestEqual(TEXT("Musket"), Attack(Sniper, Musket), EDwCommandResult::Accepted);
			TestEqual(TEXT("500 - 25 - 20"), F.Health(KrakenUnit), 455.f);

			const float Before = PlayerSideHealth();
			TestEqual(TEXT("End turn"), F.EndTurn(), EDwCommandResult::Accepted);
			const TArray<FDwCombatCommand> Slams = F.Combat->GetRecord().Commands.FilterByPredicate([KrakenUnit](const FDwCombatCommand& C)
			{
				return C.Unit == KrakenUnit && C.Type == EDwCommandType::Ability && C.AbilityTag == DwTags::Ability_Kraken_TentacleSlam;
			});
			TestEqual(TEXT("One slam (one action a turn)"), Slams.Num(), 1);
			TestEqual(TEXT("50 damage to a crew member or the ship"), Before - PlayerSideHealth(), 50.f);
			TestEqual(TEXT("Back to the player"), F.Combat->GetActiveSide(), EDwSide::Player);
			TestEqual(TEXT("Nobody won yet"), F.Combat->GetOutcome(), EDwCombatOutcome::None);
		});

		It("is VictoryBoss when the body dies, and plain defeats still apply", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFreshKrakenFight(Setup)) { return; }
			F.ApplyEffect(DwGas::GetSharedEffects().Kill, Body(), FDwCombatFixture::CrewHandle(0));
			F.EndTurn();
			TestEqual(TEXT("VictoryBoss"), F.Combat->GetOutcome(), EDwCombatOutcome::VictoryBoss);

			if (!StartFreshKrakenFight(Setup)) { return; }
			F.ApplyEffect(DwGas::GetSharedEffects().Kill, FDwCombatFixture::CrewHandle(0));
			F.EndTurn();
			TestEqual(TEXT("Captain dead (C10)"), F.Combat->GetOutcome(), EDwCombatOutcome::DefeatCaptainDead);
		});
	});

	Describe("Run", [this]()
	{
		It("puts a buildable Kraken fight on the boss node of every chart (seed sweep, all captains)", [this]()
		{
			if (!TestNotNull(TEXT("DA_GameData"), GameData) || !TestNotNull(TEXT("KrakenEncounter"), Kraken())) { return; }
			int32 Bad = 0;
			for (const UDwCaptainDefinition* Captain : GameData->Captains)
			{
				for (int32 Seed = 1; Seed <= 100; ++Seed)
				{
					UDwRun* Sweep = NewObject<UDwRun>();
					TArray<FString> Errors;
					FDwEncounterSetup Setup;
					const bool bOk = Sweep->Initialize(*GameData, *Captain, Seed, UDwRunSubsystem::GatherEncounterPool(), &Errors)
						&& Sweep->GetState().Map.Nodes.IsValidIndex(Sweep->GetState().Map.BossNode)
						&& Sweep->GetState().Map.Nodes[Sweep->GetState().Map.BossNode].Encounter == Kraken()
						&& FDwEncounterSetupBuilder::Build(*GameData, *Kraken(), FreshCrew(), Seed, Setup, Errors) && Setup.OutcomeConfig.bIsBossFight;
					if (!bOk && ++Bad <= 5)
					{
						AddError(FString::Printf(TEXT("%s seed %d: %s"), *Captain->GetName(), Seed, *FString::Join(Errors, TEXT("; "))));
					}
				}
			}
			TestEqual(TEXT("Charts without a playable boss"), Bad, 0);
		});

		It("launches the Kraken from the boss node of a generated chart, and beating it wins the run", [this]()
		{
			if (!StartRun(TEXT("TeamLeader"), 7)) { return; }
			FDwEncounterSetup Setup;
			if (!SailToTheKraken(Setup)) { return; }
			const FDwRunState& S = Run->GetState();
			TestEqual(TEXT("Boss node"), Run->GetCurrentNode()->Type, EDwNodeType::Boss);
			TestTrue(TEXT("Its encounter is DA_GameData.KrakenEncounter"), Setup.Encounter == Kraken());
			TestTrue(TEXT("Boss fight"), Setup.OutcomeConfig.bIsBossFight);
			TestFalse(TEXT("No enemy ship"), Setup.bHasEnemyShip);
			TestEqual(TEXT("Every survivor aboard"), Setup.Crew.Num(), S.CountLivingCrew());
			TestEqual(TEXT("The Kraken at 500"), F.Health(Body()), 500.f);
			const int32 Cleared = S.Stats.NodesCleared;

			CaptainKillsEveryEnemy();
			const FDwCombatResult Result = F.Combat->GetResult();
			F.Combat->EndEncounter();
			TestEqual(TEXT("VictoryBoss"), Result.Outcome, EDwCombatOutcome::VictoryBoss);
			TestTrue(TEXT("Applied"), Run->ApplyCombatResult(Result));
			TestEqual(TEXT("Victory"), S.Phase, EDwRunPhase::Victory);
			TestEqual(TEXT("End reason"), S.Stats.EndReason, EDwRunEndReason::Victory);
			TestEqual(TEXT("The boss counts as a cleared node"), S.Stats.NodesCleared, Cleared + 1);
			TestTrue(TEXT("Over"), Run->IsOver());
			TestEqual(TEXT("No rewards after the boss"), S.PendingRewards.Gold, 0);
		});

		It("ends the run in defeat when the crew falls to the Kraken", [this]()
		{
			if (!StartRun(TEXT("Fearmongerer"), 11)) { return; }
			FDwEncounterSetup Setup;
			if (!SailToTheKraken(Setup)) { return; }
			for (const ADwCombatUnit* Unit : F.Combat->GetUnits())
			{
				if (Unit->GetSide() == EDwSide::Player)
				{
					F.ApplyEffect(DwGas::GetSharedEffects().Kill, Unit->GetHandle(), Body());
				}
			}
			F.EndTurn();
			const FDwCombatResult Result = F.Combat->GetResult();
			F.Combat->EndEncounter();
			TestEqual(TEXT("Crew dead"), Result.Outcome, EDwCombatOutcome::DefeatCrewDead);
			TestTrue(TEXT("Applied"), Run->ApplyCombatResult(Result));
			TestEqual(TEXT("Defeat"), Run->GetPhase(), EDwRunPhase::Defeat);
			TestEqual(TEXT("End reason"), Run->GetState().Stats.EndReason, EDwRunEndReason::CrewDead);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
