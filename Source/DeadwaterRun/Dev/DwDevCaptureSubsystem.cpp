// Copyright Brandon Hillig 2026

#include "Dev/DwDevCaptureSubsystem.h"

#include "Abilities/DwAbilitySystemComponent.h"
#include "Abilities/DwGameplayAbility.h"
#include "Board/DwBoard.h"
#include "Board/DwGridQueries.h"
#include "Presentation/DwCameraRig.h"
#include "Core/DwGameData.h"
#include "GameplayEffect.h"
#include "Combat/DwCombatInteraction.h"
#include "Combat/DwCombatRules.h"
#include "Combat/DwCombatSubsystem.h"
#include "Combat/DwCombatUnit.h"
#include "Combat/DwShip.h"
#include "Core/DwGameplayTags.h"
#include "Core/DwLog.h"
#include "Core/DwPlayerController.h"
#include "Engine/World.h"
#include "HighResScreenshot.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

bool UDwDevCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return Super::ShouldCreateSubsystem(Outer) && FCString::Strifind(FCommandLine::Get(), TEXT("-DwCapture=")) != nullptr;
#endif
}

bool UDwDevCaptureSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDwDevCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!FParse::Value(FCommandLine::Get(), TEXT("DwCapture="), Path) || Path.IsEmpty())
	{
		return;
	}
	float Delay = 4.f;
	FParse::Value(FCommandLine::Get(), TEXT("DwCaptureDelay="), Delay);
	FParse::Value(FCommandLine::Get(), TEXT("DwCaptureDemo="), Demo);
	UE_LOG(LogDw, Log, TEXT("Dev capture: %s in %.1f s (demo '%s')"), *Path, Delay, *Demo);
	InWorld.GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::StageDemo), FMath::Max(Delay - 1.f, 0.1f), false);
}

void UDwDevCaptureSubsystem::StageDemo()
{
	UWorld* World = GetWorld();
	const ADwPlayerController* PC = World ? Cast<ADwPlayerController>(World->GetFirstPlayerController()) : nullptr;
	UDwCombatInteraction* Interaction = PC ? PC->GetInteraction() : nullptr;
	UDwCombatSubsystem* Combat = World ? World->GetSubsystem<UDwCombatSubsystem>() : nullptr;

	// Close-ups for art review: -DwCaptureZoom=0.3 (fraction of the framed distance) -DwCaptureTile=2,1 (focus tile).
	float Zoom = 0.f;
	if (PC && PC->GetCameraRig() && Combat && Combat->GetBoard() && FParse::Value(FCommandLine::Get(), TEXT("DwCaptureZoom="), Zoom) && Zoom > 0.f)
	{
		FVector Focus = PC->GetCameraRig()->GetFocus();
		FString TileText, X, Y;
		if (FParse::Value(FCommandLine::Get(), TEXT("DwCaptureTile="), TileText, /*bShouldStopOnSeparator*/ false) && TileText.Split(TEXT(","), &X, &Y))
		{
			Focus = Combat->GetBoard()->TileToWorld(FIntPoint(FCString::Atoi(*X), FCString::Atoi(*Y)));
		}
		PC->GetCameraRig()->SnapTo(Focus, Zoom);
	}

	if (Interaction && Combat && Combat->IsActive())
	{
		const FDwCombatSnapshot Snapshot = Combat->GetSnapshot();
		auto FirstOf = [&Snapshot](EDwSide Side, const FGameplayTag& Tag)
		{
			for (const FDwUnitSnapshot& Unit : Snapshot.Units)
			{
				if (Unit.Side == Side && Unit.bAlive && (!Tag.IsValid() || Unit.Tags.HasTag(Tag)))
				{
					return Unit.Handle;
				}
			}
			return FDwUnitHandle();
		};
		if (Demo == TEXT("move"))
		{
			// The captain selected, hovering the farthest reachable tile: move range plus path preview.
			Interaction->SelectUnit(FirstOf(EDwSide::Player, DwTags::Unit_Role_Captain));
			const TArray<FIntPoint>& Reach = Interaction->GetReachable();
			if (!Reach.IsEmpty())
			{
				Interaction->HoverTile(Reach.Last());
			}
		}
		else if (Demo == TEXT("aim"))
		{
			// The sniper up in the nest (+2 range, sees over crates) aiming its musket: targets plus damage preview.
			const FDwUnitHandle Sniper = FirstOf(EDwSide::Player, DwTags::Unit_Archetype_Sniper);
			for (int32 Index = 0; Index < Snapshot.Grid.Tiles.Num(); ++Index)
			{
				const FDwTile& Tile = Snapshot.Grid.Tiles[Index];
				if (Tile.Kind == EDwTileKind::Nest && Tile.Owner == EDwSide::Player && !Snapshot.Grid.IsOccupied(Snapshot.Grid.ToTile(Index)))
				{
					Combat->MoveUnit(Sniper, Snapshot.Grid.ToTile(Index), /*bSnapActor*/ true);
					break;
				}
			}
			const FDwCombatSnapshot InNest = Combat->GetSnapshot();
			Interaction->SelectUnit(Sniper);
			const FDwUnitSnapshot* Unit = InNest.FindUnit(Sniper);
			if (Unit && Interaction->SelectAbility(FDwCombatQueries::FindAbility(*Unit, DwTags::Ability_Attack_Ranged)) && !Interaction->GetValidTargets().IsEmpty())
			{
				Interaction->HoverTile(Interaction->GetValidTargets()[0].Tile);
			}
		}
		else if (Demo == TEXT("enemy"))
		{
			// The player ends the turn and the crew AI (DW-106) plays the enemy side; Capture() takes a frame sequence.
			Combat->SubmitCommand(FDwCombatCommand::MakeEndTurn(EDwSide::Player));
			World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::CaptureEnemyTurn), 0.5f, false);
			return;
		}
		else if (Demo == TEXT("cannon"))
		{
			// The swordsman next to a cannon, aiming it: the enemy deck lights up.
			const FDwUnitHandle Gunner = FirstOf(EDwSide::Player, DwTags::Unit_Archetype_Swordsman);
			const ADwShip* Ship = Combat->GetShip(EDwSide::Player);
			if (Ship && !Ship->GetCannonTiles().IsEmpty())
			{
				Combat->MoveUnit(Gunner, Ship->GetCannonTiles()[0], /*bSnapActor*/ true);
			}
			Interaction->SelectUnit(Gunner);
			const FDwUnitSnapshot* Unit = Combat->GetSnapshot().FindUnit(Gunner);
			if (Unit && Interaction->SelectAbility(FDwCombatQueries::FindAbility(*Unit, DwTags::Ability_Cannon)))
			{
				Interaction->HoverTile(Combat->FindUnit(FirstOf(EDwSide::Enemy, FGameplayTag()))->GetTile());
			}
		}
		else if (Demo == TEXT("hit"))
		{
			// The sniper shoots the first enemy it can: floating damage text, HP bars and the spent action (DW-107 HUD).
			// The enemy sniper is also frozen (shared GE_Frozen), so a status chip with turns left and "Frozen!" show.
			if (const ADwCombatUnit* Frozen = Combat->FindUnit(FirstOf(EDwSide::Enemy, DwTags::Unit_Archetype_Sniper)))
			{
				UDwAbilitySystemComponent* Asc = Frozen->GetDwAbilitySystem();
				Asc->ApplyGameplayEffectToSelf(DwGas::GetSharedEffects().Frozen->GetDefaultObject<UGameplayEffect>(), 1.f, Asc->MakeEffectContext());
			}
			const FDwUnitHandle Sniper = FirstOf(EDwSide::Player, DwTags::Unit_Archetype_Sniper);
			for (int32 Index = 0; Index < Snapshot.Grid.Tiles.Num(); ++Index)
			{
				const FDwTile& Tile = Snapshot.Grid.Tiles[Index];
				if (Tile.Kind == EDwTileKind::Nest && Tile.Owner == EDwSide::Player && !Snapshot.Grid.IsOccupied(Snapshot.Grid.ToTile(Index)))
				{
					Combat->MoveUnit(Sniper, Snapshot.Grid.ToTile(Index), /*bSnapActor*/ true); // the nest sees over the crates
					break;
				}
			}
			Interaction->SelectUnit(Sniper);
			const FDwCombatSnapshot InNest = Combat->GetSnapshot();
			const FDwUnitSnapshot* Unit = InNest.FindUnit(Sniper);
			if (Unit && Interaction->SelectAbility(FDwCombatQueries::FindAbility(*Unit, DwTags::Ability_Attack_Ranged)) && !Interaction->GetValidTargets().IsEmpty())
			{
				Interaction->ClickTile(Interaction->GetValidTargets()[0].Tile);
			}
			// Capture while the label is still up (the shot resolves after the 0.5 s placeholder beat).
			World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::Capture), 0.8f, false);
			return;
		}
		else if (Demo == TEXT("endturn"))
		{
			// End the player's turn: the passive enemy side passes, so the banner for the next side is up (DW-107 HUD).
			Interaction->RequestEndTurn();
			World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::Capture), 0.4f, false);
			return;
		}
	}
	World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::Capture), 1.f, false);
}

void UDwDevCaptureSubsystem::CaptureEnemyTurn()
{
	// <file>-01.png, -02.png, ... while the enemy side plays EnemyTurns turns (the player passes in between); the frame
	// after the last one hands back to the player is also <file>.
	UDwCombatSubsystem* Combat = GetWorld()->GetSubsystem<UDwCombatSubsystem>();
	const bool bOver = !Combat || !Combat->IsActive() || Combat->GetOutcome() != EDwCombatOutcome::None;
	const bool bTurnDone = bOver || Combat->GetActiveSide() == EDwSide::Player;
	++EnemyFrame;
	FScreenshotRequest::RequestScreenshot(FPaths::GetBaseFilename(Path, false) + FString::Printf(TEXT("-%02d.png"), EnemyFrame), /*bShowUI*/ true, false);
	if (bTurnDone && !bOver && ++EnemyTurnsPlayed < EnemyTurns && EnemyFrame < MaxEnemyFrames)
	{
		Combat->SubmitCommand(FDwCombatCommand::MakeEndTurn(EDwSide::Player));
		GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::CaptureEnemyTurn), 0.7f, false);
		return;
	}
	const bool bDone = bOver || (bTurnDone && EnemyTurnsPlayed >= EnemyTurns);
	if (bDone || EnemyFrame >= MaxEnemyFrames)
	{
		UE_LOG(LogDw, Log, TEXT("Dev capture: enemy turn %s after %d frames"), bDone ? TEXT("finished") : TEXT("still running"), EnemyFrame);
		GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::Capture), 0.5f, false);
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::CaptureEnemyTurn), 0.7f, false);
}

void UDwDevCaptureSubsystem::Capture()
{
	FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
	UE_LOG(LogDw, Log, TEXT("Dev capture requested: %s"), *Path);
	// Give the renderer a moment to write the file, then quit.
	GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([]() { FPlatformMisc::RequestExit(false); }), 1.5f, false);
}
