// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DwDevCaptureSubsystem.generated.h"

/**
 * Dev-only proof capture (never created in Shipping): with -DwCapture=<file.png> on the command line, the game waits
 * -DwCaptureDelay seconds (default 4), optionally stages the interaction (-DwCaptureDemo=move | aim | cannon | enemy | run | voyage | boss ...), writes a
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
	/**
	 * Demo "run" (needs -DwCaptain=): -01 the map at Set Sail; sail to the first route, -02 fight 1; the captain is hurt
	 * to 60 HP, the sniper dies and the captain kills every enemy; -03 rewards, -04 level-up, -05 back on the map; sail
	 * on, final frame fight 2.
	 */
	void RunDemoStep();
	/**
	 * Demo "voyage" (needs -DwCaptain=; DW-202/211): plays a whole generated chart through the run screens. A frame of the
	 * map at every stop and of the first rewards, level-up, shop and event screens; fights are won by the captain;
	 * routes rotate between the choices. At the end of the chart: the map with only the Kraken ahead, then the boss fight
	 * (BossFightStep); the final frame is the Victory run summary.
	 * Demo "boss" (DW-209) plays the same voyage but only captures from the last stop on: -01 the map with only the Kraken,
	 * -02 the Kraken fight with the sniper aiming, -03 after the Kraken's Tentacle Slam; final frame the Victory summary.
	 */
	void VoyageDemoStep();
	/**
	 * The boss fight inside the voyage: settle; the sniper on the rail aims its musket at the Kraken (frame); it fires and
	 * the player ends the turn, so the Kraken slams back (frame); then the captain kills the body (VictoryBoss).
	 */
	void BossFightStep();
	/**
	 * Demo "shell" (DW-210; start in L_MainMenu with -Map MainMenu): presses the real screens' buttons. L_MainMenu: -01 main
	 * menu, -02 settings, -03 captain select, -04 a captain picked, Set Sail. L_Sea: -05 the map, -06 pause, -07 the abandon
	 * confirm, -08 the summary, -09 New Run's captain select, -10 the fresh run's map; then pause, abandon, Main Menu: final frame
	 * the main menu again (the run cleared).
	 */
	void ShellDemoStep();
	/** Kills every living enemy with the player captain as the instigator (kill XP), then ends the player turn. */
	void WinFightByCaptain() const;
	void CaptureFrame(int32 Frame) const;

	FString Path;
	FString Demo;
	FTimerHandle Timer;
	int32 RunStep = 0;
	int32 VoyageFrame = 0;
	int32 VoyageStops = 0;
	/** The current screen is captured; act on it next step. */
	bool bVoyageCaptured = false;
	bool bVoyageFightStaged = false;
	int32 BossStep = 0;
	TSet<uint8> VoyageScreensSeen;
	int32 EnemyFrame = 0;
	int32 EnemyTurnsPlayed = 0;
	static constexpr int32 EnemyTurns = 2;
	static constexpr int32 MaxEnemyFrames = 40;
};
