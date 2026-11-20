// Copyright Brandon Hillig 2026

#include "Shell/DwGameUserSettings.h"

#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Engine/Engine.h"
#include "Sound/SoundMix.h"
#include "Engine/World.h"
#include "Presentation/DwPresentationSubsystem.h"

UDwGameUserSettings::UDwGameUserSettings()
{
	SetToDefaults();
}

UDwGameUserSettings* UDwGameUserSettings::Get()
{
	return GEngine ? Cast<UDwGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

float UDwGameUserSettings::GetPlaybackRate(EDwGameSpeed Speed)
{
	switch (Speed)
	{
	case EDwGameSpeed::Fast:    return 1.5f;
	case EDwGameSpeed::Fastest: return 2.f;
	default:                    return 1.f;
	}
}

float UDwGameUserSettings::GetVolume(EDwVolume Slot) const
{
	switch (Slot)
	{
	case EDwVolume::Music: return MusicVolume;
	case EDwVolume::Sfx:   return SfxVolume;
	case EDwVolume::Ui:    return UiVolume;
	default:               return MasterVolume;
	}
}

void UDwGameUserSettings::SetVolume(EDwVolume Slot, float Value)
{
	const float Clamped = FMath::Clamp(Value, 0.f, 1.f);
	switch (Slot)
	{
	case EDwVolume::Music: MusicVolume = Clamped; break;
	case EDwVolume::Sfx:   SfxVolume = Clamped; break;
	case EDwVolume::Ui:    UiVolume = Clamped; break;
	default:               MasterVolume = Clamped; break;
	}
}

void UDwGameUserSettings::ApplyPresentation(UWorld* World) const
{
	auto Apply = [this](UWorld* Target)
	{
		if (UDwPresentationSubsystem* Presentation = Target ? Target->GetSubsystem<UDwPresentationSubsystem>() : nullptr)
		{
			Presentation->SetPlaybackRate(GetPlaybackRate());
		}
	};
	if (World)
	{
		Apply(World);
		return;
	}
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
			{
				Apply(Context.World());
			}
		}
	}
}

TArray<FDwVolumeOverride> UDwGameUserSettings::GetVolumeOverrides(const FDwAudioConfig& Audio) const
{
	// Mix adjusters are applied after the sound class tree has been resolved (FAudioDevice::UpdateSoundClassProperties),
	// so an adjuster on SC_Master only reaches SC_Music/SFX/UI when it applies to children.
	const TPair<USoundClass*, EDwVolume> Slots[] = {
		{ Audio.MasterClass, EDwVolume::Master }, { Audio.MusicClass, EDwVolume::Music },
		{ Audio.SfxClass, EDwVolume::Sfx }, { Audio.UiClass, EDwVolume::Ui } };
	TArray<FDwVolumeOverride> Overrides;
	for (const TPair<USoundClass*, EDwVolume>& Slot : Slots)
	{
		if (Slot.Key)
		{
			Overrides.Add({ Slot.Key, GetVolume(Slot.Value), Slot.Value == EDwVolume::Master });
		}
	}
	return Overrides;
}

void UDwGameUserSettings::ApplyAudio() const
{
	const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
	FAudioDeviceManager* Devices = GEngine ? GEngine->GetAudioDeviceManager() : nullptr;
	if (!GameData || !GameData->Audio.BaseMix || !Devices)
	{
		return; // -nosound, or no audio data yet
	}
	USoundMix* Mix = GameData->Audio.BaseMix;
	const TArray<FDwVolumeOverride> Overrides = GetVolumeOverrides(GameData->Audio);
	// Every device: PIE runs one per instance. The overrides persist on the device and apply whenever SM_Base is pushed.
	Devices->IterateOverAllDevices([Mix, &Overrides](Audio::FDeviceId, FAudioDevice* Device)
	{
		for (const FDwVolumeOverride& Override : Overrides)
		{
			Device->SetSoundMixClassOverride(Mix, Override.Class, Override.Volume, /*Pitch*/ 1.f, /*FadeInTime*/ 0.f, Override.bApplyToChildren);
		}
	});
}

void UDwGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	MasterVolume = 1.f;
	MusicVolume = 0.8f;
	SfxVolume = 1.f;
	UiVolume = 1.f;
	GameSpeed = EDwGameSpeed::Normal;
	bScreenShake = true;
	HighlightPalette = EDwHighlightPalette::Default;
}

void UDwGameUserSettings::ApplySettings(bool bCheckForCommandLineOverrides)
{
	Super::ApplySettings(bCheckForCommandLineOverrides);
	ApplyPresentation();
	ApplyAudio();
}
