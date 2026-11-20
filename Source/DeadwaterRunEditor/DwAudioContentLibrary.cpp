// Copyright Brandon Hillig 2026

#include "DwAudioContentLibrary.h"

#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "DwContentAssetUtils.h"
#include "Misc/DataValidation.h"
#include "NiagaraSystem.h"
#include "Presentation/DwAudioCueMap.h"
#include "Presentation/DwGameplayCueNotify_Burst.h"
#include "Presentation/DwMusicSet.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace DwAudioContent
{
	const TCHAR* AudioFolder = TEXT("/Game/DeadwaterRun/Audio");
	const TCHAR* CueFolder = TEXT("/Game/DeadwaterRun/VFX/Cues");

	/**
	 * The DW-114 placeholder cues. Niagara systems are Synty stand-ins until DW-307 builds the real ones; the cap keeps
	 * any looping stand-in bounded.
	 */
	struct FCueSpec
	{
		const TCHAR* Name;
		FGameplayTag Tag;
		const TCHAR* Niagara;
		float MaxEffectSeconds;
	};

	TArray<FCueSpec> CueSpecs()
	{
		return {
			{ TEXT("GC_Hit_Melee"), DwTags::GameplayCue_Hit_Melee, TEXT("/Game/Synty/PolygonPirateRealm/FX/NS_FX_Sparks_01.NS_FX_Sparks_01"), 1.f },
			{ TEXT("GC_Cannon_Fire"), DwTags::GameplayCue_Cannon_Fire, TEXT("/Game/Synty/PolygonPirateRealm/FX/NS_FX_CannonShot_01.NS_FX_CannonShot_01"), 2.f },
			{ TEXT("GC_Cannon_Impact"), DwTags::GameplayCue_Cannon_Impact, TEXT("/Game/Synty/PolygonPirateRealm/FX/NS_FX_Canon_Rock_01.NS_FX_Canon_Rock_01"), 2.f },
			{ TEXT("GC_Unit_Death"), DwTags::GameplayCue_Unit_Death, TEXT("/Game/Synty/PolygonGeneric/FX/NS_Smoke_01.NS_Smoke_01"), 1.5f },
		};
	}

	/**
	 * PLACEHOLDER (sound-wrangler owns DA_MusicSet; question for Brandon in the DW-114 report): the design names no
	 * music for the between-fight screens, so rewards, level-up, shop and events keep the map music.
	 */
	TMap<EDwRunPhase, FGameplayTag> DefaultPhaseStates()
	{
		return {
			{ EDwRunPhase::Map, DwTags::Music_Map },
			{ EDwRunPhase::Rewards, DwTags::Music_Map },
			{ EDwRunPhase::LevelUp, DwTags::Music_Map },
			{ EDwRunPhase::Shop, DwTags::Music_Map },
			{ EDwRunPhase::Event, DwTags::Music_Map },
			{ EDwRunPhase::Victory, DwTags::Music_Victory },
			{ EDwRunPhase::Defeat, DwTags::Music_Defeat },
		};
	}

	bool Validate(const UObject* Asset, TArray<FString>& Log)
	{
		FDataValidationContext Context;
		const bool bValid = Asset->IsDataValid(Context) != EDataValidationResult::Invalid;
		for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
		{
			Log.Add(FString::Printf(TEXT("%s %s: %s"), Issue.Severity == EMessageSeverity::Error ? TEXT("ERROR") : TEXT("WARNING"),
				*Asset->GetName(), *Issue.Message.ToString()));
		}
		return bValid;
	}
}

TArray<FString> UDwAudioContentLibrary::CreateAudioContent()
{
	using namespace DwContentAssets;
	using namespace DwAudioContent;
	TArray<FString> Log;
	bool bOk = true;

	// 1. DA_AudioCueMap: empty on creation; sound-wrangler fills it (DW-311), so its entries are never rewritten here.
	const bool bHadCueMap = Load<UDwAudioCueMap>(AudioFolder, TEXT("DA_AudioCueMap")) != nullptr;
	UDwAudioCueMap* CueMap = FindOrCreate<UDwAudioCueMap>(AudioFolder, TEXT("DA_AudioCueMap"), Log);
	bOk &= Validate(CueMap, Log);
	if (!bHadCueMap)
	{
		bOk &= Save(CueMap, Log);
	}
	Log.Add(FString::Printf(TEXT("DA_AudioCueMap has %d cue entries"), CueMap->Cues.Num()));

	// 2. DA_MusicSet: no tracks until DW-311; the run-phase states are seeded once.
	UDwMusicSet* MusicSet = FindOrCreate<UDwMusicSet>(AudioFolder, TEXT("DA_MusicSet"), Log);
	const bool bSeedPhases = MusicSet->PhaseStates.IsEmpty();
	if (bSeedPhases)
	{
		MusicSet->PhaseStates = DefaultPhaseStates();
		Log.Add(TEXT("DA_MusicSet: PLACEHOLDER phase states (rewards, level-up, shop and events keep the map music)"));
	}
	bOk &= Validate(MusicSet, Log);
	if (bSeedPhases)
	{
		bOk &= Save(MusicSet, Log);
	}
	Log.Add(FString::Printf(TEXT("DA_MusicSet has %d tracks, %d phase states"), MusicSet->Tracks.Num(), MusicSet->PhaseStates.Num()));

	// 3. The placeholder cue notifies.
	for (const FCueSpec& Spec : CueSpecs())
	{
		UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, Spec.Niagara);
		UBlueprint* Blueprint = FindOrCreateBlueprint(CueFolder, Spec.Name, UDwGameplayCueNotify_Burst::StaticClass(), Log);
		UDwGameplayCueNotify_Burst* Cue = Defaults<UDwGameplayCueNotify_Burst>(Blueprint);
		if (!System || !Cue)
		{
			Log.Add(FString::Printf(TEXT("ERROR %s: %s"), Spec.Name, System ? TEXT("blueprint has no defaults") : *FString::Printf(TEXT("missing %s"), Spec.Niagara)));
			bOk = false;
			continue;
		}
		Cue->GameplayCueTag = Spec.Tag;
		Cue->MaxEffectSeconds = Spec.MaxEffectSeconds;
		Cue->SetBurstNiagaraSystems({ System }); // no sounds on the asset: they come from DA_AudioCueMap
		bOk &= Validate(Cue, Log);
		bOk &= Save(Blueprint, Log);
		Log.Add(FString::Printf(TEXT("%s: %s -> %s (cap %.1f s, PLACEHOLDER until DW-307)"), Spec.Name, *Spec.Tag.ToString(), *System->GetName(),
			Spec.MaxEffectSeconds));
	}

	// 4. DA_GameData.Audio.
	const FString Data = TEXT("/Game/DeadwaterRun/Data");
	UDwGameData* GameData = Load<UDwGameData>(Data, UDwGameData::AssetName.ToString());
	FDwAudioConfig Audio;
	Audio.CueMap = CueMap;
	Audio.MusicSet = MusicSet;
	Audio.BaseMix = Load<USoundMix>(FString(AudioFolder) / TEXT("Mix"), TEXT("SM_Base"));
	Audio.PauseMix = Load<USoundMix>(FString(AudioFolder) / TEXT("Mix"), TEXT("SM_Pause"));
	Audio.MasterClass = Load<USoundClass>(FString(AudioFolder) / TEXT("Classes"), TEXT("SC_Master"));
	Audio.MusicClass = Load<USoundClass>(FString(AudioFolder) / TEXT("Classes"), TEXT("SC_Music"));
	Audio.SfxClass = Load<USoundClass>(FString(AudioFolder) / TEXT("Classes"), TEXT("SC_SFX"));
	Audio.UiClass = Load<USoundClass>(FString(AudioFolder) / TEXT("Classes"), TEXT("SC_UI"));
	if (!GameData || !Audio.BaseMix || !Audio.PauseMix || !Audio.MasterClass || !Audio.MusicClass || !Audio.SfxClass || !Audio.UiClass)
	{
		Log.Add(TEXT("ERROR DA_GameData or a DW-113 sound class / mix is missing (run Tools/CreateAudioContent.ps1 first)"));
		bOk = false;
	}
	else
	{
		GameData->Audio = Audio;
		bOk &= Save(GameData, Log);
		Log.Add(TEXT("DA_GameData.Audio: DA_AudioCueMap, DA_MusicSet, SM_Base, SM_Pause, SC_Master/Music/SFX/UI"));
	}

	Log.Add(bOk ? TEXT("CONTENT PASS") : TEXT("CONTENT FAIL"));
	return Log;
}
