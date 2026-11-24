// Copyright Brandon Hillig 2026

// DW-209 + DW-301: the Kraken boss (Docs/Architecture/08-Kraken-Boss.md). The arena builder, layout rules and Kraken
// rules are pure; the encounter is the real content (Tools/CreateCombatContent.ps1): a body (HP from DT_UnitTuning) out in the water and
// four 60 HP tentacles along the rail, five moves gated by phase, cut tentacles weakening it (B2), melee only reaching
// the tentacles while muskets and cannons reach the body (B4), the enraged phase starting on its next turn (B7), and
// intents stored for the brain and HUD. The Kraken's turns are scripted here; choosing moves is DW-302's brain. Played
// through the real run: the boss node launches it, killing the body is VictoryBoss and ends the run in Victory.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/DwAttributeSets.h"
#include "Abilities/DwGameplayEffects.h"
#include "Board/DwBoardBuilder.h"
#include "Board/DwDeckLayout.h"
#include "Combat/DwCombatRules.h"
#include "Combat/DwEncounterSetupBuilder.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwKrakenDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Misc/DataValidation.h"
#include "Misc/ScopeExit.h"
#include "Presentation/DwAudioCueMap.h"
#include "Presentation/DwPresentationSubsystem.h"
#include "Sound/SoundBase.h"
#include "Run/DwRun.h"
#include "Run/DwRunSubsystem.h"
#include "Tests/Combat/DwCombatTestFixture.h"
#include "Tests/DwTestAbilities.h"
#include "Tests/Run/DwRunTestContent.h"

BEGIN_DEFINE_SPEC(FDwKrakenSpec, "DeadwaterRun.Run.Kraken",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	FDwCombatFixture F; // world only; the fight comes from content
	const UDwGameData* GameData = nullptr;
	UDwRun* Run = nullptr;

	/** The player deck of DA_Deck_PlayerShip (6 x 4, boarding edges in the rightmost column, cannons at (1,1) and (1,3)). */
	static TArray<FString> PlayerDeck() { return { TEXT("S...NE"), TEXT("SC.MLE"), TEXT("S.c..E"), TEXT("SC...E") }; }
	/** DA_Deck_KrakenArena (UDwKrakenContentLibrary::ArenaRows): four tentacle slots on the rail, the body three tiles out. */
	static TArray<FString> Arena() { return { TEXT("~~~~T"), TEXT("~~~~T"), TEXT("~~B~T"), TEXT("~~~~T") }; }

	/** On the composed board (player deck x 0-5, the arena mirrored at x 6-10). */
	static FIntPoint AnchorTile() { return FIntPoint(8, 2); }
	static FIntPoint SlotTile(int32 Row) { return FIntPoint(6, Row); }

	/** Handles in spawn order: crew 1-3 (1 = captain), then the body, then the four tentacles. */
	static FDwUnitHandle Captain() { return FDwCombatFixture::CrewHandle(0); }
	static FDwUnitHandle Swordsman() { return FDwCombatFixture::CrewHandle(1); }
	static FDwUnitHandle Sniper() { return FDwCombatFixture::CrewHandle(2); }
	static FDwUnitHandle Body() { return FDwUnitHandle(4); }
	static FDwUnitHandle Tentacle(int32 Index) { return FDwUnitHandle(5 + Index); }

	const UDwEncounterDefinition* Kraken() const { return GameData ? GameData->KrakenEncounter.Get() : nullptr; }

	const UDwAbilityDefinition* Move(const FGameplayTag& Tag) const
	{
		const UDwUnitDefinition* BodyDef = Kraken() && Kraken()->Kraken ? Kraken()->Kraken->Body.Get() : nullptr;
		if (!BodyDef)
		{
			return nullptr;
		}
		if (BodyDef->BasicAttack && BodyDef->BasicAttack->AbilityTag == Tag)
		{
			return BodyDef->BasicAttack;
		}
		const TObjectPtr<UDwAbilityDefinition>* Found = BodyDef->DefaultAbilities.FindByPredicate([&Tag](const UDwAbilityDefinition* A) { return A && A->AbilityTag == Tag; });
		return Found ? Found->Get() : nullptr;
	}

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

	/** The Kraken fight outside a run, with FreshCrew(). */
	bool StartFreshKrakenFight(FDwEncounterSetup& Setup, int32 Seed = 7)
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

	/** Scripts the Kraken's turns (tentacles never get one). The controller ends the unit after OnUnit. */
	UDwTestScriptedController* ScriptKraken(TFunction<void(UDwCombatSubsystem&, FDwUnitHandle)> OnUnit)
	{
		UDwTestScriptedController* Script = F.Root(NewObject<UDwTestScriptedController>(F.Combat));
		Script->OnUnitTurn = MoveTemp(OnUnit);
		F.Combat->SetSideController(EDwSide::Enemy, Script);
		return Script;
	}

	EDwCommandResult KrakenUses(const FGameplayTag& MoveTag, const FDwAbilityTarget& Target, FText* OutReason = nullptr)
	{
		return F.Combat->SubmitCommand(FDwCombatCommand::MakeAbility(Body(), MoveTag, Target), OutReason);
	}

	FDwAbilityTarget AtUnit(FDwUnitHandle Unit) const { return FDwAbilityTarget::MakeUnit(Unit, F.Unit(Unit)->GetTile()); }

	void SetHealth(FDwUnitHandle Unit, float Health)
	{
		F.Asc(Unit)->SetNumericAttributeBase(UDwUnitAttributeSet::GetHealthAttribute(), Health);
	}

	/** The body's max HP: a balance number (DT_UnitTuning KrakenBody, DW-401), so tests read it instead of pinning it. */
	float BodyMax() const { return F.Attr(Body(), UDwUnitAttributeSet::GetMaxHealthAttribute()); }

	/** Big crews, so random multi-hit moves never kill anyone (or the captain, C10) mid-test. */
	void ToughenCrew()
	{
		for (int32 Index = 0; Index < 3; ++Index)
		{
			F.Asc(FDwCombatFixture::CrewHandle(Index))->SetNumericAttributeBase(UDwUnitAttributeSet::GetMaxHealthAttribute(), 1000.f);
			SetHealth(FDwCombatFixture::CrewHandle(Index), 1000.f);
		}
	}

	/** Damage the body dealt to the player side (crew events) since the listener was last reset. */
	TArray<float> KrakenHits() const
	{
		TArray<float> Hits;
		for (const FDwCombatEvent& Event : F.Listener->Events)
		{
			if (Event.Type == EDwCombatEventType::UnitDamaged && Event.Source == Body())
			{
				Hits.Add(Event.Magnitude);
			}
		}
		return Hits;
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

	bool StartRun(const TCHAR* CaptainName, int32 Seed)
	{
		const UDwCaptainDefinition* CaptainDef = UDwRunSubsystem::FindCaptain(CaptainName);
		if (!TestNotNull(TEXT("DA_GameData"), GameData) || !TestNotNull(TEXT("Captain"), CaptainDef))
		{
			return false;
		}
		Run = F.Root(NewObject<UDwRun>());
		TArray<FString> Errors;
		const bool bOk = Run->Initialize(*GameData, *CaptainDef, Seed, UDwRunSubsystem::GatherEncounterPool(), &Errors);
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
			if (Run->GetPhase() == EDwRunPhase::Event) { TestTrue(TEXT("Passed the event"), DwRunTest::PassEvent(*Run)); }
		}
		TArray<FString> Errors;
		const bool bBuilt = Run->BuildEncounterSetup(OutSetup, Errors);
		return TestTrue(FString::Printf(TEXT("Setup at node %d: %s"), Run->GetState().CurrentNode, *FString::Join(Errors, TEXT("; "))), bBuilt)
			&& TestTrue(TEXT("Fight started"), F.Combat->StartEncounter(OutSetup));
	}

	/** The Kraken body: the enemy with Unit.Kraken.Body (in a run, enemy handles follow the surviving crew). */
	FDwUnitHandle RunBody() const
	{
		return F.Combat->GetKrakenState().Body;
	}

	/** Every living enemy dies by the captain's hand; deaths outside a command count at the turn boundary. */
	void CaptainKillsEveryEnemy()
	{
		const FDwUnitHandle RunCaptain = FindCrewUnit(Run->GetState().Crew[0].Id);
		for (const ADwCombatUnit* Unit : F.Combat->GetUnits())
		{
			if (Unit->GetSide() == EDwSide::Enemy && Unit->IsAlive())
			{
				F.ApplyEffect(DwGas::GetSharedEffects().Kill, Unit->GetHandle(), RunCaptain);
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
		It("puts the player deck beside a neutral arena: the body anchor out in the water, four tentacle slots on the rail, nothing to board", [this]()
		{
			const FDwGridModel Grid = FDwBoardBuilder::BuildKrakenArena(FDwDeckAscii::Parse(PlayerDeck()), FDwDeckAscii::Parse(Arena()));
			TestEqual(TEXT("11 x 4"), Grid.Size, FIntPoint(11, 4));
			const FDwTile* Anchor = Grid.GetTile(AnchorTile()); // the arena is mirrored: its rightmost column lies next to the rail
			if (!TestNotNull(TEXT("Anchor"), Anchor)) { return; }
			TestTrue(TEXT("Body anchor"), Anchor->Tags.HasTagExact(DwTags::Tile_Kraken_Body));
			TestTrue(TEXT("Enemy spawn"), Anchor->Tags.HasTagExact(DwTags::Tile_Spawn_Enemy));

			int32 Slots = 0;
			for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
			{
				const FIntPoint P = Grid.ToTile(Index);
				if (P.X < 6)
				{
					continue;
				}
				const FDwTile& T = Grid.Tiles[Index];
				const bool bSlot = T.Tags.HasTagExact(DwTags::Tile_Kraken_Tentacle);
				Slots += bSlot ? 1 : 0;
				TestEqual(TEXT("Arena tiles are nobody's deck"), T.Owner, EDwSide::Neutral);
				TestEqual(*FString::Printf(TEXT("(%d,%d): only the anchor and the slots hold anything"), P.X, P.Y), Grid.IsStandable(P), P == AnchorTile() || bSlot);
				TestEqual(*FString::Printf(TEXT("(%d,%d): slots are on the rail column"), P.X, P.Y), bSlot, P.X == 6);
			}
			TestEqual(TEXT("Four tentacle slots"), Slots, 4);

			TArray<FDwGridStep> Steps;
			for (int32 Row = 0; Row < 4; ++Row)
			{
				Grid.GetNeighbors(FIntPoint(5, Row), FDwGridRules(), Steps);
				TestFalse(TEXT("No step from the rail onto a tentacle slot"), Steps.ContainsByPredicate([Row](const FDwGridStep& S) { return S.Tile == SlotTile(Row); }));
			}
			Grid.GetNeighbors(AnchorTile(), FDwGridRules(), Steps);
			TestTrue(TEXT("No step off the anchor"), Steps.IsEmpty());
			TestEqual(TEXT("ASCII round trip"), FString::Join(FDwDeckAscii::Export(FDwDeckAscii::Parse(Arena())), TEXT("|")), FString::Join(Arena(), TEXT("|")));
		});

		It("validates arenas: one body anchor, no boarding edge, tentacle slots on the rail column; ship decks still need an edge", [this]()
		{
			auto Has = [](const TArray<FString>& Errors, const TCHAR* Text) { return Errors.ContainsByPredicate([Text](const FString& E) { return E.Contains(Text); }); };
			TestTrue(TEXT("The arena is valid"), MakeLayout(Arena())->Validate().IsEmpty());
			TestTrue(TEXT("Two anchors"), Has(MakeLayout({ TEXT("~~~B"), TEXT("~~~B") })->Validate(), TEXT("exactly one body anchor")));
			TestTrue(TEXT("A boarding edge"), Has(MakeLayout({ TEXT("~~SE"), TEXT("~~~B") })->Validate(), TEXT("nothing to board")));
			TestTrue(TEXT("A slot off the rail"), Has(MakeLayout({ TEXT("~T~~"), TEXT("~~~B") })->Validate(), TEXT("rail column")));
			TestTrue(TEXT("Ship deck without an edge"), Has(MakeLayout({ TEXT("S..."), TEXT("S...") })->Validate(), TEXT("boarding edge")));
		});
	});

	Describe("Encounter", [this]()
	{
#if WITH_EDITOR
		It("is DA_GameData.KrakenEncounter: valid, a body with its tuned HP, five moves, four 60 HP tentacles, two phases", [this]()
		{
			const UDwEncounterDefinition* Encounter = Kraken();
			if (!TestNotNull(TEXT("KrakenEncounter"), Encounter) || !TestNotNull(TEXT("Kraken definition"), Encounter->Kraken.Get())) { return; }
			const UDwKrakenDefinition* Def = Encounter->Kraken;
			TestEqual(TEXT("Kind"), Encounter->Kind, EDwEncounterKind::Kraken);
			TArray<const UObject*> Assets = { Encounter, Def, Def->Body.Get(), Def->Tentacle.Get(), Def->Arena.Get() };
			const FGameplayTag MoveTags[] = { DwTags::Ability_Kraken_TentacleSlam, DwTags::Ability_Kraken_KrankySwipe, DwTags::Ability_Kraken_TentacleGrab,
				DwTags::Ability_Kraken_ArmsOfFury, DwTags::Ability_Kraken_EyesOfTheSea };
			for (const FGameplayTag& Tag : MoveTags)
			{
				TestNotNull(*FString::Printf(TEXT("The body has %s"), *Tag.ToString()), Move(Tag));
				Assets.Add(Move(Tag));
			}
			for (const UObject* Asset : Assets)
			{
				FDataValidationContext Context;
				TestEqual(*FString::Printf(TEXT("%s is valid"), *GetNameSafe(Asset)), Asset ? Asset->IsDataValid(Context) : EDataValidationResult::Invalid,
					EDataValidationResult::Valid);
			}
			const FDwUnitTuningRow* BodyRow = Def->Body ? Def->Body->GetTuningRow<FDwUnitTuningRow>() : nullptr;
			const FDwUnitTuningRow* TentacleRow = Def->Tentacle ? Def->Tentacle->GetTuningRow<FDwUnitTuningRow>() : nullptr;
			if (!TestNotNull(TEXT("KrakenBody row"), BodyRow) || !TestNotNull(TEXT("KrakenTentacle row"), TentacleRow)) { return; }
			TestTrue(TEXT("Body HP from tuning (the design number is checked in DeadwaterRun.Data.Tuning)"), BodyRow->GetMaxHealthAtLevel(1) > 0);
			TestEqual(TEXT("60 HP tentacles (B1)"), static_cast<float>(TentacleRow->GetMaxHealthAtLevel(1)), 60.f);
			const FGameplayTag BodyTags[] = { DwTags::Unit_Kraken_Body, DwTags::Unit_Immune_Fear, DwTags::Unit_Immune_Grapple, DwTags::Unit_Immune_Freeze,
				DwTags::Unit_Immune_Melee, DwTags::Unit_CannonTarget };
			for (const FGameplayTag& Tag : BodyTags)
			{
				TestTrue(*FString::Printf(TEXT("Body: %s"), *Tag.ToString()), Def->Body->UnitTags.HasTagExact(Tag));
			}
			TestTrue(TEXT("Tentacle: Unit.Kraken.Tentacle"), Def->Tentacle->UnitTags.HasTagExact(DwTags::Unit_Kraken_Tentacle));
			TestFalse(TEXT("Tentacles can be frozen (K10)"), Def->Tentacle->UnitTags.HasTagExact(DwTags::Unit_Immune_Freeze));

			if (!TestEqual(TEXT("Two phases"), Def->Phases.Num(), 2)) { return; }
			TestEqual(TEXT("Initial from full health"), Def->Phases[0].EnterBelowHealthPct, 1.f);
			TestEqual(TEXT("Enraged below 30%"), Def->Phases[1].EnterBelowHealthPct, 0.3f);
			FGameplayTagContainer Initial;
			Initial.AddTag(DwTags::Ability_Kraken_TentacleSlam); Initial.AddTag(DwTags::Ability_Kraken_KrankySwipe); Initial.AddTag(DwTags::Ability_Kraken_TentacleGrab);
			FGameplayTagContainer Enraged;
			Enraged.AddTag(DwTags::Ability_Kraken_ArmsOfFury); Enraged.AddTag(DwTags::Ability_Kraken_EyesOfTheSea);
			TestTrue(TEXT("Initial: Slam, Swipe, Grab"), FDwKrakenRules::GetPhaseMoves(Def->Phases[0]) == Initial);
			TestTrue(TEXT("Enraged: Arms of Fury, Eyes of the Sea"), FDwKrakenRules::GetPhaseMoves(Def->Phases[1]) == Enraged);
			TestTrue(TEXT("Enrage cue"), Def->Phases[1].EnterCue == DwTags::GameplayCue_Kraken_Enrage);
			TestTrue(TEXT("Enraged music"), Def->Phases[1].MusicState == DwTags::Music_Boss_Enraged);

			const FDwKrakenTuningRow* KrakenRow = Def->GetTuningRow<FDwKrakenTuningRow>();
			if (!TestNotNull(TEXT("DT_KrakenTuning row"), KrakenRow)) { return; }
			TestEqual(TEXT("Four tentacles (B1)"), KrakenRow->TentacleCount, 4);
			TestEqual(TEXT("Cut tentacles weaken it (B2)"), Def->TentacleRules.Effect, EDwTentacleEffect::ReduceDamage);
			TestEqual(TEXT("-10% per tentacle"), KrakenRow->DamageMultiplierPerMissingTentacle, 0.1f);
			TestTrue(TEXT("The body is driven by an AI profile until DW-302"), Encounter->AiProfile != nullptr);
			TestFalse(TEXT("Never in the fight pool"), UDwRunSubsystem::GatherEncounterPool().Contains(Encounter));
		});
#endif

		It("builds a boss setup: the arena, no enemy ship, the body on its anchor and the tentacles on their slots", [this]()
		{
			if (!TestNotNull(TEXT("DA_GameData"), GameData) || !TestNotNull(TEXT("KrakenEncounter"), Kraken())) { return; }
			FDwEncounterSetup Setup;
			TArray<FString> Errors;
			if (!TestTrue(FString::Printf(TEXT("Built: %s"), *FString::Join(Errors, TEXT("; "))),
				FDwEncounterSetupBuilder::Build(*GameData, *Kraken(), FreshCrew(), 3, Setup, Errors))) { return; }
			TestFalse(TEXT("No enemy ship"), Setup.bHasEnemyShip);
			TestTrue(TEXT("Boss fight"), Setup.OutcomeConfig.bIsBossFight);
			TestTrue(TEXT("The Kraken's rules travel with the setup"), Setup.Kraken == Kraken()->Kraken);
			if (!TestEqual(TEXT("The body and four tentacles"), Setup.Enemies.Num(), 5)) { return; }
			const FDwCombatantSetup& BodySetup = Setup.Enemies[0];
			TestTrue(TEXT("Body first (acting order)"), BodySetup.Definition == Kraken()->Kraken->Body);
			TestTrue(TEXT("On the anchor"), BodySetup.bUseSpawnTile && BodySetup.SpawnTile == AnchorTile());
			TestTrue(TEXT("Driven by the encounter's AI profile"), BodySetup.AiProfile == Kraken()->AiProfile);
			for (int32 Index = 1; Index < 5; ++Index)
			{
				const FDwCombatantSetup& T = Setup.Enemies[Index];
				TestTrue(TEXT("Tentacle"), T.Definition == Kraken()->Kraken->Tentacle);
				TestTrue(TEXT("On its slot"), T.bUseSpawnTile && T.SpawnTile == SlotTile(Index - 1));
				TestNull(TEXT("No AI: tentacles never act"), T.AiProfile.Get());
			}
			TestEqual(TEXT("Player deck + arena"), Setup.Grid.Size.X, GameData->DefaultPlayerDeck->Size.X + Kraken()->Kraken->Arena->Size.X);
			TestEqual(TEXT("Player ship from DT_ShipTuning"), Setup.PlayerShip.MaxHealth, 250.f);

			TArray<FString> Refused;
			TestFalse(TEXT("Not a ship battle"), FDwEncounterSetupBuilder::BuildShipBattle(*GameData, *Kraken(), FreshCrew(), 3, Setup, Refused));
			TestFalse(TEXT("A ship battle is not the Kraken"), GameData->DevEncounter && FDwEncounterSetupBuilder::BuildKraken(*GameData, *GameData->DevEncounter, FreshCrew(), 3, Setup, Refused));
			TestEqual(TEXT("Two refusals"), Refused.Num(), 2);
		});

		It("refuses more tentacles than the arena has slots", [this]()
		{
			if (!TestNotNull(TEXT("KrakenEncounter"), Kraken())) { return; }
			UDataTable* Table = F.Root(NewObject<UDataTable>(GetTransientPackage()));
			Table->RowStruct = FDwKrakenTuningRow::StaticStruct();
			FDwKrakenTuningRow Row = *Kraken()->Kraken->GetTuningRow<FDwKrakenTuningRow>();
			Row.TentacleCount = 6;
			Table->AddRow(TEXT("TooMany"), Row);
			UDwKrakenDefinition* Def = F.Root(DuplicateObject<UDwKrakenDefinition>(Kraken()->Kraken.Get(), GetTransientPackage()));
			Def->Tuning.DataTable = Table;
			Def->Tuning.RowName = TEXT("TooMany");
			UDwEncounterDefinition* Encounter = F.Root(DuplicateObject<UDwEncounterDefinition>(Kraken(), GetTransientPackage()));
			Encounter->Kraken = Def;
			FDwEncounterSetup Setup;
			TArray<FString> Errors;
			TestFalse(TEXT("Refused"), FDwEncounterSetupBuilder::Build(*GameData, *Encounter, FreshCrew(), 3, Setup, Errors));
			TestTrue(TEXT("Says why"), Errors.Num() == 1 && Errors[0].Contains(TEXT("tentacle slots")));
#if WITH_EDITOR
			FDataValidationContext Context;
			TestEqual(TEXT("Validation catches it too"), Def->IsDataValid(Context), EDataValidationResult::Invalid);
#endif
		});
	});

	Describe("Rules", [this]()
	{
		It("picks the phase: initial down to 30% exactly, enraged below it (B7 thresholds)", [this]()
		{
			TArray<FDwKrakenPhase> Phases;
			Phases.AddDefaulted_GetRef().EnterBelowHealthPct = 1.f;
			Phases.AddDefaulted_GetRef().EnterBelowHealthPct = 0.3f;
			TestEqual(TEXT("100%"), FDwKrakenRules::GetPhaseIndex(Phases, 1.f), 0);
			TestEqual(TEXT("31%"), FDwKrakenRules::GetPhaseIndex(Phases, 0.31f), 0);
			TestEqual(TEXT("30%"), FDwKrakenRules::GetPhaseIndex(Phases, 0.3f), 0);
			TestEqual(TEXT("29%"), FDwKrakenRules::GetPhaseIndex(Phases, 0.29f), 1);
			TestEqual(TEXT("0%"), FDwKrakenRules::GetPhaseIndex(Phases, 0.f), 1);
			Phases.AddDefaulted_GetRef().EnterBelowHealthPct = 0.1f;
			TestEqual(TEXT("A third phase below 10%"), FDwKrakenRules::GetPhaseIndex(Phases, 0.05f), 2);
			TestEqual(TEXT("No phases"), FDwKrakenRules::GetPhaseIndex(TArray<FDwKrakenPhase>(), 0.5f), 0);
		});

		It("counts actions per phase, minus lost actions when tentacles reduce actions, never below 1", [this]()
		{
			FDwKrakenTuningRow Row;
			Row.BaseActionsPerTurn = { 1, 2 };
			Row.ActionsLostPerTentacle = 0.5f;
			TestEqual(TEXT("Phase 0"), FDwKrakenRules::GetActionsPerTurn(Row, EDwTentacleEffect::ReduceDamage, 0, 0), 1);
			TestEqual(TEXT("Phase 1"), FDwKrakenRules::GetActionsPerTurn(Row, EDwTentacleEffect::ReduceDamage, 1, 4), 2);
			TestEqual(TEXT("No entry = 1"), FDwKrakenRules::GetActionsPerTurn(Row, EDwTentacleEffect::ReduceDamage, 5, 0), 1);
			const int32 Expected[] = { 2, 2, 1, 1, 1 };
			for (int32 Dead = 0; Dead <= 4; ++Dead)
			{
				TestEqual(*FString::Printf(TEXT("ReduceActions, %d cut"), Dead), FDwKrakenRules::GetActionsPerTurn(Row, EDwTentacleEffect::ReduceActions, 1, Dead), Expected[Dead]);
				TestEqual(*FString::Printf(TEXT("Both, %d cut"), Dead), FDwKrakenRules::GetActionsPerTurn(Row, EDwTentacleEffect::Both, 1, Dead), Expected[Dead]);
			}
		});

		It("weakens every hit by 10% per cut tentacle when tentacles reduce damage (B2), down to nothing at most", [this]()
		{
			FDwKrakenTuningRow Row;
			Row.DamageMultiplierPerMissingTentacle = 0.1f;
			TestEqual(TEXT("0 cut"), FDwKrakenRules::GetDamageMultiplier(Row, EDwTentacleEffect::ReduceDamage, 0), 1.f);
			TestEqual(TEXT("1 cut"), FDwKrakenRules::GetDamageMultiplier(Row, EDwTentacleEffect::ReduceDamage, 1), 0.9f);
			TestEqual(TEXT("4 cut"), FDwKrakenRules::GetDamageMultiplier(Row, EDwTentacleEffect::Both, 4), 0.6f);
			TestEqual(TEXT("Actions only: full damage"), FDwKrakenRules::GetDamageMultiplier(Row, EDwTentacleEffect::ReduceActions, 4), 1.f);
			TestEqual(TEXT("Never negative"), FDwKrakenRules::GetDamageMultiplier(Row, EDwTentacleEffect::ReduceDamage, 20), 0.f);
		});
	});

	Describe("Fight", [this]()
	{
		It("spawns the body at full HP on its anchor and four 60 HP tentacles on the rail, with no enemy ship", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFreshKrakenFight(Setup)) { return; }
			TestEqual(TEXT("Eight units: three crew, the body, four tentacles"), F.Combat->GetUnits().Num(), 8);
			TestTrue(TEXT("Full HP"), BodyMax() > 0.f && F.Health(Body()) == BodyMax());
			TestEqual(TEXT("Body on the anchor"), F.Unit(Body())->GetTile(), AnchorTile());
			for (int32 Index = 0; Index < 4; ++Index)
			{
				TestEqual(TEXT("Tentacle at 60"), F.Health(Tentacle(Index)), 60.f);
				TestEqual(TEXT("On its slot"), F.Unit(Tentacle(Index))->GetTile(), SlotTile(Index));
			}
			TestNotNull(TEXT("Player ship"), F.Combat->GetShip(EDwSide::Player));
			TestNull(TEXT("No enemy ship"), F.Combat->GetShip(EDwSide::Enemy));
			TestEqual(TEXT("Player first"), F.Combat->GetActiveSide(), EDwSide::Player);

			const FDwKrakenSnapshot& K = F.Combat->GetSnapshot().Kraken;
			TestTrue(TEXT("Kraken state"), K.IsSet() && K.Body == Body());
			TestEqual(TEXT("Initial phase"), K.PhaseId, FName(TEXT("Initial")));
			TestEqual(TEXT("Three initial moves"), K.PhaseMoves.Num(), 3);
			TestEqual(TEXT("Four tentacles"), K.TentaclesTotal, 4);
			TestEqual(TEXT("None cut"), K.TentaclesDead, 0);
			TestEqual(TEXT("Full strength"), K.DamageMultiplier, 1.f);
		});

		It("lets cutlasses reach only the tentacles, and muskets and cannons the body (B4)", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFreshKrakenFight(Setup)) { return; }
			const UDwAbilityDefinition* Cutlass = GameData->DevCrew[1]->BasicAttack;
			const UDwAbilityDefinition* Musket = GameData->DevCrew[2]->BasicAttack;
			auto Attack = [this](FDwUnitHandle Unit, const UDwAbilityDefinition* Ability, FDwUnitHandle Target, FText* Reason = nullptr)
			{
				return F.Combat->SubmitCommand(FDwCombatCommand::MakeAbility(Unit, Ability->AbilityTag, AtUnit(Target)), Reason);
			};

			TestEqual(TEXT("Musket from the spawn: out of range"), Attack(Sniper(), Musket, Body()), EDwCommandResult::InvalidTarget);
			F.Combat->MoveUnit(Swordsman(), FIntPoint(5, 2), /*bSnapActor*/ true); // on the rail, beside a tentacle
			F.Combat->MoveUnit(Sniper(), FIntPoint(5, 3), true);
			FText Reason;
			TestEqual(TEXT("Cutlass at the body: refused"), Attack(Swordsman(), Cutlass, Body(), &Reason), EDwCommandResult::InvalidTarget);
			TestTrue(*FString::Printf(TEXT("Says melee hits the tentacles (%s)"), *Reason.ToString()), Reason.ToString().Contains(TEXT("tentacles")));
			TestEqual(TEXT("Cutlass at the tentacle beside him"), Attack(Swordsman(), Cutlass, Tentacle(2)), EDwCommandResult::Accepted);
			TestEqual(TEXT("60 - 25"), F.Health(Tentacle(2)), 35.f);
			const float Full = BodyMax();
			TestEqual(TEXT("The body is untouched (B3)"), F.Health(Body()), Full);
			TestEqual(TEXT("Musket from the rail at the body"), Attack(Sniper(), Musket, Body()), EDwCommandResult::Accepted);
			TestEqual(TEXT("Full - 20"), F.Health(Body()), Full - 20.f);

			// Cannons: with no enemy ship the guns aim at the body, flat CannonDamage whatever the gunner's bonus.
			const UDwAbilityDefinition* Cannon = GameData->CommonAbilities.FindByPredicate([](const UDwAbilityDefinition* A) { return A && A->AbilityTag == DwTags::Ability_Cannon; })->Get();
			F.Combat->MoveUnit(Captain(), FIntPoint(1, 1), true);
			F.Asc(Captain())->SetNumericAttributeBase(UDwUnitAttributeSet::GetDamageBonusAttribute(), 0.5f);
			const FDwCombatSnapshot Snapshot = F.Combat->GetSnapshot();
			const UDwGameplayAbility* CannonCdo = FDwCombatQueries::GetAbilityCDO(Cannon);
			FDwTargetingContext Context;
			Context.Snapshot = &Snapshot;
			Context.Ability = Cannon;
			Context.Source = Captain();
			TArray<FDwAbilityTarget> Targets;
			CannonCdo->GatherValidTargets(Context, Targets);
			TestTrue(TEXT("The body is the cannons' only target"), Targets.Num() == 1 && Targets[0].Unit == Body());
			const FDwAbilityPreview Preview = CannonCdo->PreviewOutcome(Context, AtUnit(Body()));
			TestTrue(TEXT("Preview: 70"), Preview.bValid && Preview.Targets.Num() == 1 && Preview.Targets[0].Damage == 70);
			TestEqual(TEXT("Fire"), F.Combat->SubmitCommand(FDwCombatCommand::MakeAbility(Captain(), DwTags::Ability_Cannon, AtUnit(Body()))), EDwCommandResult::Accepted);
			TestEqual(TEXT("Full - 20 - 70, no gunner bonus"), F.Health(Body()), Full - 90.f);
		});

		It("gives tentacles no turn and the body one action a turn", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFreshKrakenFight(Setup)) { return; }
			TArray<FDwUnitHandle> Actors;
			TArray<float> Actions;
			ScriptKraken([&](UDwCombatSubsystem&, FDwUnitHandle Unit)
			{
				Actors.Add(Unit);
				Actions.Add(F.Attr(Unit, UDwUnitAttributeSet::GetActionPointsAttribute()));
			});
			F.EndTurn();
			F.EndTurn();
			TestTrue(TEXT("Only the body acted, once per turn"), Actors.Num() == 2 && Actors[0] == Body() && Actors[1] == Body());
			TestTrue(TEXT("One action each turn"), Actions.Num() == 2 && Actions[0] == 1.f && Actions[1] == 1.f);
		});

		It("is VictoryBoss when the body dies with every tentacle still up, and plain defeats still apply", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFreshKrakenFight(Setup)) { return; }
			F.ApplyEffect(DwGas::GetSharedEffects().Kill, Body(), Captain());
			F.EndTurn();
			TestEqual(TEXT("VictoryBoss"), F.Combat->GetOutcome(), EDwCombatOutcome::VictoryBoss);

			if (!StartFreshKrakenFight(Setup)) { return; }
			for (int32 Index = 0; Index < 4; ++Index)
			{
				F.ApplyEffect(DwGas::GetSharedEffects().Kill, Tentacle(Index), Captain());
			}
			F.EndTurn();
			TestEqual(TEXT("Cutting every tentacle doesn't win"), F.Combat->GetOutcome(), EDwCombatOutcome::None);

			if (!StartFreshKrakenFight(Setup)) { return; }
			F.ApplyEffect(DwGas::GetSharedEffects().Kill, Captain());
			F.EndTurn();
			TestEqual(TEXT("Captain dead (C10)"), F.Combat->GetOutcome(), EDwCombatOutcome::DefeatCaptainDead);
		});
	});

	Describe("Moves", [this]()
	{
		It("Tentacle Slam: 50 to one crew member, or to the ship", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFreshKrakenFight(Setup)) { return; }
			int32 Turn = 0;
			ScriptKraken([&](UDwCombatSubsystem&, FDwUnitHandle)
			{
				TestEqual(TEXT("Slam"), KrakenUses(DwTags::Ability_Kraken_TentacleSlam, Turn++ == 0 ? AtUnit(Swordsman()) : FDwAbilityTarget::MakeShip(EDwSide::Player)),
					EDwCommandResult::Accepted);
			});
			F.EndTurn();
			TestEqual(TEXT("Swordsman 100 - 50"), F.Health(Swordsman()), 50.f);
			F.EndTurn();
			TestEqual(TEXT("Ship 250 - 50"), F.ShipHealth(EDwSide::Player), 200.f);
		});

		It("Kranky Swipe: 20 to every crew member, not the ship (B8)", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFreshKrakenFight(Setup)) { return; }
			ScriptKraken([&](UDwCombatSubsystem&, FDwUnitHandle)
			{
				TestEqual(TEXT("Swipe"), KrakenUses(DwTags::Ability_Kraken_KrankySwipe, AtUnit(Captain())), EDwCommandResult::Accepted);
			});
			F.EndTurn();
			for (int32 Index = 0; Index < 3; ++Index)
			{
				TestEqual(TEXT("Crew 100 - 20"), F.Health(FDwCombatFixture::CrewHandle(Index)), 80.f);
			}
			TestEqual(TEXT("Ship untouched"), F.ShipHealth(EDwSide::Player), 250.f);
			TestEqual(TEXT("Its own tentacles untouched"), F.Health(Tentacle(0)), 60.f);
		});

		It("Tentacle Grab: the target can't move on the player's next turn, then is free", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFreshKrakenFight(Setup)) { return; }
			bool bGrabbed = false;
			ScriptKraken([&](UDwCombatSubsystem&, FDwUnitHandle)
			{
				if (!bGrabbed)
				{
					bGrabbed = true;
					TestEqual(TEXT("Grab"), KrakenUses(DwTags::Ability_Kraken_TentacleGrab, AtUnit(Swordsman())), EDwCommandResult::Accepted);
				}
			});
			F.EndTurn();
			TestTrue(TEXT("Rooted"), F.Asc(Swordsman())->HasMatchingGameplayTag(DwTags::Status_Rooted));
			TestEqual(TEXT("Can't move"), F.MoveTo(Swordsman(), FIntPoint(1, 2)), EDwCommandResult::Blocked);
			TestEqual(TEXT("The others can"), F.MoveTo(Sniper(), FIntPoint(1, 2)), EDwCommandResult::Accepted);
			F.EndTurn();
			TestFalse(TEXT("Free after that turn"), F.Asc(Swordsman())->HasMatchingGameplayTag(DwTags::Status_Rooted));
			TestEqual(TEXT("Moves again"), F.MoveTo(Swordsman(), FIntPoint(2, 1)), EDwCommandResult::Accepted);
		});

		It("Arms of Fury: 3-5 random hits of 40 on the crew, never the ship", [this]()
		{
			TSet<int32> Counts;
			for (int32 Seed = 1; Seed <= 12; ++Seed)
			{
				FDwEncounterSetup Setup;
				if (!StartFreshKrakenFight(Setup, Seed)) { return; }
				ToughenCrew();
				SetHealth(Body(), 0.28f * BodyMax()); // 28%: enraged from its next turn
				bool bUsed = false;
				ScriptKraken([&](UDwCombatSubsystem&, FDwUnitHandle)
				{
					if (!bUsed)
					{
						bUsed = true;
						TestEqual(TEXT("Arms of Fury"), KrakenUses(DwTags::Ability_Kraken_ArmsOfFury, AtUnit(Captain())), EDwCommandResult::Accepted);
					}
				});
				F.Listener->Reset();
				F.EndTurn();
				const TArray<float> Hits = KrakenHits();
				TestTrue(*FString::Printf(TEXT("Seed %d: %d hits"), Seed, Hits.Num()), Hits.Num() >= 3 && Hits.Num() <= 5);
				TestTrue(TEXT("40 each"), !Hits.ContainsByPredicate([](float H) { return !FMath::IsNearlyEqual(H, 40.f); }));
				TestEqual(TEXT("Ship untouched"), F.ShipHealth(EDwSide::Player), 250.f);
				Counts.Add(Hits.Num());
				F.Combat->EndEncounter();
			}
			TestTrue(TEXT("The hit count varies with the seed"), Counts.Num() > 1);
		});

		It("Eyes of the Sea: two actions on its next turn only", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFreshKrakenFight(Setup)) { return; }
			ToughenCrew();
			SetHealth(Body(), 0.28f * BodyMax());
			int32 Turn = 0;
			TArray<float> Actions;
			TArray<EDwCommandResult> Second;
			ScriptKraken([&](UDwCombatSubsystem&, FDwUnitHandle Unit)
			{
				Actions.Add(F.Attr(Unit, UDwUnitAttributeSet::GetActionPointsAttribute()));
				if (Turn++ == 0)
				{
					TestEqual(TEXT("Eyes of the Sea"), KrakenUses(DwTags::Ability_Kraken_EyesOfTheSea, AtUnit(Body())), EDwCommandResult::Accepted);
					return;
				}
				TestEqual(TEXT("First action"), KrakenUses(DwTags::Ability_Kraken_ArmsOfFury, AtUnit(Captain())), EDwCommandResult::Accepted);
				Second.Add(KrakenUses(DwTags::Ability_Kraken_ArmsOfFury, AtUnit(Captain())));
			});
			F.EndTurn();
			F.EndTurn();
			F.EndTurn();
			TestTrue(TEXT("1, then 2, then 1 action"), Actions.Num() == 3 && Actions[0] == 1.f && Actions[1] == 2.f && Actions[2] == 1.f);
			TestTrue(TEXT("A second action the turn after, none after that"), Second.Num() == 2 && Second[0] == EDwCommandResult::Accepted
				&& Second[1] == EDwCommandResult::CannotAfford);
		});
	});

	Describe("Tentacles", [this]()
	{
		It("weaken every Kraken hit by 10% each as they are cut, on crew and ship alike (B2)", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFreshKrakenFight(Setup)) { return; }
			F.ApplyEffect(DwGas::GetSharedEffects().Kill, Tentacle(0), Captain());
			const FDwKrakenSnapshot K = F.Combat->GetSnapshot().Kraken;
			TestEqual(TEXT("One cut"), K.TentaclesDead, 1);
			TestEqual(TEXT("x0.9"), K.DamageMultiplier, 0.9f, 0.0001f);

			// The preview is the result (the brain and the HUD read it).
			const FDwCombatSnapshot Snapshot = F.Combat->GetSnapshot();
			const UDwAbilityDefinition* Slam = Move(DwTags::Ability_Kraken_TentacleSlam);
			FDwTargetingContext Context;
			Context.Snapshot = &Snapshot;
			Context.Ability = Slam;
			Context.Source = Body();
			const FDwAbilityPreview Preview = FDwCombatQueries::GetAbilityCDO(Slam)->PreviewOutcome(Context, AtUnit(Swordsman()));
			TestTrue(TEXT("Preview: 45"), Preview.bValid && Preview.Targets.Num() == 1 && Preview.Targets[0].Damage == 45);

			int32 Turn = 0;
			ScriptKraken([&](UDwCombatSubsystem&, FDwUnitHandle)
			{
				KrakenUses(DwTags::Ability_Kraken_TentacleSlam, Turn++ == 0 ? AtUnit(Swordsman()) : FDwAbilityTarget::MakeShip(EDwSide::Player));
			});
			F.EndTurn();
			TestEqual(TEXT("Swordsman 100 - 45"), F.Health(Swordsman()), 55.f);
			for (int32 Index = 1; Index < 4; ++Index)
			{
				F.ApplyEffect(DwGas::GetSharedEffects().Kill, Tentacle(Index), Captain());
			}
			TestEqual(TEXT("All four cut: x0.6"), F.Combat->GetSnapshot().Kraken.DamageMultiplier, 0.6f, 0.0001f);
			F.EndTurn();
			TestEqual(TEXT("Ship 250 - 30"), F.ShipHealth(EDwSide::Player), 220.f);
		});
	});

	Describe("Phases", [this]()
	{
		It("only lets the body use its phase's moves, and never move, fire or use items", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFreshKrakenFight(Setup)) { return; }
			const FDwCombatSnapshot Snapshot = F.Combat->GetSnapshot();
			const FDwUnitSnapshot* BodySnap = Snapshot.FindUnit(Body());
			if (!TestNotNull(TEXT("Body"), BodySnap)) { return; }
			auto Check = [&](const FGameplayTag& Tag) { return FDwCombatQueries::CanActivate(Snapshot, *BodySnap, FDwCombatQueries::FindAbility(*BodySnap, Tag)); };
			TestEqual(TEXT("Slam"), Check(DwTags::Ability_Kraken_TentacleSlam), EDwCommandResult::Accepted);
			TestEqual(TEXT("Swipe"), Check(DwTags::Ability_Kraken_KrankySwipe), EDwCommandResult::Accepted);
			TestEqual(TEXT("Grab"), Check(DwTags::Ability_Kraken_TentacleGrab), EDwCommandResult::Accepted);
			TestEqual(TEXT("No Arms of Fury yet"), Check(DwTags::Ability_Kraken_ArmsOfFury), EDwCommandResult::Blocked);
			TestEqual(TEXT("No Eyes of the Sea yet"), Check(DwTags::Ability_Kraken_EyesOfTheSea), EDwCommandResult::Blocked);
			TestEqual(TEXT("Never moves"), Check(DwTags::Ability_Move), EDwCommandResult::Blocked);
			TestEqual(TEXT("Never fires"), Check(DwTags::Ability_Cannon), EDwCommandResult::Blocked);
		});

		It("enrages below 30% at the start of its next turn, with the cue's event, and swaps its moves (B7)", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFreshKrakenFight(Setup)) { return; }
			ToughenCrew();
			TArray<FName> PhaseAtTurn;
			TArray<EDwCommandResult> SlamResults;
			ScriptKraken([&](UDwCombatSubsystem& Combat, FDwUnitHandle)
			{
				PhaseAtTurn.Add(Combat.GetKrakenState().PhaseId);
				SlamResults.Add(KrakenUses(DwTags::Ability_Kraken_TentacleSlam, AtUnit(Swordsman())));
			});

			SetHealth(Body(), BodyMax() * 3.f / 10.f); // exactly 30%: still the initial phase
			F.EndTurn();
			SetHealth(Body(), BodyMax() * 29.f / 100.f); // 29%, during the player's turn
			TestEqual(TEXT("Not mid-turn"), F.Combat->GetKrakenState().PhaseId, FName(TEXT("Initial")));
			TestEqual(TEXT("No phase event yet"), F.Listener->Count(EDwCombatEventType::PhaseChanged), 0);
			F.EndTurn();

			TestTrue(TEXT("Initial at 30%, enraged at 29%"), PhaseAtTurn.Num() == 2 && PhaseAtTurn[0] == TEXT("Initial") && PhaseAtTurn[1] == TEXT("Enraged"));
			TestTrue(TEXT("Slam in the initial phase only"), SlamResults.Num() == 2 && SlamResults[0] == EDwCommandResult::Accepted
				&& SlamResults[1] == EDwCommandResult::Blocked);
			const FDwCombatEvent* Changed = F.Listener->Events.FindByPredicate([](const FDwCombatEvent& E) { return E.Type == EDwCombatEventType::PhaseChanged; });
			TestEqual(TEXT("One phase event"), F.Listener->Count(EDwCombatEventType::PhaseChanged), 1);
			TestTrue(TEXT("From the body, phase 1, enraged music"), Changed && Changed->Source == Body() && Changed->Magnitude == 1.f
				&& Changed->Tag == DwTags::Music_Boss_Enraged);
			FGameplayTagContainer Enraged;
			Enraged.AddTag(DwTags::Ability_Kraken_ArmsOfFury);
			Enraged.AddTag(DwTags::Ability_Kraken_EyesOfTheSea);
			TestTrue(TEXT("Enraged moves"), F.Combat->GetSnapshot().Kraken.PhaseMoves == Enraged);
		});
	});

	Describe("Intents", [this]()
	{
		It("stores declared intents for the snapshot, announces each one, and drops them when the unit dies", [this]()
		{
			FDwEncounterSetup Setup;
			if (!StartFreshKrakenFight(Setup)) { return; }
			FDwAiIntent Slam;
			Slam.AbilityTag = DwTags::Ability_Kraken_TentacleSlam;
			Slam.Target = AtUnit(Captain());
			Slam.bTelegraph = true;
			FDwAiIntent Swipe;
			Swipe.AbilityTag = DwTags::Ability_Kraken_KrankySwipe;
			Swipe.bTelegraph = true;
			F.Combat->DeclareIntents(Body(), { Slam, Swipe });

			const FDwCombatSnapshot Snapshot = F.Combat->GetSnapshot();
			const FDwUnitIntents* Stored = Snapshot.FindIntents(Body());
			TestTrue(TEXT("Both, in order"), Stored && Stored->Intents.Num() == 2 && Stored->Intents[0].AbilityTag == Slam.AbilityTag
				&& Stored->Intents[1].AbilityTag == Swipe.AbilityTag);
			TArray<FDwCombatEvent> Declared = F.Listener->Events.FilterByPredicate([](const FDwCombatEvent& E) { return E.Type == EDwCombatEventType::IntentDeclared; });
			TestTrue(TEXT("Two events"), Declared.Num() == 2);
			TestTrue(TEXT("The slam names its target"), Declared.Num() == 2 && Declared[0].Source == Body() && Declared[0].Tag == Slam.AbilityTag
				&& Declared[0].Target == Captain() && Declared[0].Magnitude == 0.f && Declared[1].Magnitude == 1.f);

			F.Combat->DeclareIntents(Body(), {});
			const FDwCombatSnapshot Cleared = F.Combat->GetSnapshot();
			TestNull(TEXT("Cleared"), Cleared.FindIntents(Body()));
			F.Combat->DeclareIntents(Body(), { Slam });
			F.ApplyEffect(DwGas::GetSharedEffects().Kill, Body(), Captain());
			TestNull(TEXT("Dropped on death"), F.Combat->GetIntents(Body()));
			F.Listener->Reset();
			F.Combat->DeclareIntents(Body(), { Slam });
			TestNull(TEXT("The dead declare nothing"), F.Combat->GetIntents(Body()));
			TestEqual(TEXT("No event"), F.Listener->Count(EDwCombatEventType::IntentDeclared), 0);
		});
	});


	Describe("Cues", [this]()
	{
		It("picks the death cue from the unit's tags: Kraken.Death for the body, TentacleCut for a tentacle, else Unit.Death", [this]()
		{
			TestEqual(TEXT("Body"), UDwCombatSubsystem::GetDeathCue(FGameplayTagContainer(DwTags::Unit_Kraken_Body)), FGameplayTag(DwTags::GameplayCue_Kraken_Death));
			TestEqual(TEXT("Tentacle"), UDwCombatSubsystem::GetDeathCue(FGameplayTagContainer(DwTags::Unit_Kraken_Tentacle)), FGameplayTag(DwTags::GameplayCue_Kraken_TentacleCut));
			TestEqual(TEXT("Crew"), UDwCombatSubsystem::GetDeathCue(FGameplayTagContainer()), FGameplayTag(DwTags::GameplayCue_Unit_Death));
		});

		It("watched: plays Kraken.Appear at the start, TentacleCut and Kraken.Death on those deaths, Attack.Miss on an empty tile", [this]()
		{
			// Every cue under test plays one engine click through DA_AudioCueMap's override; the sound-only GC_* notifies
			// (UDwAudioContentLibrary) must exist for any of them to reach the presentation subsystem.
			USoundBase* Click = LoadObject<USoundBase>(nullptr, TEXT("/Engine/VREditor/Sounds/VR_click1.VR_click1"));
			if (!TestNotNull(TEXT("Click"), Click)) { return; }
			const FGameplayTag Tags[] = { DwTags::GameplayCue_Kraken_Appear, DwTags::GameplayCue_Kraken_Death, DwTags::GameplayCue_Kraken_TentacleCut,
				DwTags::GameplayCue_Attack_Miss, DwTags::GameplayCue_Unit_Death };
			UDwAudioCueMap* Map = F.Root(NewObject<UDwAudioCueMap>(GetTransientPackage()));
			for (const FGameplayTag& Tag : Tags)
			{
				Map->Cues.Add(Tag).Start.AddDefaulted_GetRef().Sounds.Add(Click);
			}
			UDwPresentationSubsystem* Presentation = F.World->GetSubsystem<UDwPresentationSubsystem>();
			Presentation->SetCueMapOverride(Map);
			Presentation->SetInstant(false); // as when a player watches: ASCs are bound unsuppressed
			TArray<FGameplayTag> Played;
			const FDelegateHandle Handle = Presentation->OnCueSoundPlayed.AddLambda([&Played](const FGameplayTag& Tag, EDwCueSoundSection, USoundBase*) { Played.Add(Tag); });
			ON_SCOPE_EXIT { Presentation->OnCueSoundPlayed.Remove(Handle); Presentation->SetCueMapOverride(nullptr); Presentation->SetInstant(true); };

			FDwEncounterSetup Setup;
			if (!StartFreshKrakenFight(Setup)) { return; }
			TestEqual(TEXT("Appear on the start, once"), Played, TArray<FGameplayTag>{ DwTags::GameplayCue_Kraken_Appear });

			Played.Reset();
			F.ApplyEffect(DwGas::GetSharedEffects().Kill, Tentacle(0), Captain());
			TestEqual(TEXT("A cut tentacle plays TentacleCut, not Unit.Death"), Played, TArray<FGameplayTag>{ DwTags::GameplayCue_Kraken_TentacleCut });

			Played.Reset();
			F.Combat->ReportMiss(Body(), FIntPoint(0, 0), DwTags::Ability_Kraken_TentacleSlam);
			TestEqual(TEXT("A miss splashes"), Played, TArray<FGameplayTag>{ DwTags::GameplayCue_Attack_Miss });

			Played.Reset();
			F.ApplyEffect(DwGas::GetSharedEffects().Kill, Body(), Captain());
			TestEqual(TEXT("The body plays Kraken.Death, not Unit.Death"), Played, TArray<FGameplayTag>{ DwTags::GameplayCue_Kraken_Death });
		});

		It("instant (simulator, tests): none of them play", [this]()
		{
			UDwPresentationSubsystem* Presentation = F.World->GetSubsystem<UDwPresentationSubsystem>();
			const int32 Before = Presentation->GetCueSoundsPlayed();
			FDwEncounterSetup Setup;
			if (!StartFreshKrakenFight(Setup)) { return; }
			F.ApplyEffect(DwGas::GetSharedEffects().Kill, Tentacle(0), Captain());
			F.Combat->ReportMiss(Body(), FIntPoint(0, 0), DwTags::Ability_Kraken_TentacleSlam);
			F.ApplyEffect(DwGas::GetSharedEffects().Kill, Body(), Captain());
			TestEqual(TEXT("No cue sounds"), Presentation->GetCueSoundsPlayed(), Before);
			TestEqual(TEXT("No cue reached a notify"), Presentation->GetCuesPresented(), 0);
		});
	});

	Describe("Run", [this]()
	{
		It("puts a buildable Kraken fight on the boss node of every chart (seed sweep, all captains)", [this]()
		{
			if (!TestNotNull(TEXT("DA_GameData"), GameData) || !TestNotNull(TEXT("KrakenEncounter"), Kraken())) { return; }
			int32 Bad = 0;
			for (const UDwCaptainDefinition* CaptainDef : GameData->Captains)
			{
				for (int32 Seed = 1; Seed <= 100; ++Seed)
				{
					UDwRun* Sweep = NewObject<UDwRun>();
					TArray<FString> Errors;
					FDwEncounterSetup Setup;
					const bool bOk = Sweep->Initialize(*GameData, *CaptainDef, Seed, UDwRunSubsystem::GatherEncounterPool(), &Errors)
						&& Sweep->GetState().Map.Nodes.IsValidIndex(Sweep->GetState().Map.BossNode)
						&& Sweep->GetState().Map.Nodes[Sweep->GetState().Map.BossNode].Encounter == Kraken()
						&& FDwEncounterSetupBuilder::Build(*GameData, *Kraken(), FreshCrew(), Seed, Setup, Errors) && Setup.OutcomeConfig.bIsBossFight;
					if (!bOk && ++Bad <= 5)
					{
						AddError(FString::Printf(TEXT("%s seed %d: %s"), *CaptainDef->GetName(), Seed, *FString::Join(Errors, TEXT("; "))));
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
			TestEqual(TEXT("The Kraken at full HP"), F.Health(RunBody()), F.Attr(RunBody(), UDwUnitAttributeSet::GetMaxHealthAttribute()));
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
					F.ApplyEffect(DwGas::GetSharedEffects().Kill, Unit->GetHandle(), RunBody());
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
