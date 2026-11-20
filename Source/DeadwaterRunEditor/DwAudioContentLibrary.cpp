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
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"
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
			// Sound-only (nullptr Niagara) until DW-307: Brandon took sound-wrangler's extra cues on 2026-11-20.
			{ TEXT("GC_Kraken_Appear"), DwTags::GameplayCue_Kraken_Appear, nullptr, 0.f },
			{ TEXT("GC_Kraken_Death"), DwTags::GameplayCue_Kraken_Death, nullptr, 0.f },
			{ TEXT("GC_Kraken_TentacleCut"), DwTags::GameplayCue_Kraken_TentacleCut, nullptr, 0.f },
			{ TEXT("GC_Attack_Miss"), DwTags::GameplayCue_Attack_Miss, nullptr, 0.f },
			// DW-311: every cue the game fires needs a notify for its sounds to play. Sound-only until DW-307.
			{ TEXT("GC_Hit_Ranged"), DwTags::GameplayCue_Hit_Ranged, nullptr, 0.f },
			{ TEXT("GC_Unit_Heal"), DwTags::GameplayCue_Unit_Heal, nullptr, 0.f },
			{ TEXT("GC_Status_Fear"), DwTags::GameplayCue_Status_Fear, nullptr, 0.f },
			{ TEXT("GC_Ship_Sink"), DwTags::GameplayCue_Ship_Sink, nullptr, 0.f },
			{ TEXT("GC_Kraken_Enrage"), DwTags::GameplayCue_Kraken_Enrage, nullptr, 0.f },
			{ TEXT("GC_Kraken_TentacleSlam"), DwTags::GameplayCue_Kraken_TentacleSlam, nullptr, 0.f },
			{ TEXT("GC_Kraken_KrankySwipe"), DwTags::GameplayCue_Kraken_KrankySwipe, nullptr, 0.f },
			{ TEXT("GC_Kraken_TentacleGrab"), DwTags::GameplayCue_Kraken_TentacleGrab, nullptr, 0.f },
			{ TEXT("GC_Kraken_ArmsOfFury"), DwTags::GameplayCue_Kraken_ArmsOfFury, nullptr, 0.f },
			{ TEXT("GC_Kraken_EyesOfTheSea"), DwTags::GameplayCue_Kraken_EyesOfTheSea, nullptr, 0.f },
		};
	}

	/** Tools/Content/audio_cue_map.json (DW-311, sound-wrangler's source of truth for the cue map and music tracks). */
	FString CueMapJsonPath()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Tools/Content/audio_cue_map.json"));
	}

	/** "SFX/Combat/SW_MeleeHit_01" -> the asset under /Game/DeadwaterRun/Audio, or null (logged as an error). */
	template <typename T>
	T* LoadAudio(const FString& RelativePath, const FString& Where, TArray<FString>& Log, bool& bOk)
	{
		const FString Name = FPaths::GetBaseFilename(RelativePath);
		const FString Path = FString::Printf(TEXT("%s/%s.%s"), AudioFolder, *RelativePath, *Name);
		T* Asset = LoadObject<T>(nullptr, *Path);
		if (!Asset)
		{
			Log.Add(FString::Printf(TEXT("ERROR %s: no %s at %s"), *Where, *T::StaticClass()->GetName(), *Path));
			bOk = false;
		}
		return Asset;
	}

	FGameplayTag JsonTag(const FString& Name, TArray<FString>& Log, bool& bOk)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Name), /*ErrorIfNotFound*/ false);
		if (!Tag.IsValid())
		{
			Log.Add(FString::Printf(TEXT("ERROR audio_cue_map.json: unknown tag %s"), *Name));
			bOk = false;
		}
		return Tag;
	}

	TArray<FDwCueSoundLayer> ReadLayers(const TSharedPtr<FJsonObject>& Cue, const TCHAR* Section, const FString& CueName,
		const TMap<FString, USoundConcurrency*>& Concurrency, TArray<FString>& Log, bool& bOk)
	{
		TArray<FDwCueSoundLayer> Layers;
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (!Cue->TryGetArrayField(Section, Array))
		{
			return Layers;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			const TSharedPtr<FJsonObject> Object = Value->AsObject();
			FDwCueSoundLayer& Layer = Layers.AddDefaulted_GetRef();
			const TArray<TSharedPtr<FJsonValue>>* Sounds = nullptr;
			if (Object->TryGetArrayField(TEXT("sounds"), Sounds))
			{
				for (const TSharedPtr<FJsonValue>& Sound : *Sounds)
				{
					if (USoundBase* Loaded = LoadAudio<USoundBase>(Sound->AsString(), CueName, Log, bOk))
					{
						Layer.Sounds.Add(Loaded);
					}
				}
			}
			Object->TryGetNumberField(TEXT("volume"), Layer.Volume);
			const TArray<TSharedPtr<FJsonValue>>* Pitch = nullptr;
			if (Object->TryGetArrayField(TEXT("pitch"), Pitch) && Pitch->Num() == 2)
			{
				Layer.PitchMin = (*Pitch)[0]->AsNumber();
				Layer.PitchMax = (*Pitch)[1]->AsNumber();
			}
			FString ConcurrencyName;
			if (Object->TryGetStringField(TEXT("concurrency"), ConcurrencyName))
			{
				USoundConcurrency* const* Found = Concurrency.Find(ConcurrencyName);
				if (!Found)
				{
					Log.Add(FString::Printf(TEXT("ERROR %s: unknown concurrency %s"), *CueName, *ConcurrencyName));
					bOk = false;
				}
				else
				{
					Layer.Concurrency = *Found;
				}
			}
		}
		return Layers;
	}

	/**
	 * Rebuilds CueMap->Cues and MusicSet->Tracks from the JSON file (the file is the source of truth; the assets are
	 * regenerated). False with errors in Log if the file is missing, malformed or names a sound or tag that doesn't exist.
	 */
	bool FillFromJson(UDwAudioCueMap& CueMap, UDwMusicSet& MusicSet, TArray<FString>& Log)
	{
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Text, *CueMapJsonPath()) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
		{
			Log.Add(FString::Printf(TEXT("ERROR cannot read %s"), *CueMapJsonPath()));
			return false;
		}
		bool bOk = true;

		TMap<FString, USoundConcurrency*> Concurrency;
		const TSharedPtr<FJsonObject>* ConcurrencyObject = nullptr;
		if (Root->TryGetObjectField(TEXT("concurrency"), ConcurrencyObject))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*ConcurrencyObject)->Values)
			{
				if (USoundConcurrency* Loaded = LoadAudio<USoundConcurrency>(Entry.Value->AsString(), Entry.Key, Log, bOk))
				{
					Concurrency.Add(Entry.Key, Loaded);
				}
			}
		}

		CueMap.Cues.Reset();
		const TSharedPtr<FJsonObject>* Cues = nullptr;
		if (Root->TryGetObjectField(TEXT("cues"), Cues))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*Cues)->Values)
			{
				const FGameplayTag Tag = JsonTag(Entry.Key, Log, bOk);
				const TSharedPtr<FJsonObject> Object = Entry.Value->AsObject();
				if (!Tag.IsValid() || !Object)
				{
					continue;
				}
				FDwCueSound& Sound = CueMap.Cues.Add(Tag);
				Sound.Start = ReadLayers(Object, TEXT("start"), Entry.Key, Concurrency, Log, bOk);
				Sound.Loop = ReadLayers(Object, TEXT("loop"), Entry.Key, Concurrency, Log, bOk);
				Sound.Stop = ReadLayers(Object, TEXT("stop"), Entry.Key, Concurrency, Log, bOk);
			}
		}

		MusicSet.Tracks.Reset();
		const TSharedPtr<FJsonObject>* Music = nullptr;
		if (Root->TryGetObjectField(TEXT("music"), Music))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*Music)->Values)
			{
				const FGameplayTag Tag = JsonTag(Entry.Key, Log, bOk);
				const TSharedPtr<FJsonObject> Object = Entry.Value->AsObject();
				if (!Tag.IsValid() || !Object)
				{
					continue;
				}
				FDwMusicTrack& Track = MusicSet.Tracks.Add(Tag);
				FString Path;
				if (Object->TryGetStringField(TEXT("intro"), Path))
				{
					Track.Intro = LoadAudio<USoundBase>(Path, Entry.Key, Log, bOk);
				}
				if (Object->TryGetStringField(TEXT("sound"), Path))
				{
					Track.Sound = LoadAudio<USoundBase>(Path, Entry.Key, Log, bOk);
				}
				Object->TryGetBoolField(TEXT("loop"), Track.bLoop);
				Object->TryGetNumberField(TEXT("volume"), Track.Volume);
			}
		}
		return bOk;
	}

	/**
	 * Brandon (2026-11-20): rewards, level-up, shop and events keep the map music (sound-wrangler owns DA_MusicSet;
	 * these are only seeded once).
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

	// 1-2. DA_AudioCueMap cues and DA_MusicSet tracks, rebuilt from Tools/Content/audio_cue_map.json (DW-311); the
	// run-phase states are seeded once.
	UDwAudioCueMap* CueMap = FindOrCreate<UDwAudioCueMap>(AudioFolder, TEXT("DA_AudioCueMap"), Log);
	UDwMusicSet* MusicSet = FindOrCreate<UDwMusicSet>(AudioFolder, TEXT("DA_MusicSet"), Log);
	if (MusicSet->PhaseStates.IsEmpty())
	{
		MusicSet->PhaseStates = DefaultPhaseStates();
		Log.Add(TEXT("DA_MusicSet: seeded phase states (rewards, level-up, shop and events keep the map music)"));
	}
	bOk &= FillFromJson(*CueMap, *MusicSet, Log);
	bOk &= Validate(CueMap, Log) && Save(CueMap, Log);
	bOk &= Validate(MusicSet, Log) && Save(MusicSet, Log);
	Log.Add(FString::Printf(TEXT("DA_AudioCueMap has %d cue entries (from %s)"), CueMap->Cues.Num(), *CueMapJsonPath()));
	Log.Add(FString::Printf(TEXT("DA_MusicSet has %d tracks, %d phase states"), MusicSet->Tracks.Num(), MusicSet->PhaseStates.Num()));

	// 3. The placeholder cue notifies.
	for (const FCueSpec& Spec : CueSpecs())
	{
		const bool bSoundOnly = Spec.Niagara == nullptr;
		UNiagaraSystem* System = bSoundOnly ? nullptr : LoadObject<UNiagaraSystem>(nullptr, Spec.Niagara);
		UBlueprint* Blueprint = FindOrCreateBlueprint(CueFolder, Spec.Name, UDwGameplayCueNotify_Burst::StaticClass(), Log);
		UDwGameplayCueNotify_Burst* Cue = Defaults<UDwGameplayCueNotify_Burst>(Blueprint);
		if ((!bSoundOnly && !System) || !Cue)
		{
			Log.Add(FString::Printf(TEXT("ERROR %s: %s"), Spec.Name, Cue ? *FString::Printf(TEXT("missing %s"), Spec.Niagara) : TEXT("blueprint has no defaults")));
			bOk = false;
			continue;
		}
		Cue->GameplayCueTag = Spec.Tag;
		Cue->MaxEffectSeconds = Spec.MaxEffectSeconds;
		// No sounds on the asset: they come from DA_AudioCueMap.
		Cue->SetBurstNiagaraSystems(bSoundOnly ? TArray<UNiagaraSystem*>() : TArray<UNiagaraSystem*>{ System });
		bOk &= Validate(Cue, Log);
		bOk &= Save(Blueprint, Log);
		Log.Add(bSoundOnly
			? FString::Printf(TEXT("%s: %s -> sound only (VFX in DW-307)"), Spec.Name, *Spec.Tag.ToString())
			: FString::Printf(TEXT("%s: %s -> %s (cap %.1f s, PLACEHOLDER until DW-307)"), Spec.Name, *Spec.Tag.ToString(), *System->GetName(),
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
