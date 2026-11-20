// Copyright Brandon Hillig 2026

#include "DwEnemyCrewContentLibrary.h"

#include "AI/DwAiProfile.h"
#include "Board/DwDeckLayout.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "DwContentAssetUtils.h"
#include "Misc/DataValidation.h"

namespace DwEnemyCrewContent
{
	const FString UnitData = TEXT("/Game/DeadwaterRun/Data/Units");
	const FString EncounterData = TEXT("/Game/DeadwaterRun/Data/Encounters");
	const FString Boards = TEXT("/Game/DeadwaterRun/Data/Boards");
	const FString AiData = TEXT("/Game/DeadwaterRun/Data/AI");
	const FString Tuning = TEXT("/Game/DeadwaterRun/Data/Tuning");

	/**
	 * Map tiers (Open-Questions N3), written 1-based in the design (layers 1-3 / 4-6 / 7-9); map layers are 0-based.
	 * The top tier is open-ended so a longer chart never leaves a fight node without a crew.
	 */
	constexpr int32 Tier1Min = 0, Tier1Max = 2;
	constexpr int32 Tier2Min = 3, Tier2Max = 5;
	constexpr int32 Tier3Min = 6, Tier3Max = 99;

	struct FCrewSpec
	{
		const TCHAR* Id;
		const TCHAR* DisplayName;
		const TCHAR* Description;
		int32 MinLayer;
		int32 MaxLayer;
		int32 Level;
		/** One letter per enemy: W = Swordsman, N = Sniper. */
		const TCHAR* Roster;
		bool bBoarders;
		bool bPreferCannons;
	};

	bool Validate(const UObject* Asset, TArray<FString>& Log)
	{
		FDataValidationContext Context;
		if (Asset->IsDataValid(Context) != EDataValidationResult::Invalid)
		{
			return true;
		}
		for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
		{
			Log.Add(FString::Printf(TEXT("ERROR %s: %s"), *Asset->GetName(), *Issue.Message.ToString()));
		}
		return false;
	}

	/** N2 (Brandon, 2026-11-19). Levels are DW-401 placeholders: 1 / 2 / 3 by tier, the Dread Crew a level above its peers. */
	const FCrewSpec Crews[] = {
		{ TEXT("Deckhands"), TEXT("Deckhands"),
			TEXT("Three green cutlass hands who swarm your deck the moment the ships touch."),
			Tier1Min, Tier1Max, 1, TEXT("WWW"), true, false },
		{ TEXT("PowderMonkeys"), TEXT("Powder Monkeys"),
			TEXT("They stay on their own deck and work the guns, trusting the cannons to do the talking."),
			Tier1Min, Tier2Max, 1, TEXT("WWN"), false, true },
		{ TEXT("Sharpshooters"), TEXT("Sharpshooters"),
			TEXT("Three muskets that keep their distance and climb for the nest."),
			Tier2Min, Tier2Max, 2, TEXT("NNN"), false, false },
		{ TEXT("Cutthroats"), TEXT("Cutthroats"),
			TEXT("They want your captain's head and will cut through anyone to get it."),
			Tier2Min, Tier3Max, 2, TEXT("WWN"), true, false },
		{ TEXT("DreadCrew"), TEXT("Dread Crew"),
			TEXT("Seasoned killers: blades, muskets and guns, all of it done well."),
			Tier3Min, Tier3Max, 3, TEXT("WWNN"), true, true },
	};
}

TArray<FString> UDwEnemyCrewContentLibrary::CrewIds()
{
	TArray<FString> Ids;
	for (const DwEnemyCrewContent::FCrewSpec& Spec : DwEnemyCrewContent::Crews)
	{
		Ids.Add(Spec.Id);
	}
	return Ids;
}

TArray<FString> UDwEnemyCrewContentLibrary::CreateEnemyCrewContent()
{
	using namespace DwContentAssets;
	using namespace DwEnemyCrewContent;
	TArray<FString> Log;
	bool bOk = true;
	auto Fail = [&Log]() { Log.Add(TEXT("CONTENT FAIL")); return Log; };

	// 1. What every crew shares: N2's one enemy deck and ship row, the Swordsman and Sniper, the tuning tables.
	UDwDeckLayout* EnemyDeck = Load<UDwDeckLayout>(Boards, TEXT("DA_Deck_EnemyPlaceholder"));
	UDwUnitDefinition* Swordsman = Load<UDwUnitDefinition>(UnitData, TEXT("DA_Unit_Swordsman"));
	UDwUnitDefinition* Sniper = Load<UDwUnitDefinition>(UnitData, TEXT("DA_Unit_Sniper"));
	UDwEncounterDefinition* Placeholder = Load<UDwEncounterDefinition>(EncounterData, TEXT("DA_Encounter_Placeholder"));
	UDataTable* AiTable = Load<UDataTable>(Tuning, TEXT("DT_AiTuning"));
	UDataTable* EncounterTable = Load<UDataTable>(Tuning, TEXT("DT_EncounterTuning"));
	if (!EnemyDeck || !Swordsman || !Sniper || !Placeholder || !AiTable || !EncounterTable)
	{
		Log.Add(TEXT("ERROR the enemy deck, Swordsman, Sniper, DA_Encounter_Placeholder, DT_AiTuning or DT_EncounterTuning is missing (run the combat and AI content first)"));
		return Fail();
	}

	for (const FCrewSpec& Spec : Crews)
	{
		const FString Id = Spec.Id;

		// 2. The personality: switches here, every number in DT_AiTuning row <Id>.
		UDwAiProfile* Profile = FindOrCreate<UDwAiProfile>(AiData, TEXT("DA_AiProfile_") + Id, Log);
		Profile->DisplayName = FText::FromString(Spec.DisplayName);
		Profile->Description = FText::FromString(Spec.Description);
		Profile->BrainClass = nullptr; // UDwCrewAiBrain
		Profile->bBoarders = Spec.bBoarders;
		Profile->bPreferCannons = Spec.bPreferCannons;
		Profile->Tuning = Row(AiTable, Spec.Id, Log, bOk);
		bOk &= Validate(Profile, Log) && Save(Profile, Log);

		// 3. The crew. The ship class starts as the placeholder's; the art step gives every crew the enemy sloop.
		UDwEncounterDefinition* Encounter = FindOrCreate<UDwEncounterDefinition>(EncounterData, TEXT("DA_Encounter_") + Id, Log);
		Encounter->DisplayName = FText::FromString(Spec.DisplayName);
		Encounter->Description = FText::FromString(Spec.Description);
		Encounter->Kind = EDwEncounterKind::ShipBattle;
		Encounter->MinLayer = Spec.MinLayer;
		Encounter->MaxLayer = Spec.MaxLayer;
		Encounter->Weight = 1.f;
		Encounter->ShipDeck = EnemyDeck;
		if (Encounter->ShipClass.IsNull())
		{
			Encounter->ShipClass = Placeholder->ShipClass;
		}
		Encounter->ShipTuning = Placeholder->ShipTuning;
		Encounter->Tuning = Row(EncounterTable, Spec.Id, Log, bOk);
		Encounter->AiProfile = Profile;
		Encounter->Kraken = nullptr;

		// Keep the art step's per-slot looks when the roster is unchanged; reset them when it changes.
		TArray<FDwEnemySlot> Units;
		for (const TCHAR* Letter = Spec.Roster; *Letter; ++Letter)
		{
			FDwEnemySlot& Slot = Units.AddDefaulted_GetRef();
			Slot.UnitDefinition = *Letter == TEXT('N') ? Sniper : Swordsman;
			Slot.Level = Spec.Level;
			const int32 Index = Units.Num() - 1;
			if (Encounter->Units.IsValidIndex(Index) && Encounter->Units[Index].UnitDefinition == Slot.UnitDefinition)
			{
				Slot.UnitClassOverride = Encounter->Units[Index].UnitClassOverride;
			}
		}
		Encounter->Units = MoveTemp(Units);
		bOk &= Validate(Encounter, Log) && Save(Encounter, Log);
		Log.Add(FString::Printf(TEXT("%s: layers %d-%d, %d units at level %d, profile %s"), *Encounter->GetName(), Spec.MinLayer,
			Spec.MaxLayer, Encounter->Units.Num(), Spec.Level, *Profile->GetName()));
	}

	Log.Add(bOk ? TEXT("CONTENT PASS") : TEXT("CONTENT FAIL"));
	return Log;
}
