// Copyright Brandon Hillig 2026

#include "Shell/DwGameUserSettings.h"

#include "Engine/Engine.h"
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
}
