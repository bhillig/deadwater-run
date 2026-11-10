// Copyright Brandon Hillig 2026

// DW-201: the real captains (Tools/CreateCombatContent.ps1) start runs, and a run's fights are played through the real
// combat subsystem: Health and permadeath carry over, Mana always starts full (C9).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/DwGameplayAbility.h"
#include "Abilities/DwGameplayEffects.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Data/DwCaptainDefinition.h"
#include "Misc/DataValidation.h"
#include "Run/DwRun.h"
#include "Run/DwRunRandom.h"
#include "Run/DwRunSubsystem.h"
#include "Tests/Combat/DwCombatTestFixture.h"

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
		const TArray<int32> Choices = Run->GetChoices();
		if (!TestTrue(TEXT("A route"), !Choices.IsEmpty()) || !TestTrue(TEXT("Sailed"), Run->ChooseNode(Choices[0])))
		{
			return false;
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
		TestTrue(TEXT("Rule tags"), Setup.CaptainRuleTags.HasTagExact(DwTags::Rule_Captain_DreadReputation));
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
		TestEqual(TEXT("Back on the map"), S.Phase, EDwRunPhase::Map);
		TestEqual(TEXT("Captain kept 60 HP"), S.FindCrew(CaptainId)->Health, 60.f);
		TestFalse(TEXT("Sniper lost"), S.FindCrew(SniperId)->bAlive);
		TestEqual(TEXT("One cleared"), S.Stats.NodesCleared, 1);

		// Fight 2: two crew; the captain starts at 60 HP and full Mana.
		FDwEncounterSetup Setup;
		if (!StartNextFight(&Setup)) { return; }
		TestEqual(TEXT("Only the living sail on"), Setup.Crew.Num(), 2);
		TestEqual(TEXT("Five units"), F.Combat->GetUnits().Num(), 5);
		TestFalse(TEXT("No sniper aboard"), FindCrewUnit(SniperId).IsValid());
		const FDwUnitHandle Again = FindCrewUnit(CaptainId);
		if (!TestTrue(TEXT("Captain aboard"), Again.IsValid())) { return; }
		TestEqual(TEXT("Health carried"), F.Health(Again), 60.f);
		TestEqual(TEXT("Mana full"), F.Attr(Again, UDwUnitAttributeSet::GetManaAttribute()), F.Attr(Again, UDwUnitAttributeSet::GetMaxManaAttribute()));
		TestTrue(TEXT("Max Mana is 20"), F.Attr(Again, UDwUnitAttributeSet::GetMaxManaAttribute()) == 20.f);
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
