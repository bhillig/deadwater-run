// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "DwGameUserSettings.generated.h"

/** Presentation speed (U1): only how fast animations and moves play, never the rules. */
UENUM(BlueprintType)
enum class EDwGameSpeed : uint8
{
	Normal,  // 1x
	Fast,    // 1.5x
	Fastest  // 2x
};

/** Tile highlight colours (U3). The palette assets themselves are DW-216 (UDwHighlightPalette). */
UENUM(BlueprintType)
enum class EDwHighlightPalette : uint8
{
	Default,
	ColorblindSafe
};

/** One audio volume slot (Docs/Architecture/15-Audio-VFX.md sound classes). */
UENUM(BlueprintType)
enum class EDwVolume : uint8
{
	Master,
	Music,
	Sfx,
	Ui
};

/**
 * Deadwater's settings (Docs/Architecture/16-Shell.md "UDwGameUserSettings"), saved to GameUserSettings.ini; runs are
 * never saved (D2). Display settings (resolution, window mode, vsync) come from UGameUserSettings.
 * Applied today: display (engine) and game speed (UDwPresentationSubsystem playback rate in every game world, also
 * at world start). Saved but not applied until their systems exist: volumes (audio, DW-114/DW-216), screen shake (no
 * camera shake yet), highlight palette (palette assets, DW-216).
 */
UCLASS(config = GameUserSettings, configdonotcheckdefaults)
class DEADWATERRUN_API UDwGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UDwGameUserSettings();

	/** The engine's settings object (GEngine->GetGameUserSettings()), or null if the project uses another class. */
	static UDwGameUserSettings* Get();

	/** 1.0 / 1.5 / 2.0. */
	static float GetPlaybackRate(EDwGameSpeed Speed);
	float GetPlaybackRate() const { return GetPlaybackRate(GameSpeed); }

	float GetVolume(EDwVolume Slot) const;
	void SetVolume(EDwVolume Slot, float Value);

	EDwGameSpeed GetGameSpeed() const { return GameSpeed; }
	void SetGameSpeed(EDwGameSpeed Speed) { GameSpeed = Speed; }
	bool IsScreenShakeEnabled() const { return bScreenShake; }
	void SetScreenShakeEnabled(bool bEnabled) { bScreenShake = bEnabled; }
	EDwHighlightPalette GetHighlightPalette() const { return HighlightPalette; }
	void SetHighlightPalette(EDwHighlightPalette Palette) { HighlightPalette = Palette; }

	/** Game speed into the presentation of World (or every game world when null). */
	void ApplyPresentation(UWorld* World = nullptr) const;

	// UGameUserSettings
	virtual void SetToDefaults() override;
	virtual void ApplySettings(bool bCheckForCommandLineOverrides) override;

private:
	UPROPERTY(Config) float MasterVolume = 1.f;
	UPROPERTY(Config) float MusicVolume = 0.8f;
	UPROPERTY(Config) float SfxVolume = 1.f;
	UPROPERTY(Config) float UiVolume = 1.f;
	UPROPERTY(Config) EDwGameSpeed GameSpeed = EDwGameSpeed::Normal;
	UPROPERTY(Config) bool bScreenShake = true;
	UPROPERTY(Config) EDwHighlightPalette HighlightPalette = EDwHighlightPalette::Default;
};
