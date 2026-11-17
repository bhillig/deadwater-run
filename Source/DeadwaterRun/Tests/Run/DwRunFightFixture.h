// Copyright Brandon Hillig 2026

#pragma once

// A real run (DA_GameData, the real captains and content) whose fights play in an FDwCombatFixture world, for the
// captain specs (DeadwaterRun.Run.Captains, DW-206..208). DeadwaterRun.Run.Content and .Kraken still carry their own
// copies of these helpers; moving them here is a test cleanup.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Combat/DwPassiveSideController.h"
#include "Misc/AutomationTest.h"
#include "Run/DwRun.h"
#include "Run/DwRunSubsystem.h"
#include "Tests/Combat/DwCombatTestFixture.h"
#include "Tests/Run/DwRunTestContent.h"

struct FDwRunFight
{
	FDwCombatFixture F;
	const UDwGameData* GameData = nullptr;
	UDwRun* Run = nullptr;
	FAutomationTestBase* Test = nullptr;

	void Create(FAutomationTestBase& InTest)
	{
		Test = &InTest;
		F.Create();
		GameData = UDwAssetManager::Get().GetGameData();
	}

	void Destroy()
	{
		F.Destroy();
		Run = nullptr;
	}

	/** A new run with the captain of that short name (TeamLeader, Fearmongerer, Conqueror). */
	bool StartRun(const TCHAR* CaptainName, int32 Seed = 7)
	{
		const UDwCaptainDefinition* Captain = UDwRunSubsystem::FindCaptain(CaptainName);
		if (!Test->TestNotNull(TEXT("DA_GameData"), GameData) || !Test->TestNotNull(TEXT("Captain"), Captain))
		{
			return false;
		}
		Run = F.Root(NewObject<UDwRun>());
		TArray<FString> Errors;
		const bool bOk = Run->Initialize(*GameData, *Captain, Seed, UDwRunSubsystem::GatherEncounterPool(), &Errors);
		return Test->TestTrue(FString::Printf(TEXT("Run initialized: %s"), *FString::Join(Errors, TEXT("; "))), bOk);
	}

	/** Sails to the next fight route (shops left, events passed) and starts its fight in the fixture world. */
	bool StartNextFight(FDwEncounterSetup* OutSetup = nullptr)
	{
		for (int32 Step = 0; Step < 20 && Run->GetPhase() != EDwRunPhase::Combat; ++Step)
		{
			const TArray<int32> Choices = Run->GetChoices();
			const int32* Fight = Choices.FindByPredicate([this](int32 Index) { return Run->GetState().Map.Nodes[Index].Type == EDwNodeType::Fight; });
			if (!Test->TestTrue(TEXT("A route"), !Choices.IsEmpty()) || !Test->TestTrue(TEXT("Sailed"), Run->ChooseNode(Fight ? *Fight : Choices[0])))
			{
				return false;
			}
			if (Run->GetPhase() == EDwRunPhase::Shop) { Run->LeaveShop(); }
			if (Run->GetPhase() == EDwRunPhase::Event) { Test->TestTrue(TEXT("Passed the event"), DwRunTest::PassEvent(*Run)); }
		}
		FDwEncounterSetup Setup;
		TArray<FString> Errors;
		if (!Test->TestTrue(FString::Printf(TEXT("Setup: %s"), *FString::Join(Errors, TEXT("; "))), Run->BuildEncounterSetup(Setup, Errors)))
		{
			return false;
		}
		if (OutSetup)
		{
			*OutSetup = Setup;
		}
		return Test->TestTrue(TEXT("Fight started"), F.Combat->StartEncounter(Setup));
	}

	/** The fight's result to the run; then claims the rewards and level-ups back to the map (victories only). */
	bool FinishFight()
	{
		const FDwCombatResult Result = F.Combat->GetResult();
		F.Combat->EndEncounter();
		if (!Test->TestTrue(TEXT("Applied"), Run->ApplyCombatResult(Result)))
		{
			return false;
		}
		if (Run->IsOver())
		{
			return true;
		}
		Run->ClaimRewards(INDEX_NONE, FGuid());
		for (int32 Guard = 0; Guard < 20 && Run->GetPhase() == EDwRunPhase::LevelUp; ++Guard)
		{
			Run->ChoosePerk(Run->GetCurrentLevelUp()->PerkChoices.IsEmpty() ? INDEX_NONE : 0);
		}
		return Test->TestEqual(TEXT("Map"), Run->GetPhase(), EDwRunPhase::Map);
	}

	FDwUnitHandle CaptainUnit() const { return FindCrewUnit(Run->GetState().Crew[0].Id); }

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

	TArray<FDwUnitHandle> Units(EDwSide Side, bool bAliveOnly = true) const
	{
		TArray<FDwUnitHandle> Out;
		for (const ADwCombatUnit* Unit : F.Combat->GetUnits())
		{
			if (Unit->GetSide() == Side && (!bAliveOnly || Unit->IsAlive()))
			{
				Out.Add(Unit->GetHandle());
			}
		}
		return Out;
	}

	/** The captain's ability with this tag (one of the captain's two). */
	const UDwAbilityDefinition* CaptainAbility(const FGameplayTag& Tag) const
	{
		for (const UDwAbilityDefinition* Ability : Run->GetState().Captain->CaptainAbilities)
		{
			if (Ability && Ability->AbilityTag == Tag)
			{
				return Ability;
			}
		}
		return nullptr;
	}

	/** Submits an ability of Source at a unit (or, for group patterns, any unit: the pattern decides). */
	EDwCommandResult Use(FDwUnitHandle Source, const UDwAbilityDefinition* Ability, FDwUnitHandle Target)
	{
		const ADwCombatUnit* TargetUnit = F.Unit(Target);
		return F.Combat->SubmitCommand(FDwCombatCommand::MakeAbility(Source, Ability->AbilityTag,
			FDwAbilityTarget::MakeUnit(Target, TargetUnit ? TargetUnit->GetTile() : FIntPoint::ZeroValue)));
	}

	/** The enemy side does nothing on its turns (tests about the player's moves only). Call after StartNextFight. */
	void MakeEnemiesPassive()
	{
		F.Combat->SetSideController(EDwSide::Enemy, F.Root(NewObject<UDwPassiveSideController>(F.Combat)));
	}

	/** Ends the player's turn and lets the enemy side play out (AI or passive) until it's the player's turn again. */
	void PassRound()
	{
		F.EndTurn(EDwSide::Player);
		for (int32 Guard = 0; Guard < 50 && F.Combat->GetOutcome() == EDwCombatOutcome::None && F.Combat->GetActiveSide() != EDwSide::Player; ++Guard)
		{
			F.EndTurn(EDwSide::Enemy);
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
