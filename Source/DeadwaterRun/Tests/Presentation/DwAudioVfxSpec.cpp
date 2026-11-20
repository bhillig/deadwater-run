// Copyright Brandon Hillig 2026

// DW-114: GameplayCue bases, the audio cue map, the music set and subsystem, the settings mix overrides, and the
// placeholder cue content (Tools/CreateCombatContent.ps1). Engine sounds (VREditor clicks) stand in for DW-311 audio.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/DwAbilitySystemComponent.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Engine/GameInstance.h"
#include "Misc/DataValidation.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Presentation/DwAudioCueMap.h"
#include "Presentation/DwGameplayCueNotify_Burst.h"
#include "Presentation/DwGameplayCueNotify_Looping.h"
#include "Presentation/DwMusicSet.h"
#include "Presentation/DwMusicSubsystem.h"
#include "Presentation/DwPresentationSubsystem.h"
#include "Run/DwRun.h"
#include "Shell/DwGameUserSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWave.h"
#include "Tests/Combat/DwCombatTestFixture.h"
#include "Tests/Run/DwRunTestContent.h"
#include "UObject/UObjectIterator.h"

namespace DwAudioVfxTest
{
	const TCHAR* ClickPaths[] = {
		TEXT("/Engine/VREditor/Sounds/VR_click1.VR_click1"),
		TEXT("/Engine/VREditor/Sounds/VR_click2.VR_click2"),
		TEXT("/Engine/VREditor/Sounds/VR_click3.VR_click3") };

	/** The four DW-114 placeholder cues (Content/DeadwaterRun/VFX/Cues). */
	TArray<TPair<const TCHAR*, FGameplayTag>> Cues()
	{
		return { { TEXT("GC_Hit_Melee"), DwTags::GameplayCue_Hit_Melee }, { TEXT("GC_Cannon_Fire"), DwTags::GameplayCue_Cannon_Fire },
			{ TEXT("GC_Cannon_Impact"), DwTags::GameplayCue_Cannon_Impact }, { TEXT("GC_Unit_Death"), DwTags::GameplayCue_Unit_Death } };
	}

	UClass* LoadCue(const TCHAR* Name)
	{
		return LoadObject<UClass>(nullptr, *FString::Printf(TEXT("/Game/DeadwaterRun/VFX/Cues/%s.%s_C"), Name, Name));
	}

	int32 CountNiagara(const UWorld* World)
	{
		int32 Count = 0;
		for (TObjectIterator<UNiagaraComponent> It; It; ++It)
		{
			Count += It->GetWorld() == World && IsValid(*It) ? 1 : 0;
		}
		return Count;
	}
}

BEGIN_DEFINE_SPEC(FDwAudioVfxSpec, "DeadwaterRun.Presentation.AudioVfx",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	FDwCombatFixture F;
	TArray<USoundBase*> Clicks;
	TArray<UObject*> Rooted;
	/** Every cue sound the presentation subsystem started: (tag, section, sound). */
	TArray<TTuple<FGameplayTag, EDwCueSoundSection, USoundBase*>> Played;
	FDelegateHandle PlayedHandle;

	template <typename T>
	T* Root(T* Object)
	{
		Object->AddToRoot();
		Rooted.Add(Object);
		return Object;
	}

	/** A cue map with Tag's Start = the three clicks (one layer), and an optional second layer. */
	UDwAudioCueMap* MakeCueMap(const FGameplayTag& Tag, int32 Layers = 1)
	{
		UDwAudioCueMap* Map = Root(NewObject<UDwAudioCueMap>());
		FDwCueSound& Cue = Map->Cues.Add(Tag);
		for (int32 Index = 0; Index < Layers; ++Index)
		{
			FDwCueSoundLayer& Layer = Cue.Start.AddDefaulted_GetRef();
			Layer.Sounds.Append(Clicks);
			Layer.PitchMin = 0.9f;
			Layer.PitchMax = 1.1f;
		}
		return Map;
	}

	UDwPresentationSubsystem* Presentation() const { return F.World->GetSubsystem<UDwPresentationSubsystem>(); }

	void ListenForSounds()
	{
		PlayedHandle = Presentation()->OnCueSoundPlayed.AddLambda([this](const FGameplayTag& Tag, EDwCueSoundSection Section, USoundBase* Sound)
		{
			Played.Add(MakeTuple(Tag, Section, Sound));
		});
	}

	/** Player crew 0 next to enemy 0, both with real melee (GameplayCue.Hit.Melee on each hit). */
	bool StartMeleeFight()
	{
		F.UseRealActions();
		if (!TestTrue(TEXT("Started"), F.Start()))
		{
			return false;
		}
		F.Combat->MoveUnit(FDwCombatFixture::CrewHandle(0), FIntPoint(8, 0)); // next to enemy 0 at (9,0)
		return true;
	}

	/** Cue parameters as the game sends them: the target's location, the attacker as instigator. */
	FGameplayCueParameters CueAt(const AActor& Target) const
	{
		FGameplayCueParameters Cue;
		Cue.Location = Target.GetActorLocation();
		return Cue;
	}

END_DEFINE_SPEC(FDwAudioVfxSpec)

void FDwAudioVfxSpec::Define()
{
	using namespace DwAudioVfxTest;

	BeforeEach([this]()
	{
		Clicks.Reset();
		for (const TCHAR* Path : ClickPaths)
		{
			USoundBase* Sound = LoadObject<USoundBase>(nullptr, Path);
			TestNotNull(Path, Sound);
			Clicks.Add(Sound);
		}
		F.Create();
		Played.Reset();
		ListenForSounds();
	});

	AfterEach([this]()
	{
		Presentation()->OnCueSoundPlayed.Remove(PlayedHandle);
		F.Destroy();
		for (UObject* Object : Rooted)
		{
			Object->RemoveFromRoot();
		}
		Rooted.Reset();
	});

	Describe("Cue map", [this]()
	{
		It("never picks the same variation twice in a row, and uses them all", [this]()
		{
			FRandomStream Random(7);
			int32 Previous = INDEX_NONE;
			TSet<int32> Seen;
			bool bRepeated = false;
			for (int32 Pick = 0; Pick < 2000; ++Pick)
			{
				const int32 Index = UDwAudioCueMap::PickVariation(3, Previous, Random);
				bRepeated |= Index == Previous;
				Seen.Add(Index);
				Previous = Index;
			}
			TestFalse(TEXT("No immediate repeat"), bRepeated);
			TestEqual(TEXT("All three used"), Seen.Num(), 3);
			TestTrue(TEXT("Only ever 0..2"), Seen.Contains(0) && Seen.Contains(1) && Seen.Contains(2));
			TestEqual(TEXT("One variation repeats"), UDwAudioCueMap::PickVariation(1, 0, Random), 0);
			TestEqual(TEXT("No variations"), UDwAudioCueMap::PickVariation(0, INDEX_NONE, Random), INDEX_NONE);
			const int32 FromBadPrevious = UDwAudioCueMap::PickVariation(2, 5, Random);
			TestTrue(TEXT("A stale previous index is ignored"), FromBadPrevious == 0 || FromBadPrevious == 1);
		});

		It("finds a cue by its tag, else by its nearest parent", [this]()
		{
			const FGameplayTag Hit = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Hit"));
			UDwAudioCueMap* Map = MakeCueMap(Hit);
			FGameplayTag Matched;
			TestTrue(TEXT("Hit.Melee falls back to Hit"), Map->FindCue(DwTags::GameplayCue_Hit_Melee, &Matched) == Map->Cues.Find(Hit));
			TestEqual(TEXT("Matched the parent"), Matched, Hit);
			Map->Cues.Add(DwTags::GameplayCue_Hit_Melee);
			TestTrue(TEXT("An exact entry wins"), Map->FindCue(DwTags::GameplayCue_Hit_Melee) == Map->Cues.Find(DwTags::GameplayCue_Hit_Melee));
			TestNull(TEXT("Cannon has none"), Map->FindCue(DwTags::GameplayCue_Cannon_Fire));
		});

#if WITH_EDITOR
		It("rejects entries without sounds and keys that are not cues", [this]()
		{
			UDwAudioCueMap* Map = MakeCueMap(DwTags::GameplayCue_Hit_Melee);
			FDataValidationContext Valid;
			TestEqual(TEXT("Three clicks are valid"), Map->IsDataValid(Valid), EDataValidationResult::Valid);
			Map->Cues.FindChecked(DwTags::GameplayCue_Hit_Melee).Start.AddDefaulted();
			FDataValidationContext Empty;
			TestEqual(TEXT("An empty layer is invalid"), Map->IsDataValid(Empty), EDataValidationResult::Invalid);
			UDwAudioCueMap* Wrong = MakeCueMap(DwTags::Music_Map);
			FDataValidationContext NotCue;
			TestEqual(TEXT("Music.Map is not a cue"), Wrong->IsDataValid(NotCue), EDataValidationResult::Invalid);
		});
#endif
	});

	Describe("Instant mode", [this]()
	{
		It("spawns no Niagara and plays no sound for a real melee hit or any placeholder cue", [this]()
		{
			for (const TPair<const TCHAR*, FGameplayTag>& Cue : Cues())
			{
				TestNotNull(Cue.Key, LoadCue(Cue.Key));
			}
			Presentation()->SetCueMapOverride(MakeCueMap(DwTags::GameplayCue_Hit_Melee));
			if (!StartMeleeFight())
			{
				return;
			}
			TestTrue(TEXT("The fight is instant"), UDwPresentationSubsystem::IsInstant(F.World));
			TestTrue(TEXT("ASCs suppress cues"), F.Asc(F.EnemyHandle(0))->bSuppressGameplayCues);
			TestEqual(TEXT("Melee"), F.Use(FDwCombatFixture::CrewHandle(0), F.Melee, F.EnemyHandle(0)), EDwCommandResult::Accepted);
			TestEqual(TEXT("The hit landed"), F.Health(F.EnemyHandle(0)), 75.f);

			// Every placeholder cue, even past the ASC's suppression: the cue bases check instant mode themselves.
			ADwCombatUnit* Enemy = F.Unit(F.EnemyHandle(1));
			UDwAbilitySystemComponent* Asc = Enemy->GetDwAbilitySystem();
			for (const bool bSuppressed : { true, false })
			{
				Asc->bSuppressGameplayCues = bSuppressed;
				for (const TPair<const TCHAR*, FGameplayTag>& Cue : Cues())
				{
					Asc->ExecuteGameplayCue(Cue.Value, CueAt(*Enemy));
				}
			}
			TestEqual(TEXT("No Niagara components"), CountNiagara(F.World), 0);
			TestEqual(TEXT("No cue sounds"), Presentation()->GetCueSoundsPlayed(), 0);
			TestEqual(TEXT("Nothing broadcast"), Played.Num(), 0);
			TestEqual(TEXT("No cue reached the Niagara spawn"), Presentation()->GetCuesPresented(), 0);
		});
	});

	Describe("Presented", [this]()
	{
		It("presents GC_Hit_Melee's effects and its DA_AudioCueMap sounds, never the same click twice in a row", [this]()
		{
			if (!TestNotNull(TEXT("GC_Hit_Melee"), LoadCue(TEXT("GC_Hit_Melee"))) || !StartMeleeFight())
			{
				return;
			}
			Presentation()->SetCueMapOverride(MakeCueMap(DwTags::GameplayCue_Hit_Melee, /*Layers*/ 2));
			Presentation()->SetInstant(false); // as when a player watches
			ADwCombatUnit* Enemy = F.Unit(F.EnemyHandle(0));
			UDwAbilitySystemComponent* Asc = Enemy->GetDwAbilitySystem();
			Asc->bSuppressGameplayCues = false;
			for (int32 Hit = 0; Hit < 8; ++Hit)
			{
				Asc->ExecuteGameplayCue(DwTags::GameplayCue_Hit_Melee, CueAt(*Enemy));
			}
			TestEqual(TEXT("Two layers per hit"), Played.Num(), 16);
			bool bAllMelee = true;
			bool bRepeated = false;
			for (int32 Index = 0; Index < Played.Num(); ++Index)
			{
				bAllMelee &= Played[Index].Get<0>() == DwTags::GameplayCue_Hit_Melee && Played[Index].Get<1>() == EDwCueSoundSection::Start
					&& Clicks.Contains(Played[Index].Get<2>());
				// Same layer of consecutive hits: indices two apart.
				bRepeated |= Index >= 2 && Played[Index].Get<2>() == Played[Index - 2].Get<2>();
			}
			TestTrue(TEXT("Every sound is a mapped click for Hit.Melee"), bAllMelee);
			TestFalse(TEXT("A layer never repeats its last click"), bRepeated);
			TestEqual(TEXT("Counted"), Presentation()->GetCueSoundsPlayed(), 16);

			// The VFX: each hit reached the engine's spawn (a headless world has no view, so Niagara culls the system itself;
			// the rendered proof is Tools/Capture.ps1 -Demo cues).
			TestEqual(TEXT("Every hit presented its effects"), Presentation()->GetCuesPresented(), 8);
			TestTrue(TEXT("Every spawned effect is under the lifetime cap"), Presentation()->GetNumCappedEffects() <= CountNiagara(F.World));
		});

		It("a cue without a map entry plays only its VFX", [this]()
		{
			if (!TestNotNull(TEXT("GC_Cannon_Impact"), LoadCue(TEXT("GC_Cannon_Impact"))) || !TestTrue(TEXT("Started"), F.Start()))
			{
				return;
			}
			Presentation()->SetCueMapOverride(MakeCueMap(DwTags::GameplayCue_Hit_Melee));
			Presentation()->SetInstant(false);
			ADwShip* Ship = F.Combat->GetShip(EDwSide::Enemy);
			UAbilitySystemComponent* Asc = Ship->GetDwAbilitySystem();
			Asc->bSuppressGameplayCues = false;
			Asc->ExecuteGameplayCue(DwTags::GameplayCue_Cannon_Impact, CueAt(*Ship));
			TestEqual(TEXT("No sound"), Played.Num(), 0);
			TestEqual(TEXT("Splinters presented"), Presentation()->GetCuesPresented(), 1);
		});

		It("a status loop plays Start on apply, Loop once while active, Stop on removal; nothing in instant mode", [this]()
		{
			if (!TestTrue(TEXT("Started"), F.Start()))
			{
				return;
			}
			UDwAudioCueMap* Map = MakeCueMap(DwTags::GameplayCue_Status_Fear);
			FDwCueSound& Fear = Map->Cues.FindChecked(DwTags::GameplayCue_Status_Fear);
			Fear.Loop = Fear.Start;
			Fear.Stop = Fear.Start;
			Presentation()->SetCueMapOverride(Map);
			ADwCombatUnit* Target = F.Unit(F.EnemyHandle(0));
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ADwGameplayCueNotify_Looping* Notify = F.World->SpawnActor<ADwGameplayCueNotify_Looping>(Params);
			Notify->GameplayCueTag = DwTags::GameplayCue_Status_Fear;
			const FGameplayCueParameters Cue = CueAt(*Target);

			// Instant (the fixture's default): silent.
			Notify->HandleGameplayCue(Target, EGameplayCueEvent::OnActive, Cue);
			Notify->HandleGameplayCue(Target, EGameplayCueEvent::WhileActive, Cue);
			Notify->HandleGameplayCue(Target, EGameplayCueEvent::Removed, Cue);
			TestEqual(TEXT("Instant: no sounds"), Played.Num(), 0);

			Presentation()->SetInstant(false);
			Notify->HandleGameplayCue(Target, EGameplayCueEvent::OnActive, Cue);
			Notify->HandleGameplayCue(Target, EGameplayCueEvent::WhileActive, Cue);
			Notify->HandleGameplayCue(Target, EGameplayCueEvent::WhileActive, Cue); // relevancy again: no second loop
			Notify->HandleGameplayCue(Target, EGameplayCueEvent::Removed, Cue);
			TArray<EDwCueSoundSection> Sections;
			for (const TTuple<FGameplayTag, EDwCueSoundSection, USoundBase*>& Sound : Played)
			{
				Sections.Add(Sound.Get<1>());
			}
			const TArray<EDwCueSoundSection> Expected = { EDwCueSoundSection::Start, EDwCueSoundSection::Loop, EDwCueSoundSection::Stop };
			TestTrue(FString::Printf(TEXT("Start, Loop, Stop (got %d sounds)"), Sections.Num()), Sections == Expected);
			TestEqual(TEXT("No loop left playing"), Notify->GetNumLoopSounds(), 0);
		});
	});

	Describe("Music", [this]()
	{
		It("decides crossfades, stingers and silence from the music set", [this]()
		{
			UDwMusicSet* Set = Root(NewObject<UDwMusicSet>());
			Set->CrossfadeSeconds = 2.f;
			Set->StingerFadeOutSeconds = 0.25f;
			Set->Tracks.Add(DwTags::Music_Map).Sound = Clicks[0];
			FDwMusicTrack& Victory = Set->Tracks.Add(DwTags::Music_Victory);
			Victory.Sound = Clicks[1];
			Victory.bLoop = false;

			const FDwMusicTransition Same = UDwMusicSubsystem::DecideTransition(Set, DwTags::Music_Map, DwTags::Music_Map);
			TestFalse(TEXT("Same state: no change"), Same.bChanged);
			const FDwMusicTransition ToMap = UDwMusicSubsystem::DecideTransition(Set, DwTags::Music_Menu, DwTags::Music_Map);
			TestTrue(TEXT("Map: crossfade"), ToMap.bChanged && ToMap.Track == Set->FindTrack(DwTags::Music_Map) && ToMap.FadeInSeconds == 2.f
				&& ToMap.FadeOutSeconds == 2.f);
			const FDwMusicTransition ToVictory = UDwMusicSubsystem::DecideTransition(Set, DwTags::Music_Map, DwTags::Music_Victory);
			TestTrue(TEXT("Victory: stinger hits at once over a short fade-out"), ToVictory.Track && ToVictory.FadeInSeconds == 0.f
				&& ToVictory.FadeOutSeconds == 0.25f);
			const FDwMusicTransition ToBoss = UDwMusicSubsystem::DecideTransition(Set, DwTags::Music_Map, DwTags::Music_Boss);
			TestTrue(TEXT("No track: silence"), ToBoss.bChanged && !ToBoss.Track);
			TestEqual(TEXT("Kraken fight"), UDwMusicSubsystem::GetCombatState(true), FGameplayTag(DwTags::Music_Boss));
			TestEqual(TEXT("Ship fight"), UDwMusicSubsystem::GetCombatState(false), FGameplayTag(DwTags::Music_Combat));
		});

		It("follows the run's phases, fights and Kraken phases; an intro plays before its loop", [this]()
		{
			UDwMusicSet* Set = Root(NewObject<UDwMusicSet>());
			Set->PhaseStates = { { EDwRunPhase::Map, DwTags::Music_Map }, { EDwRunPhase::Defeat, DwTags::Music_Defeat } };
			FDwMusicTrack& Map = Set->Tracks.Add(DwTags::Music_Map);
			Map.Intro = Clicks[2];
			Map.Sound = Clicks[0];
			Set->Tracks.Add(DwTags::Music_Combat).Sound = Clicks[1];
			FDwMusicTrack& Defeat = Set->Tracks.Add(DwTags::Music_Defeat);
			Defeat.Sound = Clicks[2];
			Defeat.bLoop = false;
			// Not initialized: no world, so no audio is started; the state machine runs the same.
			UGameInstance* GameInstance = Root(NewObject<UGameInstance>());
			UDwMusicSubsystem* Music = NewObject<UDwMusicSubsystem>(GameInstance);
			Music->SetMusicSetOverride(Set);

			DwRunTest::FContent C;
			C.Create();
			UDwRun* Run = Root(NewObject<UDwRun>());
			Run->Initialize(*C.GameData, *C.Captain, 7, C.Pool());
			Music->BindRun(Run);
			TestEqual(TEXT("A new run is on the map"), Music->GetMusicState(), FGameplayTag(DwTags::Music_Map));
			TestEqual(TEXT("The map's intro first"), Music->GetCurrentSound(), Clicks[2]);
			TestFalse(TEXT("Asking again changes nothing"), Music->SetMusicState(DwTags::Music_Map));

			Music->BindCombat(F.Combat);
			if (TestTrue(TEXT("Fight started"), F.Start()))
			{
				TestEqual(TEXT("A ship fight plays combat music"), Music->GetMusicState(), FGameplayTag(DwTags::Music_Combat));
				TestEqual(TEXT("Combat track"), Music->GetCurrentSound(), Clicks[1]);
				FDwCombatEvent Enraged;
				Enraged.Type = EDwCombatEventType::PhaseChanged;
				Enraged.Tag = DwTags::Music_Boss_Enraged;
				F.Combat->OnCombatEvent.Broadcast(Enraged);
				TestEqual(TEXT("A Kraken phase switches to its state"), Music->GetMusicState(), FGameplayTag(DwTags::Music_Boss_Enraged));
				TestNull(TEXT("No enraged track in this set: silence"), Music->GetCurrentSound());
			}

			TestTrue(TEXT("Abandoned"), Run->Abandon());
			TestEqual(TEXT("Defeat stinger"), Music->GetMusicState(), FGameplayTag(DwTags::Music_Defeat));
			TestEqual(TEXT("Stinger sound"), Music->GetCurrentSound(), Clicks[2]);
			Music->BindRun(nullptr);
			Music->BindCombat(nullptr);
			C.Destroy();
		});
	});

	Describe("Mix", [this]()
	{
		It("sends the four volumes as SM_Base overrides; only Master applies to its children", [this]()
		{
			UDwGameUserSettings* Settings = Root(NewObject<UDwGameUserSettings>());
			Settings->SetVolume(EDwVolume::Master, 0.5f);
			Settings->SetVolume(EDwVolume::Music, 0.25f);
			Settings->SetVolume(EDwVolume::Sfx, 0.75f);
			Settings->SetVolume(EDwVolume::Ui, 0.f);
			FDwAudioConfig Audio;
			Audio.MasterClass = Root(NewObject<USoundClass>());
			Audio.MusicClass = Root(NewObject<USoundClass>());
			Audio.SfxClass = Root(NewObject<USoundClass>());
			Audio.UiClass = Root(NewObject<USoundClass>());
			const TArray<FDwVolumeOverride> Overrides = Settings->GetVolumeOverrides(Audio);
			if (!TestEqual(TEXT("Four overrides"), Overrides.Num(), 4))
			{
				return;
			}
			TestTrue(TEXT("Master 0.5, to children"), Overrides[0].Class == Audio.MasterClass && Overrides[0].Volume == 0.5f && Overrides[0].bApplyToChildren);
			TestTrue(TEXT("Music 0.25"), Overrides[1].Class == Audio.MusicClass && Overrides[1].Volume == 0.25f && !Overrides[1].bApplyToChildren);
			TestTrue(TEXT("SFX 0.75"), Overrides[2].Class == Audio.SfxClass && Overrides[2].Volume == 0.75f && !Overrides[2].bApplyToChildren);
			TestTrue(TEXT("UI 0"), Overrides[3].Class == Audio.UiClass && Overrides[3].Volume == 0.f && !Overrides[3].bApplyToChildren);
			Audio.UiClass = nullptr;
			TestEqual(TEXT("A missing class is skipped"), Settings->GetVolumeOverrides(Audio).Num(), 3);
		});
	});

	Describe("Content", [this]()
	{
		It("DA_GameData.Audio holds the cue map, music set, SM_Base/SM_Pause and the four classes under SC_Master", [this]()
		{
			const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
			if (!TestNotNull(TEXT("DA_GameData"), GameData))
			{
				return;
			}
			const FDwAudioConfig& Audio = GameData->Audio;
			TestTrue(TEXT("DA_AudioCueMap"), Audio.CueMap && Audio.CueMap->GetName() == TEXT("DA_AudioCueMap"));
			TestTrue(TEXT("DA_MusicSet"), Audio.MusicSet && Audio.MusicSet->GetName() == TEXT("DA_MusicSet"));
			TestTrue(TEXT("SM_Base"), Audio.BaseMix && Audio.BaseMix->GetName() == TEXT("SM_Base"));
			TestTrue(TEXT("SM_Pause"), Audio.PauseMix && Audio.PauseMix->GetName() == TEXT("SM_Pause"));
			TestTrue(TEXT("SC_Master is the root"), Audio.MasterClass && !Audio.MasterClass->ParentClass);
			for (const USoundClass* Child : { Audio.MusicClass.Get(), Audio.SfxClass.Get(), Audio.UiClass.Get() })
			{
				TestTrue(FString::Printf(TEXT("%s under SC_Master"), *GetNameSafe(Child)), Child && Child->ParentClass == Audio.MasterClass);
				// The audio device walks the tree through ChildClasses: without the link Master's volume never reaches the child.
				TestTrue(FString::Printf(TEXT("SC_Master lists %s as a child"), *GetNameSafe(Child)),
					Audio.MasterClass && Audio.MasterClass->ChildClasses.Contains(Child));
			}
#if WITH_EDITOR
			FDataValidationContext CueMapContext;
			TestTrue(TEXT("DA_AudioCueMap valid"), Audio.CueMap && Audio.CueMap->IsDataValid(CueMapContext) != EDataValidationResult::Invalid);
			FDataValidationContext MusicContext;
			TestTrue(TEXT("DA_MusicSet valid"), Audio.MusicSet && Audio.MusicSet->IsDataValid(MusicContext) != EDataValidationResult::Invalid);
#endif
			TestEqual(TEXT("The map phase plays map music"), Audio.MusicSet ? Audio.MusicSet->GetPhaseState(EDwRunPhase::Map) : FGameplayTag(), FGameplayTag(DwTags::Music_Map));
		});

		It("each placeholder cue is a Dw burst with its tag, a Niagara system and no sounds of its own", [this]()
		{
			for (const TPair<const TCHAR*, FGameplayTag>& Spec : DwAudioVfxTest::Cues())
			{
				const UClass* Class = DwAudioVfxTest::LoadCue(Spec.Key);
				const UDwGameplayCueNotify_Burst* Cue = Class ? Cast<UDwGameplayCueNotify_Burst>(Class->GetDefaultObject()) : nullptr;
				if (!TestNotNull(Spec.Key, Cue))
				{
					continue;
				}
				TestEqual(FString::Printf(TEXT("%s tag"), Spec.Key), Cue->GameplayCueTag, Spec.Value);
				const TArray<UNiagaraSystem*> Systems = Cue->GetBurstNiagaraSystems();
				TestTrue(FString::Printf(TEXT("%s has Niagara"), Spec.Key), !Systems.IsEmpty() && !Systems.Contains(nullptr));
				TestEqual(FString::Printf(TEXT("%s has no asset sounds"), Spec.Key), Cue->GetNumAssetSounds(), 0);
				TestTrue(FString::Printf(TEXT("%s is bounded"), Spec.Key), Cue->MaxEffectSeconds > 0.f);
#if WITH_EDITOR
				FDataValidationContext Context;
				TestTrue(FString::Printf(TEXT("%s valid"), Spec.Key), static_cast<const UObject*>(Cue)->IsDataValid(Context) != EDataValidationResult::Invalid);
#endif
			}
		});

		It("the sound-only cues (Kraken.Appear/Death/TentacleCut, Attack.Miss) are Dw bursts with their tag and no Niagara or sounds of their own", [this]()
		{
			const TPair<const TCHAR*, FGameplayTag> SoundOnly[] = {
				{ TEXT("GC_Kraken_Appear"), DwTags::GameplayCue_Kraken_Appear }, { TEXT("GC_Kraken_Death"), DwTags::GameplayCue_Kraken_Death },
				{ TEXT("GC_Kraken_TentacleCut"), DwTags::GameplayCue_Kraken_TentacleCut }, { TEXT("GC_Attack_Miss"), DwTags::GameplayCue_Attack_Miss } };
			for (const TPair<const TCHAR*, FGameplayTag>& Spec : SoundOnly)
			{
				const UClass* Class = DwAudioVfxTest::LoadCue(Spec.Key);
				const UDwGameplayCueNotify_Burst* Cue = Class ? Cast<UDwGameplayCueNotify_Burst>(Class->GetDefaultObject()) : nullptr;
				if (!TestNotNull(Spec.Key, Cue))
				{
					continue;
				}
				TestEqual(FString::Printf(TEXT("%s tag"), Spec.Key), Cue->GameplayCueTag, Spec.Value);
				TestTrue(FString::Printf(TEXT("%s has no Niagara yet (DW-307)"), Spec.Key), Cue->GetBurstNiagaraSystems().IsEmpty());
				TestEqual(FString::Printf(TEXT("%s has no asset sounds"), Spec.Key), Cue->GetNumAssetSounds(), 0);
#if WITH_EDITOR
				FDataValidationContext Context;
				TestTrue(FString::Printf(TEXT("%s valid"), Spec.Key), static_cast<const UObject*>(Cue)->IsDataValid(Context) != EDataValidationResult::Invalid);
#endif
			}
		});
	});
}

#endif
