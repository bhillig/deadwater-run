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
#include "Abilities/DwAttributeSets.h"
#include "Engine/World.h"
#include "Run/DwRun.h"
#include "Run/DwRunSubsystem.h"
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

	if (Demo == TEXT("run"))
	{
		RunDemoStep();
		return;
	}
	if (Demo == TEXT("voyage"))
	{
		VoyageDemoStep();
		return;
	}

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

void UDwDevCaptureSubsystem::CaptureFrame(int32 Frame) const
{
	FScreenshotRequest::RequestScreenshot(FPaths::GetBaseFilename(Path, false) + FString::Printf(TEXT("-%02d.png"), Frame), /*bShowUI*/ true, false);
}

void UDwDevCaptureSubsystem::RunDemoStep()
{
	UWorld* World = GetWorld();
	const UDwRunSubsystem* Runs = UDwRunSubsystem::Get(World);
	UDwRun* Run = Runs ? Runs->GetRun() : nullptr;
	UDwCombatSubsystem* Combat = World->GetSubsystem<UDwCombatSubsystem>();
	auto Next = [this, World](float Seconds)
	{
		World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::RunDemoStep), Seconds, false);
	};
	auto Sail = [Run]()
	{
		// The next fight: a fight route if offered, shops and events on the way left at once.
		for (int32 Step = 0; Step < 20 && Run->GetPhase() == EDwRunPhase::Map; ++Step)
		{
			const TArray<int32> Choices = Run->GetChoices();
			const int32* Fight = Choices.FindByPredicate([Run](int32 Index) { return Run->GetState().Map.Nodes[Index].Type == EDwNodeType::Fight; });
			if (Choices.IsEmpty() || !Run->ChooseNode(Fight ? *Fight : Choices[0]))
			{
				return false;
			}
			if (Run->GetPhase() == EDwRunPhase::Shop) { Run->LeaveShop(); }
			if (Run->GetPhase() == EDwRunPhase::Event) { Run->LeaveEvent(); }
		}
		return Run->GetPhase() == EDwRunPhase::Combat;
	};
	if (!Run || !Combat)
	{
		UE_LOG(LogDw, Error, TEXT("Dev capture 'run': no run (start the game with -DwCaptain=<name>)."));
		Capture();
		return;
	}

	switch (RunStep++)
	{
	case 0: // Set Sail: the readout on the map
		CaptureFrame(1);
		Next(0.5f);
		return;
	case 1:
		UE_CLOG(!Sail(), LogDw, Error, TEXT("Dev capture 'run': could not sail to the first route."));
		Next(2.5f); // spawn and settle
		return;
	case 2: // fight 1, then the captain takes a beating, the sniper falls and the captain cuts down the enemy crew
	{
		CaptureFrame(2);
		const FGuid CaptainId = Run->GetState().Crew[0].Id;
		const FGuid SniperId = Run->GetState().Crew.Num() > 2 ? Run->GetState().Crew[2].Id : FGuid();
		const TSubclassOf<UGameplayEffect> Kill = DwGas::GetSharedEffects().Kill;
		UDwAbilitySystemComponent* CaptainAsc = nullptr;
		for (const ADwCombatUnit* Unit : Combat->GetUnits())
		{
			if (Unit->GetRosterId() == CaptainId)
			{
				CaptainAsc = Unit->GetDwAbilitySystem();
				CaptainAsc->SetNumericAttributeBase(UDwUnitAttributeSet::GetHealthAttribute(), 60.f);
			}
		}
		for (const ADwCombatUnit* Unit : Combat->GetUnits())
		{
			UDwAbilitySystemComponent* Asc = Unit->GetDwAbilitySystem();
			if (Unit->GetRosterId() == SniperId && Unit->IsAlive() && Kill)
			{
				Asc->ApplyGameplayEffectToSelf(Kill->GetDefaultObject<UGameplayEffect>(), 1.f, Asc->MakeEffectContext());
			}
			else if (Unit->GetSide() == EDwSide::Enemy && Unit->IsAlive() && Kill && CaptainAsc)
			{
				// The captain is the instigator: his kills earn kill XP, so he levels up after the fight (DW-203).
				CaptainAsc->ApplyGameplayEffectToTarget(Kill->GetDefaultObject<UGameplayEffect>(), Asc, 1.f, CaptainAsc->MakeEffectContext());
			}
		}
		Combat->SubmitCommand(FDwCombatCommand::MakeEndTurn(EDwSide::Player)); // deaths outside a command count at the turn boundary
		Next(1.0f + 2.5f); // ADwGameMode::PostCombatSeconds, then the rewards
		return;
	}
	case 3: // rewards: gold, the plunder item, XP and the captain's level-up
		UE_LOG(LogDw, Log, TEXT("Dev capture 'run': phase %s after fight 1; %d gold, %d item(s), %d level-up(s) pending."),
			*DwRun::PhaseName(Run->GetPhase()), Run->GetState().PendingRewards.Gold, Run->GetState().PendingRewards.Items.Num(),
			Run->GetState().PendingLevelUps.Num());
		CaptureFrame(3);
		Next(0.5f); // the screenshot is taken at the end of the frame: act on the next step
		return;
	case 4:
		UE_CLOG(!Run->ClaimRewards(INDEX_NONE, FGuid()), LogDw, Error, TEXT("Dev capture 'run': could not claim the rewards."));
		Next(0.5f);
		return;
	case 5: // level-up: the perk choices
		UE_LOG(LogDw, Log, TEXT("Dev capture 'run': phase %s after claiming."), *DwRun::PhaseName(Run->GetPhase()));
		CaptureFrame(4);
		Next(0.5f);
		return;
	case 6:
		while (const FDwPendingLevelUp* LevelUp = Run->GetPhase() == EDwRunPhase::LevelUp ? Run->GetCurrentLevelUp() : nullptr)
		{
			if (!Run->ChoosePerk(LevelUp->PerkChoices.IsEmpty() ? INDEX_NONE : 0))
			{
				UE_LOG(LogDw, Error, TEXT("Dev capture 'run': could not choose a perk."));
				break;
			}
		}
		Next(0.5f);
		return;
	case 7: // back on the map: gold, item, level, perk, Health and the loss carried over
		UE_LOG(LogDw, Log, TEXT("Dev capture 'run': phase %s after the level-up."), *DwRun::PhaseName(Run->GetPhase()));
		CaptureFrame(5);
		Next(0.5f);
		return;
	case 8:
		UE_CLOG(!Sail(), LogDw, Error, TEXT("Dev capture 'run': could not sail on after fight 1."));
		Next(2.5f);
		return;
	default: // fight 2: two crew aboard, the captain at level 2 with a perk
		Capture();
		return;
	}
}

void UDwDevCaptureSubsystem::WinFightByCaptain() const
{
	UWorld* World = GetWorld();
	const UDwRunSubsystem* Runs = UDwRunSubsystem::Get(World);
	const UDwRun* Run = Runs ? Runs->GetRun() : nullptr;
	UDwCombatSubsystem* Combat = World ? World->GetSubsystem<UDwCombatSubsystem>() : nullptr;
	const TSubclassOf<UGameplayEffect> Kill = DwGas::GetSharedEffects().Kill;
	if (!Run || !Combat || !Kill)
	{
		return;
	}
	UDwAbilitySystemComponent* CaptainAsc = nullptr;
	for (const ADwCombatUnit* Unit : Combat->GetUnits())
	{
		CaptainAsc = Unit->GetRosterId() == Run->GetState().Crew[0].Id ? Unit->GetDwAbilitySystem() : CaptainAsc;
	}
	for (const ADwCombatUnit* Unit : Combat->GetUnits())
	{
		if (Unit->GetSide() == EDwSide::Enemy && Unit->IsAlive() && CaptainAsc)
		{
			CaptainAsc->ApplyGameplayEffectToTarget(Kill->GetDefaultObject<UGameplayEffect>(), Unit->GetDwAbilitySystem(), 1.f, CaptainAsc->MakeEffectContext());
		}
	}
	Combat->SubmitCommand(FDwCombatCommand::MakeEndTurn(EDwSide::Player)); // deaths outside a command count at the turn boundary
}

void UDwDevCaptureSubsystem::VoyageDemoStep()
{
	UWorld* World = GetWorld();
	const UDwRunSubsystem* Runs = UDwRunSubsystem::Get(World);
	UDwRun* Run = Runs ? Runs->GetRun() : nullptr;
	auto Next = [this, World](float Seconds)
	{
		World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::VoyageDemoStep), Seconds, false);
	};
	if (!Run)
	{
		UE_LOG(LogDw, Error, TEXT("Dev capture 'voyage': no run (start the game with -DwCaptain=<name>)."));
		Capture();
		return;
	}
	const EDwRunPhase Phase = Run->GetPhase();
	if (Phase == EDwRunPhase::Combat)
	{
		if (!bVoyageFightStaged)
		{
			bVoyageFightStaged = true;
			Next(2.5f); // spawn and settle
			return;
		}
		bVoyageFightStaged = false;
		WinFightByCaptain();
		Next(1.0f + 2.5f); // ADwGameMode::PostCombatSeconds, then the rewards
		return;
	}
	if (Run->IsOver())
	{
		UE_LOG(LogDw, Error, TEXT("Dev capture 'voyage': the run ended (%s)."), *DwRun::EndReasonName(Run->GetState().Stats.EndReason));
		Capture();
		return;
	}

	// Capture the screen first; act on it on the next step (screenshots are taken at the end of the frame).
	const TArray<int32> Choices = Run->GetChoices();
	const bool bAtBoss = Phase == EDwRunPhase::Map && Choices.Num() == 1 && Choices[0] == Run->GetState().Map.BossNode;
	const bool bWanted = Phase == EDwRunPhase::Map || !VoyageScreensSeen.Contains(static_cast<uint8>(Phase));
	if (!bVoyageCaptured && bWanted)
	{
		if (bAtBoss)
		{
			UE_LOG(LogDw, Log, TEXT("Dev capture 'voyage': the Kraken is the only route left after %d stops (%d layers); %d gold, %d crew alive."),
				VoyageStops, Run->GetState().Map.LayerCount, Run->GetState().Gold, Run->GetState().CountLivingCrew());
			Capture(); // final frame: the chart with only the boss ahead (DW-209 makes the fight)
			return;
		}
		VoyageScreensSeen.Add(static_cast<uint8>(Phase));
		CaptureFrame(++VoyageFrame);
		UE_LOG(LogDw, Log, TEXT("Dev capture 'voyage': frame %02d = %s"), VoyageFrame, *DwRun::PhaseName(Phase));
		bVoyageCaptured = true;
		Next(0.5f);
		return;
	}
	bVoyageCaptured = false;
	bool bOk = true;
	switch (Phase)
	{
	case EDwRunPhase::Map:
	{
		// A kind of stop not shown yet (shop, event) if one is offered, else rotate between the routes.
		const int32* Unseen = Choices.FindByPredicate([this, Run](int32 Index)
		{
			const EDwNodeType Type = Run->GetState().Map.Nodes[Index].Type;
			return (Type == EDwNodeType::Shop && !VoyageScreensSeen.Contains(static_cast<uint8>(EDwRunPhase::Shop)))
				|| (Type == EDwNodeType::Event && !VoyageScreensSeen.Contains(static_cast<uint8>(EDwRunPhase::Event)));
		});
		const int32 Pick = Unseen ? *Unseen : (Choices.IsEmpty() ? INDEX_NONE : Choices[VoyageStops % Choices.Num()]);
		++VoyageStops;
		bOk = Pick != INDEX_NONE && Run->ChooseNode(Pick);
		break;
	}
	case EDwRunPhase::Rewards: bOk = Run->ClaimRewards(INDEX_NONE, FGuid()); break;
	case EDwRunPhase::LevelUp:
		bOk = Run->ChoosePerk(Run->GetCurrentLevelUp() && !Run->GetCurrentLevelUp()->PerkChoices.IsEmpty() ? 0 : INDEX_NONE);
		break;
	case EDwRunPhase::Shop:  bOk = Run->LeaveShop(); break;
	case EDwRunPhase::Event: bOk = Run->LeaveEvent(); break;
	default: break;
	}
	UE_CLOG(!bOk, LogDw, Error, TEXT("Dev capture 'voyage': could not act in phase %s."), *DwRun::PhaseName(Phase));
	Next(0.6f);
}

void UDwDevCaptureSubsystem::Capture()
{
	FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
	UE_LOG(LogDw, Log, TEXT("Dev capture requested: %s"), *Path);
	// Give the renderer a moment to write the file, then quit.
	GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([]() { FPlatformMisc::RequestExit(false); }), 1.5f, false);
}
