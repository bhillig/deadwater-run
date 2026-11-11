// Copyright Brandon Hillig 2026

#include "DwKrakenContentLibrary.h"

#include "AI/DwAiProfile.h"
#include "Abilities/DwGA_EffectPayload.h"
#include "Abilities/DwGameplayEffects.h"
#include "Board/DwBoardBuilder.h"
#include "Board/DwDeckLayout.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwKrakenDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "DwCombatContentLibrary.h"
#include "DwContentAssetUtils.h"
#include "Misc/DataValidation.h"

namespace DwKrakenContent
{
	const FString Data = TEXT("/Game/DeadwaterRun/Data");
	const FString Abilities = TEXT("/Game/DeadwaterRun/Abilities");
	const FString AbilityData = TEXT("/Game/DeadwaterRun/Data/Abilities");
	const FString UnitData = TEXT("/Game/DeadwaterRun/Data/Units");
	const FString KrakenData = TEXT("/Game/DeadwaterRun/Data/Kraken");
	const FString EncounterData = TEXT("/Game/DeadwaterRun/Data/Encounters");
	const FString Boards = TEXT("/Game/DeadwaterRun/Data/Boards");
	const FString AiData = TEXT("/Game/DeadwaterRun/Data/AI");
	const FString Tuning = TEXT("/Game/DeadwaterRun/Data/Tuning");

	/** Logs every data validation error of Asset as an ERROR line. */
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
}

TArray<FString> UDwKrakenContentLibrary::PlaceholderArenaRows()
{
	// PLACEHOLDER (DW-209): open water with the body right beside the rail, so cutlasses, muskets and every captain can
	// reach it while there are no tentacles to cut (what may hit the body is Open-Questions B4; DW-301 lays out the
	// real arena with tentacle slots).
	return { TEXT("~~~~"), TEXT("~~~~"), TEXT("~~~B"), TEXT("~~~~") };
}

TArray<FString> UDwKrakenContentLibrary::CreateKrakenContent()
{
	using namespace DwContentAssets;
	using namespace DwKrakenContent;
	TArray<FString> Log;
	bool bOk = true;
	auto Fail = [&Log]() { Log.Add(TEXT("CONTENT FAIL")); return Log; };

	// 1. Tuning: DT_KrakenTuning from its JSON. Unit, ability and encounter rows were synced by the earlier steps.
	bOk &= UDwCombatContentLibrary::SyncTuningTable(TEXT("Kraken"), Log);
	UDataTable* AbilityTable = Load<UDataTable>(Tuning, TEXT("DT_AbilityTuning"));
	UDataTable* UnitTable = Load<UDataTable>(Tuning, TEXT("DT_UnitTuning"));
	UDataTable* EncounterTable = Load<UDataTable>(Tuning, TEXT("DT_EncounterTuning"));
	UDataTable* KrakenTable = Load<UDataTable>(Tuning, TEXT("DT_KrakenTuning"));
	UDwGameData* GameData = Load<UDwGameData>(Data, UDwGameData::AssetName.ToString());
	UDwAiProfile* AiProfile = Load<UDwAiProfile>(AiData, TEXT("DA_AiProfile_Default"));
	if (!GameData || !AiProfile || !KrakenTable)
	{
		Log.Add(TEXT("ERROR DA_GameData, DA_AiProfile_Default or DT_KrakenTuning is missing (run the combat and AI content first)"));
		return Fail();
	}

	// 2. The arena layout.
	UDwDeckLayout* Arena = FindOrCreate<UDwDeckLayout>(Boards, TEXT("DA_Deck_KrakenArena"), Log);
	TArray<FString> ParseErrors;
	const FDwDeckSource Source = FDwDeckAscii::Parse(PlaceholderArenaRows(), &ParseErrors);
	for (const FString& Error : ParseErrors)
	{
		Log.Add(TEXT("ERROR DA_Deck_KrakenArena: ") + Error);
		bOk = false;
	}
	Arena->DisplayName = FText::FromString(TEXT("Kraken arena (placeholder)"));
	Arena->Size = Source.Size;
	Arena->Tiles = Source.Tiles;
	Arena->BoardingRowOffset = 0;
	bOk &= Validate(Arena, Log) && Save(Arena, Log);

	// 3. Tentacle Slam: one crew member or the ship, anywhere (Docs/Architecture/08 "Moves"); damage from DT_AbilityTuning.
	UBlueprint* SlamBp = FindOrCreateBlueprint(Abilities, TEXT("GA_Kraken_TentacleSlam"), UDwGA_EffectPayload::StaticClass(), Log);
	UDwGA_EffectPayload* Slam = Defaults<UDwGA_EffectPayload>(SlamBp);
	if (!Slam)
	{
		return Fail();
	}
	FDwEffectApplication Damage;
	Damage.Effect = DwGas::GetSharedEffects().Damage;
	Slam->Targeting = FDwTargetingRules();
	Slam->Targeting.Pattern = EDwTargetPattern::SingleUnitOrShip;
	Slam->Targeting.Side = EDwTargetSide::Enemy;
	Slam->Targeting.bIgnoreRange = true;
	Slam->Applications = { Damage };
	Slam->HitCue = DwTags::GameplayCue_Hit_Melee; // no Kraken cues or art until DW-307
	Slam->PlaceholderSeconds = 0.5f;
	bOk &= Save(SlamBp, Log);

	UDwAbilityDefinition* SlamDef = FindOrCreate<UDwAbilityDefinition>(AbilityData, TEXT("DA_Ability_Kraken_TentacleSlam"), Log);
	SlamDef->DisplayName = FText::FromString(TEXT("Tentacle Slam"));
	SlamDef->Description = FText::FromString(TEXT("Slams one crew member or the ship."));
	SlamDef->AbilityClass = TSubclassOf<UDwGameplayAbility>(SlamBp->GeneratedClass.Get());
	SlamDef->AbilityTag = DwTags::Ability_Kraken_TentacleSlam;
	SlamDef->AbilityTags = FGameplayTagContainer();
	SlamDef->Tuning = Row(AbilityTable, TEXT("Kraken_TentacleSlam"), Log, bOk);
	bOk &= Validate(SlamDef, Log) && Save(SlamDef, Log);

	// 4. The body: 500 HP (DT_UnitTuning KrakenBody), Tentacle Slam as its one move. Placeholder look: the plain unit
	//    actor (Kraken visuals are DW-307). Immunities wait for Open-Questions B9/K10.
	UDwUnitDefinition* Body = FindOrCreate<UDwUnitDefinition>(UnitData, TEXT("DA_Unit_KrakenBody"), Log);
	Body->DisplayName = FText::FromString(TEXT("Kraken"));
	Body->UnitTags = FGameplayTagContainer(DwTags::Unit_Kraken_Body);
	Body->BasicAttack = SlamDef;
	Body->DefaultAbilities.Reset();
	Body->Tuning = Row(UnitTable, TEXT("KrakenBody"), Log, bOk);
	bOk &= Validate(Body, Log) && Save(Body, Log);

	// 5. The Kraken: one phase from full health with Tentacle Slam only, no tentacles (08 "Placeholder boss").
	UDwKrakenDefinition* Kraken = FindOrCreate<UDwKrakenDefinition>(KrakenData, TEXT("DA_Kraken_Placeholder"), Log);
	Kraken->DisplayName = FText::FromString(TEXT("Kraken (placeholder)"));
	Kraken->Body = Body;
	Kraken->Tentacle = nullptr;
	Kraken->Arena = Arena;
	Kraken->Phases.Reset();
	FDwKrakenPhase& Phase = Kraken->Phases.AddDefaulted_GetRef();
	Phase.PhaseId = TEXT("Placeholder");
	Phase.EnterBelowHealthPct = 1.f;
	FDwWeightedAbility& Move = Phase.Moves.AddDefaulted_GetRef();
	Move.Ability = SlamDef;
	Move.Weight = 1.f;
	Phase.MusicState = DwTags::Music_Boss;
	Kraken->Tuning = Row(KrakenTable, TEXT("Kraken"), Log, bOk);
	bOk &= Validate(Kraken, Log) && Save(Kraken, Log);

	// 6. The boss encounter, played by the boss node (never in the fight pool: kind Kraken).
	UDwEncounterDefinition* Encounter = FindOrCreate<UDwEncounterDefinition>(EncounterData, TEXT("DA_Encounter_KrakenPlaceholder"), Log);
	Encounter->DisplayName = FText::FromString(TEXT("The Kraken"));
	Encounter->Kind = EDwEncounterKind::Kraken;
	Encounter->Kraken = Kraken;
	Encounter->AiProfile = AiProfile;
	Encounter->Units.Reset();
	Encounter->ShipDeck = nullptr;
	Encounter->ShipClass.Reset();
	Encounter->ShipTuning = FDataTableRowHandle();
	Encounter->Tuning = Row(EncounterTable, TEXT("Kraken"), Log, bOk);
	bOk &= Validate(Encounter, Log) && Save(Encounter, Log);

	// 7. DA_GameData: the boss node's encounter and the Kraken tuning table.
	GameData->KrakenEncounter = Encounter;
	GameData->TuningTables.Kraken = KrakenTable;
	bOk &= Save(GameData, Log);

	Log.Add(TEXT("DA_Deck_KrakenArena:"));
	for (const FString& Line : FDwDeckAscii::Export(FDwDeckSource::FromLayout(*Arena)))
	{
		Log.Add(TEXT("    ") + Line);
	}
	Log.Add(bOk ? TEXT("CONTENT PASS") : TEXT("CONTENT FAIL"));
	return Log;
}
