// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DwGameMode.generated.h"

/**
 * Game mode for L_Sea, the level that hosts the whole run (map, combat, shop, events are phases inside it).
 * DW-201 binds UDwRun::OnPhaseChanged here and starts/ends encounters. Until then it starts DA_GameData's dev fight
 * (DevEncounter + DevCrew) so the combat is playable from Play-in-Editor. See Docs/Architecture/16-Shell.md.
 */
UCLASS()
class DEADWATERRUN_API ADwGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADwGameMode();

	virtual void StartPlay() override;

	/** Starts DA_GameData's dev fight with a fresh seed. False (logged) if the data is missing. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Dev")
	bool StartDevFight(int32 Seed = -1);

	/** Off in the run flow (DW-201) and for levels that host no fight. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater|Dev")
	bool bStartDevFight = true;
};
