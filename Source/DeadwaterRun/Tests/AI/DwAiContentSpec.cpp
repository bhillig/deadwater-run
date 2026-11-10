// Copyright Brandon Hillig 2026

// DW-106: the shipped AI data (DT_AiTuning, DA_AiProfile_Default, DA_Encounter_Placeholder) drives the dev fight's
// enemy side in L_Sea through the real setup builder.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AI/DwAiProfile.h"
#include "Combat/DwEncounterSetupBuilder.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Data/DwEncounterDefinition.h"
#include "Misc/DataValidation.h"
#include "Tests/Combat/DwCombatTestFixture.h"

BEGIN_DEFINE_SPEC(FDwAiContentSpec, "DeadwaterRun.AI.Content",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	FDwCombatFixture F; // world only; the fight comes from real content
	const UDwGameData* GameData = nullptr;

END_DEFINE_SPEC(FDwAiContentSpec)

void FDwAiContentSpec::Define()
{
	BeforeEach([this]()
	{
		F.Create();
		GameData = UDwAssetManager::Get().GetGameData();
	});
	AfterEach([this]() { F.Destroy(); });

	It("ships DT_AiTuning and DA_AiProfile_Default, linked from DA_GameData and the placeholder crew", [this]()
	{
		if (!TestNotNull(TEXT("DA_GameData"), GameData)) { return; }
		const UDataTable* Table = GameData->TuningTables.Ai;
		if (!TestNotNull(TEXT("DA_GameData.TuningTables.Ai"), Table)) { return; }
		TestTrue(TEXT("Row struct is FDwAiTuningRow"), Table->GetRowStruct() == FDwAiTuningRow::StaticStruct());
		const FDwAiTuningRow* Row = Table->FindRow<FDwAiTuningRow>(TEXT("Default"), TEXT("AI content test"));
		if (!TestNotNull(TEXT("Default row"), Row)) { return; }
		TestTrue(TEXT("Imported from the JSON (non-zero weights)"), Row->Weights.Damage > 0.f && Row->Weights.Kill > 0.f);

		const UDwEncounterDefinition* Encounter = GameData->DevEncounter;
		if (!TestNotNull(TEXT("Dev encounter"), Encounter)) { return; }
		const UDwAiProfile* Profile = Encounter->AiProfile;
		if (!TestNotNull(TEXT("The placeholder crew has an AI profile"), Profile)) { return; }
		TestEqual(TEXT("DA_AiProfile_Default"), Profile->GetName(), FString(TEXT("DA_AiProfile_Default")));
		TestEqual(TEXT("Its tuning row is the Default row"), Profile->GetTuningValues().Weights.Damage, Row->Weights.Damage);
#if WITH_EDITOR
		FDataValidationContext Context;
		TestEqual(TEXT("Profile passes data validation"), Profile->IsDataValid(Context), EDataValidationResult::Valid);
#endif
	});

	It("puts the placeholder crew under the AI and its enemies act on their turn", [this]()
	{
		if (!TestNotNull(TEXT("DA_GameData"), GameData) || !TestNotNull(TEXT("Dev encounter"), GameData->DevEncounter.Get())) { return; }
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
		if (!TestTrue(TEXT("Setup built"), FDwEncounterSetupBuilder::BuildShipBattle(*GameData, *GameData->DevEncounter, Crew, 7, Setup, Errors))) { return; }
		for (const FDwCombatantSetup& Enemy : Setup.Enemies)
		{
			TestTrue(TEXT("Every enemy carries the encounter's profile"), Enemy.AiProfile == GameData->DevEncounter->AiProfile);
		}
		if (!TestTrue(TEXT("Started"), F.Combat->StartEncounter(Setup))) { return; }

		// Two full rounds with the player doing nothing: the enemies close in and fight.
		for (int32 Round = 0; Round < 2 && F.Combat->GetOutcome() == EDwCombatOutcome::None; ++Round)
		{
			F.EndTurn();
		}
		const int32 PlayerCrew = Setup.Crew.Num();
		const TArray<FDwCombatCommand> EnemyActions = F.Combat->GetRecord().Commands.FilterByPredicate([PlayerCrew](const FDwCombatCommand& C)
		{
			return (C.Type == EDwCommandType::Move || C.Type == EDwCommandType::Ability) && C.Unit.Id > PlayerCrew;
		});
		TArray<FString> Lines;
		for (const FDwCombatCommand& C : EnemyActions) { Lines.Add(C.ToString()); }
		AddInfo(FString::Printf(TEXT("Enemy commands: %s"), *FString::Join(Lines, TEXT(" | "))));
		TestTrue(TEXT("Enemies moved"), EnemyActions.ContainsByPredicate([](const FDwCombatCommand& C) { return C.Type == EDwCommandType::Move; }));
		TestTrue(TEXT("Enemies attacked or fired"), EnemyActions.ContainsByPredicate([](const FDwCombatCommand& C) { return C.Type == EDwCommandType::Ability; }));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
