// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DwDevCaptureSubsystem.generated.h"

/**
 * Dev-only proof capture (never created in Shipping): with -DwCapture=<file.png> on the command line, the game waits
 * -DwCaptureDelay seconds (default 4), optionally stages the interaction (-DwCaptureDemo=move | aim | cannon | enemy), writes a
 * screenshot with the HUD, and quits. Lets agents produce reviewable screenshots headless (Tools/Capture.ps1) when the
 * editor MCP is unavailable. DW-111's cheat manager may absorb it.
 */
UCLASS()
class DEADWATERRUN_API UDwDevCaptureSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void StageDemo();
	void Capture();
	/** Demo "enemy": one frame every 0.7 s while the enemy side plays two turns (the player passes), then the final capture. */
	void CaptureEnemyTurn();

	FString Path;
	FString Demo;
	FTimerHandle Timer;
	int32 EnemyFrame = 0;
	int32 EnemyTurnsPlayed = 0;
	static constexpr int32 EnemyTurns = 2;
	static constexpr int32 MaxEnemyFrames = 40;
};
