// Copyright Brandon Hillig 2026

#include "DwKrakenContentLibrary.h"

#include "AI/DwAiProfile.h"
#include "AI/DwKrakenAiBrain.h"
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
#include "EditorAssetLibrary.h"
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

	/** One Kraken move: a GA_Kraken_* payload Blueprint and its DA_Ability_Kraken_* definition (Docs/Architecture/08 "Moves"). */
	struct FMoveSpec
	{
		const TCHAR* Name;        // GA_Kraken_<Name>, DA_Ability_Kraken_<Name>, DT_AbilityTuning row Kraken_<Name>
		const TCHAR* DisplayName;
		const TCHAR* Description;
		FGameplayTag AbilityTag;
		EDwTargetPattern Pattern;
		TSubclassOf<UGameplayEffect> Effect;
		/** Telegraphed at a tile: moving off it dodges (Brandon, DW-302). */
		bool bAimAtTile = false;
	};

	UDwAbilityDefinition* MakeMove(const FMoveSpec& Spec, UDataTable* AbilityTable, TArray<FString>& Log, bool& bOk)
	{
		using namespace DwContentAssets;
		UBlueprint* Bp = FindOrCreateBlueprint(Abilities, FString(TEXT("GA_Kraken_")) + Spec.Name, UDwGA_EffectPayload::StaticClass(), Log);
		UDwGA_EffectPayload* Move = Defaults<UDwGA_EffectPayload>(Bp);
		if (!Move)
		{
			bOk = false;
			return nullptr;
		}
		FDwEffectApplication Application;
		Application.Effect = Spec.Effect;
		Move->Targeting = FDwTargetingRules();
		Move->Targeting.Pattern = Spec.Pattern;
		Move->Targeting.Side = Spec.Pattern == EDwTargetPattern::Self ? EDwTargetSide::Ally : EDwTargetSide::Enemy;
		Move->Targeting.bIgnoreRange = true; // it reaches the whole ship from the water
		Move->Targeting.bAimAtTile = Spec.bAimAtTile;
		Move->Applications = { Application };
		Move->HitCue = DwTags::GameplayCue_Hit_Melee; // Kraken cues and art are DW-307
		Move->PlaceholderSeconds = 0.5f;
		bOk &= Save(Bp, Log);

		UDwAbilityDefinition* Def = FindOrCreate<UDwAbilityDefinition>(AbilityData, FString(TEXT("DA_Ability_Kraken_")) + Spec.Name, Log);
		Def->DisplayName = FText::FromString(Spec.DisplayName);
		Def->Description = FText::FromString(Spec.Description);
		Def->AbilityClass = TSubclassOf<UDwGameplayAbility>(Bp->GeneratedClass.Get());
		Def->AbilityTag = Spec.AbilityTag;
		Def->AbilityTags = FGameplayTagContainer();
		Def->Tuning = Row(AbilityTable, *(FString(TEXT("Kraken_")) + Spec.Name), Log, bOk);
		bOk &= Validate(Def, Log) && Save(Def, Log);
		return Def;
	}

	/** Removes a DW-209 placeholder asset the real Kraken replaced, if it is still there. */
	void DeleteIfPresent(const FString& Folder, const TCHAR* Name, TArray<FString>& Log)
	{
		const FString Path = Folder / Name;
		if (UEditorAssetLibrary::DoesAssetExist(Path))
		{
			Log.Add(UEditorAssetLibrary::DeleteAsset(Path) ? FString::Printf(TEXT("Deleted %s"), *Path)
				: FString::Printf(TEXT("ERROR could not delete %s"), *Path));
		}
	}
}

TArray<FString> UDwKrakenContentLibrary::ArenaRows()
{
	// Authored facing right: the rightmost column lies against the player's rail. Four tentacle slots along the rail
	// (melee reach, B4), the body three tiles out in the water: muskets and cannons reach it, cutlasses don't, and the
	// sniper's nest is just in range. Open-Questions B1 (4 tentacles) and B5 (player ship only, nothing to board).
	return { TEXT("~~~~T"), TEXT("~~~~T"), TEXT("~~B~T"), TEXT("~~~~T") };
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
	const FDwDeckSource Source = FDwDeckAscii::Parse(ArenaRows(), &ParseErrors);
	for (const FString& Error : ParseErrors)
	{
		Log.Add(TEXT("ERROR DA_Deck_KrakenArena: ") + Error);
		bOk = false;
	}
	Arena->DisplayName = FText::FromString(TEXT("Kraken arena"));
	Arena->Size = Source.Size;
	Arena->Tiles = Source.Tiles;
	Arena->BoardingRowOffset = 0;
	bOk &= Validate(Arena, Log) && Save(Arena, Log);

	// 3. The five moves (08 "Moves"); numbers from DT_AbilityTuning. B8: Swipe and Arms of Fury hit crew only.
	const FDwSharedEffects& Shared = DwGas::GetSharedEffects();
	const FMoveSpec Moves[] = {
		{ TEXT("TentacleSlam"), TEXT("Tentacle Slam"), TEXT("Slams the tile a crew member stands on (step off it to dodge), or the ship."),
			DwTags::Ability_Kraken_TentacleSlam, EDwTargetPattern::SingleUnitOrShip, Shared.Damage, true },
		{ TEXT("KrankySwipe"), TEXT("Kranky Swipe"), TEXT("Sweeps the deck: every crew member is hit."),
			DwTags::Ability_Kraken_KrankySwipe, EDwTargetPattern::AllEnemies, Shared.Damage },
		{ TEXT("TentacleGrab"), TEXT("Tentacle Grab"), TEXT("Grabs whoever stands on a tile (step off it to dodge): they can't move next turn."),
			DwTags::Ability_Kraken_TentacleGrab, EDwTargetPattern::SingleUnit, Shared.Rooted, true },
		{ TEXT("ArmsOfFury"), TEXT("Arms of Fury"), TEXT("Lashes out 3-5 times at random crew."),
			DwTags::Ability_Kraken_ArmsOfFury, EDwTargetPattern::RandomEnemies, Shared.Damage },
		{ TEXT("EyesOfTheSea"), TEXT("Eyes of the Sea"), TEXT("Gathers itself: two actions next turn."),
			DwTags::Ability_Kraken_EyesOfTheSea, EDwTargetPattern::Self, UDwGE_ExtraActionNextTurn::StaticClass() },
	};
	TMap<FGameplayTag, UDwAbilityDefinition*> MoveDefs;
	for (const FMoveSpec& Spec : Moves)
	{
		if (UDwAbilityDefinition* Def = MakeMove(Spec, AbilityTable, Log, bOk))
		{
			MoveDefs.Add(Spec.AbilityTag, Def);
		}
	}
	if (MoveDefs.Num() != UE_ARRAY_COUNT(Moves))
	{
		return Fail();
	}

	// 4. The body: 500 HP (DT_UnitTuning KrakenBody), all five moves (the phase decides which it may use).
	//    Immunities: K10 a) - no Fear, no grapple, no freeze. B4: melee can't reach it; cannons aim at it.
	UDwUnitDefinition* Body = FindOrCreate<UDwUnitDefinition>(UnitData, TEXT("DA_Unit_KrakenBody"), Log);
	Body->DisplayName = FText::FromString(TEXT("Kraken"));
	Body->UnitTags = FGameplayTagContainer(DwTags::Unit_Kraken_Body);
	Body->UnitTags.AddTag(DwTags::Unit_Immune_Fear);
	Body->UnitTags.AddTag(DwTags::Unit_Immune_Grapple);
	Body->UnitTags.AddTag(DwTags::Unit_Immune_Freeze); // freeze works on tentacles only
	Body->UnitTags.AddTag(DwTags::Unit_Immune_Melee);
	Body->UnitTags.AddTag(DwTags::Unit_CannonTarget);
	Body->BasicAttack = MoveDefs[DwTags::Ability_Kraken_TentacleSlam];
	Body->DefaultAbilities = { MoveDefs[DwTags::Ability_Kraken_KrankySwipe], MoveDefs[DwTags::Ability_Kraken_TentacleGrab],
		MoveDefs[DwTags::Ability_Kraken_ArmsOfFury], MoveDefs[DwTags::Ability_Kraken_EyesOfTheSea] };
	Body->Tuning = Row(UnitTable, TEXT("KrakenBody"), Log, bOk);
	bOk &= Validate(Body, Log) && Save(Body, Log);

	// 5. A tentacle: a target with HP (DT_UnitTuning KrakenTentacle), no moves of its own, never takes a turn.
	//    K10 a): no Fear, no grapple; it can be frozen (harmless: it never acts). Placeholder look until DW-307.
	UDwUnitDefinition* Tentacle = FindOrCreate<UDwUnitDefinition>(UnitData, TEXT("DA_Unit_KrakenTentacle"), Log);
	Tentacle->DisplayName = FText::FromString(TEXT("Tentacle"));
	Tentacle->UnitTags = FGameplayTagContainer(DwTags::Unit_Kraken_Tentacle);
	Tentacle->UnitTags.AddTag(DwTags::Unit_Immune_Fear);
	Tentacle->UnitTags.AddTag(DwTags::Unit_Immune_Grapple);
	Tentacle->BasicAttack = nullptr;
	Tentacle->DefaultAbilities.Reset();
	Tentacle->Tuning = Row(UnitTable, TEXT("KrakenTentacle"), Log, bOk);
	bOk &= Validate(Tentacle, Log) && Save(Tentacle, Log);

	// 6. The Kraken: the initial phase from full health, enraged below 30% (08 "Phases"). Move weights are
	//    placeholders for DW-302's brain (Docs/Balance/Placeholders.md).
	UDwKrakenDefinition* Kraken = FindOrCreate<UDwKrakenDefinition>(KrakenData, TEXT("DA_Kraken"), Log);
	Kraken->DisplayName = FText::FromString(TEXT("The Kraken"));
	Kraken->Body = Body;
	Kraken->Tentacle = Tentacle;
	Kraken->Arena = Arena;
	Kraken->TentacleRules.Effect = EDwTentacleEffect::ReduceDamage; // B2
	Kraken->Phases.Reset();
	auto AddPhase = [&](const TCHAR* Id, float EnterBelow, FGameplayTag Music, FGameplayTag Cue,
		std::initializer_list<TPair<FGameplayTag, float>> PhaseMoves)
	{
		FDwKrakenPhase& Phase = Kraken->Phases.AddDefaulted_GetRef();
		Phase.PhaseId = Id;
		Phase.EnterBelowHealthPct = EnterBelow;
		Phase.MusicState = Music;
		Phase.EnterCue = Cue;
		for (const TPair<FGameplayTag, float>& Move : PhaseMoves)
		{
			FDwWeightedAbility& Entry = Phase.Moves.AddDefaulted_GetRef();
			Entry.Ability = MoveDefs[Move.Key];
			Entry.Weight = Move.Value;
		}
	};
	AddPhase(TEXT("Initial"), 1.f, DwTags::Music_Boss, FGameplayTag(), {
		{ DwTags::Ability_Kraken_TentacleSlam, 1.f }, { DwTags::Ability_Kraken_KrankySwipe, 1.f }, { DwTags::Ability_Kraken_TentacleGrab, 1.f } });
	AddPhase(TEXT("Enraged"), 0.3f, DwTags::Music_Boss_Enraged, DwTags::GameplayCue_Kraken_Enrage, {
		{ DwTags::Ability_Kraken_ArmsOfFury, 1.f }, { DwTags::Ability_Kraken_EyesOfTheSea, 1.f } });
	Kraken->Tuning = Row(KrakenTable, TEXT("Kraken"), Log, bOk);
	bOk &= Validate(Kraken, Log) && Save(Kraken, Log);

	// 6b. Its brain (DW-302): UDwKrakenAiBrain, target weights in DT_AiTuning row Kraken (Tuning/AiTuning.json).
	UDataTable* AiTable = Load<UDataTable>(Tuning, TEXT("DT_AiTuning"));
	UDwAiProfile* KrakenProfile = FindOrCreate<UDwAiProfile>(AiData, TEXT("DA_AiProfile_Kraken"), Log);
	KrakenProfile->DisplayName = FText::FromString(TEXT("Kraken"));
	KrakenProfile->Description = FText::FromString(TEXT("Telegraphs every move; slams the wounded, the captain when the hit would kill, the ship when its hull is low."));
	KrakenProfile->BrainClass = UDwKrakenAiBrain::StaticClass();
	KrakenProfile->bPreferCannons = false;
	KrakenProfile->bBoarders = false;
	KrakenProfile->Tuning = Row(AiTable, TEXT("Kraken"), Log, bOk);
	bOk &= Validate(KrakenProfile, Log) && Save(KrakenProfile, Log);

	// 7. The boss encounter, played by the boss node (never in the fight pool: kind Kraken).
	UDwEncounterDefinition* Encounter = FindOrCreate<UDwEncounterDefinition>(EncounterData, TEXT("DA_Encounter_Kraken"), Log);
	Encounter->DisplayName = FText::FromString(TEXT("The Kraken"));
	Encounter->Kind = EDwEncounterKind::Kraken;
	Encounter->Kraken = Kraken;
	Encounter->AiProfile = KrakenProfile;
	Encounter->Units.Reset();
	Encounter->ShipDeck = nullptr;
	Encounter->ShipClass.Reset();
	Encounter->ShipTuning = FDataTableRowHandle();
	Encounter->Tuning = Row(EncounterTable, TEXT("Kraken"), Log, bOk);
	bOk &= Validate(Encounter, Log) && Save(Encounter, Log);

	// 8. DA_GameData: the boss node's encounter and the Kraken tuning table.
	GameData->KrakenEncounter = Encounter;
	GameData->TuningTables.Kraken = KrakenTable;
	bOk &= Save(GameData, Log);

	// 9. The DW-209 placeholders this replaces (nothing references them once DA_GameData points at the real one).
	DeleteIfPresent(EncounterData, TEXT("DA_Encounter_KrakenPlaceholder"), Log);
	DeleteIfPresent(KrakenData, TEXT("DA_Kraken_Placeholder"), Log);

	Log.Add(TEXT("DA_Deck_KrakenArena:"));
	for (const FString& Line : FDwDeckAscii::Export(FDwDeckSource::FromLayout(*Arena)))
	{
		Log.Add(TEXT("    ") + Line);
	}
	Log.Add(bOk ? TEXT("CONTENT PASS") : TEXT("CONTENT FAIL"));
	return Log;
}
