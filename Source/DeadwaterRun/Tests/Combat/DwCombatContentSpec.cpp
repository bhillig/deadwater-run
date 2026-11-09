// Copyright Brandon Hillig 2026

// DW-103/DW-104: the real placeholder content (Tools/CreateCombatContent.ps1) plays a fight through the same code paths.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Board/DwBoard.h"
#include "Board/DwGridQueries.h"
#include "Combat/DwCombatRules.h"
#include "Combat/DwEncounterSetupBuilder.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Data/DwEncounterDefinition.h"
#include "Misc/DataValidation.h"
#include "Tests/Combat/DwCombatTestFixture.h"

namespace DwCombatContentSpec
{
	template <typename T>
	T* Load(const TCHAR* Path)
	{
		return LoadObject<T>(nullptr, Path);
	}

	const TCHAR* Encounter = TEXT("/Game/DeadwaterRun/Data/Encounters/DA_Encounter_Placeholder.DA_Encounter_Placeholder");
	const TCHAR* Captain = TEXT("/Game/DeadwaterRun/Data/Units/DA_Unit_Captain_TeamLeader.DA_Unit_Captain_TeamLeader");
	const TCHAR* Swordsman = TEXT("/Game/DeadwaterRun/Data/Units/DA_Unit_Swordsman.DA_Unit_Swordsman");
	const TCHAR* Sniper = TEXT("/Game/DeadwaterRun/Data/Units/DA_Unit_Sniper.DA_Unit_Sniper");
	const TCHAR* Grog = TEXT("/Game/DeadwaterRun/Data/Items/DA_Item_HealthRestore.DA_Item_HealthRestore");
	const TCHAR* Salts = TEXT("/Game/DeadwaterRun/Data/Items/DA_Item_ManaRestore.DA_Item_ManaRestore");
}

BEGIN_DEFINE_SPEC(FDwCombatContentSpec, "DeadwaterRun.Combat.Content",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	FDwCombatFixture F; // world only; the fight comes from real content
	const UDwGameData* GameData = nullptr;
	UDwItemDefinition* Grog = nullptr;

	bool StartPlaceholderFight()
	{
		const UDwEncounterDefinition* EncounterDef = DwCombatContentSpec::Load<UDwEncounterDefinition>(DwCombatContentSpec::Encounter);
		if (!TestNotNull(TEXT("DA_GameData"), GameData) || !TestNotNull(TEXT("DA_Encounter_Placeholder"), EncounterDef))
		{
			return false;
		}
		TArray<FDwCombatantSetup> Crew;
		for (const TCHAR* Path : { DwCombatContentSpec::Captain, DwCombatContentSpec::Swordsman, DwCombatContentSpec::Sniper })
		{
			FDwCombatantSetup& Member = Crew.AddDefaulted_GetRef();
			Member.Definition = DwCombatContentSpec::Load<UDwUnitDefinition>(Path);
			Member.RosterId = FGuid::NewGuid();
		}
		Crew[0].bIsCaptain = true;
		FDwEncounterSetup Setup;
		TArray<FString> Errors;
		if (!TestTrue(FString::Printf(TEXT("Setup built: %s"), *FString::Join(Errors, TEXT("; "))),
			FDwEncounterSetupBuilder::BuildShipBattle(*GameData, *EncounterDef, Crew, 42, Setup, Errors)))
		{
			return false;
		}
		FDwItemStack& Stack = Setup.Items.AddDefaulted_GetRef();
		Stack.Item = Grog;
		Stack.Count = 1;
		return TestTrue(TEXT("Started"), F.Combat->StartEncounter(Setup));
	}

	/** Puts Unit on a free tile next to Target (no move spent). */
	bool PlaceNextTo(FDwUnitHandle Unit, FDwUnitHandle Target)
	{
		const TOptional<FIntPoint> Tile = FDwTargetingQuery::FreeTileAdjacentTo(F.Combat->GetGrid(), F.Unit(Target)->GetTile(), F.Unit(Unit)->GetTile());
		if (!TestTrue(TEXT("Free tile next to the target"), Tile.IsSet()))
		{
			return false;
		}
		F.Combat->MoveUnit(Unit, Tile.GetValue());
		return true;
	}

END_DEFINE_SPEC(FDwCombatContentSpec)

void FDwCombatContentSpec::Define()
{
	BeforeEach([this]()
	{
		F.Create();
		GameData = UDwAssetManager::Get().GetGameData();
		Grog = DwCombatContentSpec::Load<UDwItemDefinition>(DwCombatContentSpec::Grog);
	});
	AfterEach([this]() { F.Destroy(); });

#if WITH_EDITOR
	It("passes data validation for every placeholder definition", [this]()
	{
		const TCHAR* Paths[] = {
			DwCombatContentSpec::Encounter, DwCombatContentSpec::Captain, DwCombatContentSpec::Swordsman, DwCombatContentSpec::Sniper,
			DwCombatContentSpec::Grog, DwCombatContentSpec::Salts,
			TEXT("/Game/DeadwaterRun/Data/Abilities/DA_Ability_Move.DA_Ability_Move"),
			TEXT("/Game/DeadwaterRun/Data/Abilities/DA_Ability_AttackSword.DA_Ability_AttackSword"),
			TEXT("/Game/DeadwaterRun/Data/Abilities/DA_Ability_AttackRifle.DA_Ability_AttackRifle"),
			TEXT("/Game/DeadwaterRun/Data/Abilities/DA_Ability_FireCannon.DA_Ability_FireCannon"),
			TEXT("/Game/DeadwaterRun/Data/Abilities/DA_Ability_UseItem.DA_Ability_UseItem") };
		for (const TCHAR* Path : Paths)
		{
			const UDwDefinitionBase* Definition = DwCombatContentSpec::Load<UDwDefinitionBase>(Path);
			if (!TestNotNull(Path, Definition))
			{
				continue;
			}
			FDataValidationContext Context;
			TestEqual(*FString::Printf(TEXT("%s is valid"), *Definition->GetName()), Definition->IsDataValid(Context), EDataValidationResult::Valid);
		}
	});
#endif

	It("gives every unit Move, Cannon and Item through DA_GameData", [this]()
	{
		if (!StartPlaceholderFight()) { return; }
		const FDwCombatSnapshot Snap = F.Combat->GetSnapshot();
		for (const FDwUnitSnapshot& Unit : Snap.Units)
		{
			TestTrue(TEXT("Move"), Unit.GrantedAbilityTags.HasTagExact(DwTags::Ability_Move));
			TestTrue(TEXT("Cannon"), Unit.GrantedAbilityTags.HasTagExact(DwTags::Ability_Cannon));
			TestTrue(TEXT("Item"), Unit.GrantedAbilityTags.HasTagExact(DwTags::Ability_Item));
			TestTrue(TEXT("A basic attack"), Unit.GrantedAbilityTags.HasTag(DwTags::Ability_Attack));
		}
	});

	It("spawns the placeholder 3 v 3 from DA_Encounter_Placeholder on two 250 HP ships", [this]()
	{
		if (!StartPlaceholderFight()) { return; }
		TestEqual(TEXT("Six units"), F.Combat->GetUnits().Num(), 6);
		TestEqual(TEXT("Player ship"), F.ShipHealth(EDwSide::Player), 250.f);
		TestEqual(TEXT("Enemy ship"), F.ShipHealth(EDwSide::Enemy), 250.f);
		TestEqual(TEXT("Two player cannons"), F.Combat->GetShip(EDwSide::Player)->GetCannonTiles().Num(), 2);
		TestTrue(TEXT("Enemy cannons"), F.Combat->GetShip(EDwSide::Enemy)->GetCannonTiles().Num() > 0);
		for (const ADwCombatUnit* Unit : F.Combat->GetUnits())
		{
			TestEqual(TEXT("100 HP"), Unit->GetAttributes()->GetHealth(), 100.f);
			TestEqual(TEXT("20 MP"), Unit->GetAttributes()->GetMana(), 20.f);
			TestTrue(TEXT("On its own deck"), F.Combat->GetGrid().GetTile(Unit->GetTile())->Owner == Unit->GetSide());
		}
		TestTrue(TEXT("Captain flagged"), F.Unit(F.CrewHandle(0))->IsCaptain());
	});

	It("cuts an adjacent enemy for 25 with the cutlass, exactly as previewed", [this]()
	{
		if (!StartPlaceholderFight()) { return; }
		const FDwUnitHandle Swordsman = F.CrewHandle(1);
		const FDwUnitHandle Target = F.EnemyHandle(0);
		if (!PlaceNextTo(Swordsman, Target)) { return; }
		const FDwCombatSnapshot Snap = F.Combat->GetSnapshot();
		const UDwAbilityDefinition* Cutlass = FDwCombatQueries::FindAbility(*Snap.FindUnit(Swordsman), DwTags::Ability_Attack_Melee);
		const FDwAbilityPreview Preview = FDwCombatQueries::GetAbilityCDO(Cutlass)->PreviewOutcome(
			F.Context(Snap, Swordsman, Cutlass), FDwAbilityTarget::MakeUnit(Target, F.Unit(Target)->GetTile()));
		TestEqual(TEXT("Cutlass"), F.Combat->SubmitCommand(FDwCombatCommand::MakeAbility(Swordsman, DwTags::Ability_Attack_Melee,
			FDwAbilityTarget::MakeUnit(Target, F.Unit(Target)->GetTile()))), EDwCommandResult::Accepted);
		TestEqual(TEXT("100 - 25"), F.Health(Target), 75.f);
		TestTrue(TEXT("Preview == result"), Preview.bValid && Preview.Targets.Num() == 1 && Preview.Targets[0].Damage == 25);
	});

	It("sinks the enemy ship in four cannon turns", [this]()
	{
		if (!StartPlaceholderFight()) { return; }
		const FDwUnitHandle Gunner = F.CrewHandle(2);
		F.Combat->MoveUnit(Gunner, F.Combat->GetShip(EDwSide::Player)->GetCannonTiles()[0]);
		for (int32 Turn = 0; Turn < 4; ++Turn)
		{
			TestEqual(TEXT("Fire"), F.Combat->SubmitCommand(FDwCombatCommand::MakeAbility(Gunner, DwTags::Ability_Cannon,
				FDwAbilityTarget::MakeShip(EDwSide::Enemy))), EDwCommandResult::Accepted);
			if (F.Combat->GetOutcome() == EDwCombatOutcome::None)
			{
				F.EndTurn();
			}
		}
		TestEqual(TEXT("4 x 70 >= 250"), F.Combat->GetOutcome(), EDwCombatOutcome::VictorySunk);
	});

	It("heals with grog", [this]()
	{
		if (!StartPlaceholderFight()) { return; }
		F.Asc(F.CrewHandle(0))->SetNumericAttributeBase(UDwUnitAttributeSet::GetHealthAttribute(), 50.f);
		TestEqual(TEXT("Grog on self"), F.UseItem(F.CrewHandle(0), Grog, F.CrewHandle(0)), EDwCommandResult::Accepted);
		TestEqual(TEXT("50 + 30"), F.Health(F.CrewHandle(0)), 80.f);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
