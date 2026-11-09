// Copyright Brandon Hillig 2026

// DW-004: native tags and shared types.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/DwGameplayTags.h"
#include "Core/DwTypes.h"
#include "GameplayTagsManager.h"

BEGIN_DEFINE_SPEC(FDwCoreSpec, "DeadwaterRun.Core",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FDwCoreSpec)

void FDwCoreSpec::Define()
{
	Describe("Tags", [this]()
	{
		It("registers every native tag under its dotted name", [this]()
		{
			// Spot-check one tag per namespace; a typo in DwGameplayTags.cpp fails here.
			const TPair<FGameplayTag, const TCHAR*> Expected[] = {
				{ DwTags::Ability_Attack_Melee, TEXT("Ability.Attack.Melee") },
				{ DwTags::Ability_Weapon_Firearm, TEXT("Ability.Weapon.Firearm") },
				{ DwTags::Status_Incapacitated_Frozen, TEXT("Status.Incapacitated.Frozen") },
				{ DwTags::State_Dead, TEXT("State.Dead") },
				{ DwTags::Effect_Debuff, TEXT("Effect.Debuff") },
				{ DwTags::Unit_Kraken_Tentacle, TEXT("Unit.Kraken.Tentacle") },
				{ DwTags::Rule_Turn_SingleChoice, TEXT("Rule.Turn.SingleChoice") },
				{ DwTags::Tile_BoardingEdge, TEXT("Tile.BoardingEdge") },
				{ DwTags::Event_Presentation_Impact, TEXT("Event.Presentation.Impact") },
				{ DwTags::Data_DamageScale, TEXT("Data.DamageScale") },
				{ DwTags::GameplayCue_Cannon_Impact, TEXT("GameplayCue.Cannon.Impact") },
				{ DwTags::Music_Boss_Enraged, TEXT("Music.Boss.Enraged") },
				{ DwTags::UI_Layer_Modal, TEXT("UI.Layer.Modal") },
			};
			for (const TPair<FGameplayTag, const TCHAR*>& Pair : Expected)
			{
				TestTrue(FString::Printf(TEXT("%s is valid"), Pair.Value), Pair.Key.IsValid());
				TestEqual(TEXT("Tag name"), Pair.Key.ToString(), FString(Pair.Value));
				TestEqual(TEXT("Requestable by name"),
					UGameplayTagsManager::Get().RequestGameplayTag(FName(Pair.Value), /*ErrorIfNotFound*/ false), Pair.Key);
			}
		});

		It("keeps the hierarchy blocking abilities rely on", [this]()
		{
			// ActivationBlockedTags = Status.Incapacitated must block both cowering and frozen units (doc 02).
			TestTrue(TEXT("Frozen is incapacitated"), DwTags::Status_Incapacitated_Frozen.GetTag().MatchesTag(DwTags::Status_Incapacitated));
			TestTrue(TEXT("Cowering is incapacitated"), DwTags::Status_Incapacitated_Cowering.GetTag().MatchesTag(DwTags::Status_Incapacitated));
			TestFalse(TEXT("Rooted is not incapacitated"), DwTags::Status_Rooted.GetTag().MatchesTag(DwTags::Status_Incapacitated));
			TestTrue(TEXT("Melee is an attack"), DwTags::Ability_Attack_Melee.GetTag().MatchesTag(DwTags::Ability_Attack));
		});
	});

	Describe("UnitHandle", [this]()
	{
		It("is invalid by default and compares by id", [this]()
		{
			TestFalse(TEXT("Default invalid"), FDwUnitHandle().IsValid());
			TestTrue(TEXT("Id 0 valid"), FDwUnitHandle(0).IsValid());
			TestTrue(TEXT("Equal ids"), FDwUnitHandle(3) == FDwUnitHandle(3));
			TestTrue(TEXT("Different ids"), FDwUnitHandle(3) != FDwUnitHandle(4));

			TSet<FDwUnitHandle> Set;
			Set.Add(FDwUnitHandle(7));
			Set.Add(FDwUnitHandle(7));
			TestEqual(TEXT("Hashable, no duplicates"), Set.Num(), 1);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
