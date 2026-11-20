// Copyright Brandon Hillig 2026

#include "Core/DwMenuGameMode.h"

#include "Core/DwGameplayTags.h"
#include "Core/DwPlayerController.h"
#include "Presentation/DwMusicSubsystem.h"
#include "Run/DwRunSubsystem.h"

ADwMenuGameMode::ADwMenuGameMode()
{
	PlayerControllerClass = ADwPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}

void ADwMenuGameMode::BeginPlay()
{
	Super::BeginPlay();
	// Back on the menu means the last run is over (16-Shell: quitting a run ends it; nothing was saved).
	if (UDwRunSubsystem* Runs = UDwRunSubsystem::Get(this))
	{
		Runs->ClearRun();
	}
	if (UDwMusicSubsystem* Music = UDwMusicSubsystem::Get(this))
	{
		Music->SetMusicState(DwTags::Music_Menu);
	}
}
