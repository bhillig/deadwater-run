// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DwPresentationSubsystem.generated.h"

/**
 * How abilities present themselves (Docs/Architecture/02 "Presentation tasks and instant mode").
 * Instant mode: every presentation task completes synchronously and gameplay cues are suppressed, so tests and the
 * headless simulator run whole fights in one call stack. Defaults to instant when the process can't render.
 * Presentation never decides an outcome: abilities change state only at commit, impact and end.
 */
UCLASS()
class DEADWATERRUN_API UDwPresentationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Deadwater|Presentation")
	bool IsInstant() const { return bInstant; }

	UFUNCTION(BlueprintCallable, Category = "Deadwater|Presentation")
	void SetInstant(bool bInInstant) { bInstant = bInInstant; }

	/** Game-speed setting (UDwGameUserSettings, U1): 1 = normal, 2 = twice as fast. Set at world start and on Apply. */
	UFUNCTION(BlueprintPure, Category = "Deadwater|Presentation")
	float GetPlaybackRate() const { return PlaybackRate; }

	UFUNCTION(BlueprintCallable, Category = "Deadwater|Presentation")
	void SetPlaybackRate(float Rate) { PlaybackRate = FMath::Clamp(Rate, 0.25f, 4.f); }

	/** Instant if there is no subsystem (e.g. a world without one): the safe default for logic. */
	static bool IsInstant(const UObject* WorldContext);
	static float GetPlaybackRate(const UObject* WorldContext);

private:
	bool bInstant = false;
	float PlaybackRate = 1.f;
};
