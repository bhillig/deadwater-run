// Copyright Brandon Hillig 2026

#include "Dev/DwDevCaptureSubsystem.h"

#include "Abilities/DwAbilitySystemComponent.h"
#include "Abilities/DwGameplayAbility.h"
#include "Board/DwBoard.h"
#include "Board/DwGridQueries.h"
#include "Presentation/DwCameraRig.h"
#include "Core/DwGameData.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwShipUpgradeDefinition.h"
#include "Data/DwShopDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "HAL/IConsoleManager.h"
#include "GameplayEffect.h"
#include "Combat/DwCombatInteraction.h"
#include "Combat/DwCombatRules.h"
#include "Combat/DwCombatSubsystem.h"
#include "Combat/DwCombatUnit.h"
#include "Combat/DwShip.h"
#include "Core/DwGameplayTags.h"
#include "Core/DwLog.h"
#include "Core/DwGameMode.h"
#include "Core/DwPlayerController.h"
#include "Abilities/DwAttributeSets.h"
#include "Engine/World.h"
#include "Run/DwRun.h"
#include "Run/DwRunSubsystem.h"
#include "Shell/DwGameUserSettings.h"
#include "UI/DwActivatableScreen.h"
#include "UI/DwRunViewModels.h"
#include "UI/DwShellViewModels.h"
#include "UI/DwUIManagerSubsystem.h"
#include "UI/DwRunScreens.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/PanelWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "HighResScreenshot.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Containers/Ticker.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Run/Events/DwEventResolver.h"

namespace
{
	/** Demos that only pass through an event: take its first possible choice (FDwEventResolver::PickDefaultChoice), then leave. */
	bool ResolveEvent(UDwRun& Run)
	{
		if (Run.GetEvent().Event && !Run.GetEvent().IsResolved())
		{
			FGuid Crew;
			if (!Run.ChooseEventOption(FDwEventResolver::PickDefaultChoice(Run.GetState(), Crew), Crew))
			{
				return false;
			}
		}
		return Run.LeaveEvent();
	}

	/** From the map to the next fight: a fight route if offered; shops left and events passed on the way. True in Combat. */
	bool SailToNextFight(UDwRun& Run)
	{
		for (int32 Step = 0; Step < 20 && Run.GetPhase() == EDwRunPhase::Map; ++Step)
		{
			const TArray<int32> Choices = Run.GetChoices();
			const int32* Fight = Choices.FindByPredicate([&Run](int32 Index) { return Run.GetState().Map.Nodes[Index].Type == EDwNodeType::Fight; });
			if (Choices.IsEmpty() || !Run.ChooseNode(Fight ? *Fight : Choices[0]))
			{
				return false;
			}
			if (Run.GetPhase() == EDwRunPhase::Shop) { Run.LeaveShop(); }
			if (Run.GetPhase() == EDwRunPhase::Event) { ResolveEvent(Run); }
			while (Run.GetPhase() == EDwRunPhase::Rewards || Run.GetPhase() == EDwRunPhase::LevelUp) // an event's ability offer or levels
			{
				const bool bClaim = Run.GetPhase() == EDwRunPhase::Rewards;
				const FDwPendingLevelUp* LevelUp = Run.GetCurrentLevelUp();
				if (!(bClaim ? Run.ClaimRewards(INDEX_NONE, FGuid()) : Run.ChoosePerk(LevelUp && !LevelUp->PerkChoices.IsEmpty() ? 0 : INDEX_NONE)))
				{
					return false;
				}
			}
		}
		return Run.GetPhase() == EDwRunPhase::Combat;
	}

	/** The run's captain in the fight (invalid handle if none). */
	ADwCombatUnit* FindCaptainUnit(const UDwRun& Run, const UDwCombatSubsystem& Combat)
	{
		const FGuid CaptainId = Run.GetState().Crew.IsEmpty() ? FGuid() : Run.GetState().Crew[0].Id;
		for (ADwCombatUnit* Unit : Combat.GetUnits())
		{
			if (Unit->GetRosterId() == CaptainId && Unit->IsAlive())
			{
				return Unit;
			}
		}
		return nullptr;
	}

	/** Living units of Side, nearest to From first. */
	TArray<ADwCombatUnit*> LivingUnits(const UDwCombatSubsystem& Combat, EDwSide Side, FIntPoint From = FIntPoint::ZeroValue)
	{
		TArray<ADwCombatUnit*> Out;
		for (ADwCombatUnit* Unit : Combat.GetUnits())
		{
			if (Unit->GetSide() == Side && Unit->IsAlive())
			{
				Out.Add(Unit);
			}
		}
		Out.Sort([From](const ADwCombatUnit& A, const ADwCombatUnit& B)
			{ return (A.GetTile() - From).Size() < (B.GetTile() - From).Size(); });
		return Out;
	}

	/** Submits the run captain's ability with Tag at Target (a unit, or a tile when TargetTile is set). */
	EDwCommandResult UseCaptainAbility(UDwCombatSubsystem& Combat, const ADwCombatUnit& Captain, const FGameplayTag& Tag, const ADwCombatUnit* Target,
		TOptional<FIntPoint> TargetTile = {})
	{
		const FDwAbilityTarget Aim = TargetTile.IsSet() ? FDwAbilityTarget::MakeTile(TargetTile.GetValue())
			: FDwAbilityTarget::MakeUnit(Target ? Target->GetHandle() : FDwUnitHandle(), Target ? Target->GetTile() : FIntPoint::ZeroValue);
		return Combat.SubmitCommand(FDwCombatCommand::MakeAbility(Captain.GetHandle(), Tag, Aim));
	}
}

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
	if (Demo == TEXT("voyage") || Demo == TEXT("boss"))
	{
		VoyageDemoStep();
		return;
	}
	if (Demo == TEXT("shell"))
	{
		ShellDemoStep();
		return;
	}
	if (Demo == TEXT("captain"))
	{
		CaptainDemoStep();
		return;
	}
	if (Demo == TEXT("shop"))
	{
		// No dev readout: WBP_Shop shows gold, hull, hold, crew and every offer itself (the readout bled through it).
		ShopDemoStep();
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
			const FDwCombatSnapshot Current = Combat->GetSnapshot(); // a local: FindUnit points into it
			const FDwUnitSnapshot* Unit = Current.FindUnit(Gunner);
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
		else if (Demo == TEXT("cues"))
		{
			CuesDemoStep();
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
	FScreenshotRequest::RequestScreenshot(FPaths::GetBaseFilename(Path, false) + FString::Printf(TEXT("-%02d.png"), EnemyFrame), /*bShowUI*/ !FParse::Param(FCommandLine::Get(), TEXT("DwCaptureNoUI")), false);
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

void UDwDevCaptureSubsystem::CuesDemoStep()
{
	UWorld* World = GetWorld();
	UDwCombatSubsystem* Combat = World ? World->GetSubsystem<UDwCombatSubsystem>() : nullptr;
	if (!Combat || !Combat->IsActive())
	{
		UE_LOG(LogDw, Error, TEXT("Dev capture 'cues': no fight."));
		Capture();
		return;
	}
	const FDwCombatSnapshot Snapshot = Combat->GetSnapshot(); // a local: FindUnit points into it
	auto Alive = [&Snapshot](EDwSide Side, int32 Skip, const FGameplayTag& Tag = FGameplayTag())
	{
		for (const FDwUnitSnapshot& Unit : Snapshot.Units)
		{
			if (Unit.Side == Side && Unit.bAlive && (!Tag.IsValid() || Unit.Tags.HasTag(Tag)) && Skip-- <= 0)
			{
				return &Unit;
			}
		}
		return static_cast<const FDwUnitSnapshot*>(nullptr);
	};
	auto Next = [this, World](float Seconds)
	{
		World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::CuesDemoStep), Seconds, false);
	};
	switch (CuesStep++)
	{
	case 0:
	{
		// The swordsman mans a cannon and fires at the enemy ship.
		const FDwUnitSnapshot* Gunner = Alive(EDwSide::Player, 0, DwTags::Unit_Archetype_Swordsman);
		const ADwShip* Ship = Combat->GetShip(EDwSide::Player);
		if (Gunner && Ship && !Ship->GetCannonTiles().IsEmpty())
		{
			Combat->MoveUnit(Gunner->Handle, Ship->GetCannonTiles()[0], /*bSnapActor*/ true);
			UE_CLOG(Combat->SubmitCommand(FDwCombatCommand::MakeAbility(Gunner->Handle, DwTags::Ability_Cannon,
				FDwAbilityTarget::MakeShip(EDwSide::Enemy))) != EDwCommandResult::Accepted, LogDw, Error, TEXT("Dev capture 'cues': cannon refused."));
		}
		Next(0.35f);
		return;
	}
	case 1:
		CaptureFrame(1);
		Next(1.2f); // the shot resolves (one command at a time)
		return;
	case 2:
	{
		// The captain steps up beside the first enemy and cuts him.
		const FDwUnitSnapshot* Attacker = Alive(EDwSide::Player, 0, DwTags::Unit_Role_Captain);
		const FDwUnitSnapshot* Victim = Alive(EDwSide::Enemy, 0);
		if (Attacker && Victim)
		{
			for (const FIntPoint Step : { FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1), FIntPoint(1, 0) })
			{
				const FIntPoint Tile = Victim->Tile + Step;
				if (Snapshot.Grid.IsInBounds(Tile) && Snapshot.Grid.IsStandable(Tile) && !Snapshot.Grid.IsOccupied(Tile))
				{
					Combat->MoveUnit(Attacker->Handle, Tile, /*bSnapActor*/ true);
					break;
				}
			}
			UE_CLOG(Combat->SubmitCommand(FDwCombatCommand::MakeAbility(Attacker->Handle, DwTags::Ability_Attack_Melee,
				FDwAbilityTarget::MakeUnit(Victim->Handle, Victim->Tile))) != EDwCommandResult::Accepted, LogDw, Error, TEXT("Dev capture 'cues': melee refused."));
		}
		Next(0.3f);
		return;
	}
	case 3:
		CaptureFrame(2);
		Next(0.2f); // the frame is written before anyone dies
		return;
	case 4:
	{
		// Another enemy goes down (the shared GE_Kill, as a sinking ship's crew would).
		const FDwUnitSnapshot* Victim = Alive(EDwSide::Enemy, 1);
		const ADwCombatUnit* Unit = Victim ? Combat->FindUnit(Victim->Handle) : nullptr;
		if (Unit && DwGas::GetSharedEffects().Kill)
		{
			UDwAbilitySystemComponent* Asc = Unit->GetDwAbilitySystem();
			Asc->ApplyGameplayEffectToSelf(DwGas::GetSharedEffects().Kill->GetDefaultObject<UGameplayEffect>(), 1.f, Asc->MakeEffectContext());
		}
		// Master at 50% for the dump (applied, never saved).
		if (UDwGameUserSettings* Settings = UDwGameUserSettings::Get())
		{
			MasterVolumeBeforeDemo = Settings->GetVolume(EDwVolume::Master);
			Settings->SetVolume(EDwVolume::Master, 0.5f);
			Settings->ApplyAudio();
		}
		Next(0.4f);
		return;
	}
	default:
		CaptureFrame(3);
		GEngine->Exec(World, TEXT("Dw.Audio.Dump"));
		if (UDwGameUserSettings* Settings = UDwGameUserSettings::Get())
		{
			Settings->SetVolume(EDwVolume::Master, MasterVolumeBeforeDemo);
			Settings->ApplyAudio();
		}
		World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::Capture), 0.3f, false);
		return;
	}
}

void UDwDevCaptureSubsystem::CaptureFrame(int32 Frame) const
{
	FScreenshotRequest::RequestScreenshot(FPaths::GetBaseFilename(Path, false) + FString::Printf(TEXT("-%02d.png"), Frame), /*bShowUI*/ !FParse::Param(FCommandLine::Get(), TEXT("DwCaptureNoUI")), false);
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
	auto Sail = [Run]() { return SailToNextFight(*Run); };
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

void UDwDevCaptureSubsystem::CaptainDemoStep()
{
	UWorld* World = GetWorld();
	const UDwRunSubsystem* Runs = UDwRunSubsystem::Get(World);
	UDwRun* Run = Runs ? Runs->GetRun() : nullptr;
	UDwCombatSubsystem* Combat = World->GetSubsystem<UDwCombatSubsystem>();
	auto Next = [this, World](float Seconds)
	{
		World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::CaptainDemoStep), Seconds, false);
	};
	if (!Run || !Combat || !Run->GetState().Captain)
	{
		UE_LOG(LogDw, Error, TEXT("Dev capture 'captain': no run (start the game with -DwCaptain=<name>)."));
		Capture();
		return;
	}
	const FString CaptainName = Run->GetState().Captain->GetName();
	ADwCombatUnit* Captain = FindCaptainUnit(*Run, *Combat);
	const bool bFightOver = Combat->GetOutcome() != EDwCombatOutcome::None;

	if (CaptainStage == 0)
	{
		UE_CLOG(!SailToNextFight(*Run), LogDw, Error, TEXT("Dev capture 'captain': could not sail to a fight."));
		CaptainStage = 1;
		Next(2.5f); // spawn and settle
		return;
	}
	if (CaptainStage == 100) // the fight is over: the post-combat beat, then the rewards screen
	{
		CaptainStage = 101;
		Next(3.5f);
		return;
	}
	if (CaptainStage == 101)
	{
		UE_LOG(LogDw, Log, TEXT("Dev capture 'captain': %s ends the fight %s; phase %s."), *CaptainName,
			*StaticEnum<EDwCombatOutcome>()->GetNameStringByValue(static_cast<int64>(Run->GetState().PendingRewards.Outcome)), *DwRun::PhaseName(Run->GetPhase()));
		Capture(); // final frame
		return;
	}
	if (bFightOver)
	{
		CaptainStage = 100;
		Next(0.1f);
		return;
	}
	if (Combat->GetActiveSide() != EDwSide::Player || !Captain)
	{
		UE_CLOG(!Captain, LogDw, Error, TEXT("Dev capture 'captain': the captain is not in the fight."));
		Next(0.5f); // the enemy turn plays out
		return;
	}

	// Three steps per turn: act, capture what it did, end the turn (screenshots are taken at the end of the frame).
	if (CaptainStage % 3 == 0)
	{
		++CaptainStage;
		bCaptainDemoMoved = false;
		Combat->SubmitCommand(FDwCombatCommand::MakeEndTurn(EDwSide::Player));
		Next(1.0f);
		return;
	}
	if (CaptainStage % 3 == 1)
	{
		const TArray<ADwCombatUnit*> Enemies = LivingUnits(*Combat, EDwSide::Enemy, Captain->GetTile());
		EDwCommandResult Result = EDwCommandResult::Accepted;
		FString What;
		const int32 Turn = CaptainStage / 3; // 0-based player turn of the demo
		// Step toward the nearest enemy (to the reachable tile closest to it, at least MinDistance away), once per turn.
		// True while the step is under way: come back next step.
		auto StepToward = [&](float MinDistance)
		{
			if (!bCaptainDemoMoved)
			{
				bCaptainDemoMoved = true;
				const FDwCombatSnapshot Snapshot = Combat->GetSnapshot();
				const FIntPoint EnemyTile = Enemies.IsEmpty() ? Captain->GetTile() : Enemies[0]->GetTile();
				TArray<FIntPoint> Tiles;
				for (int32 Index = 0; Index < Snapshot.Grid.Tiles.Num(); ++Index)
				{
					Tiles.Add(Snapshot.Grid.ToTile(Index));
				}
				Tiles.Sort([EnemyTile](const FIntPoint& A, const FIntPoint& B) { return (A - EnemyTile).Size() < (B - EnemyTile).Size(); });
				for (const FIntPoint& Tile : Tiles)
				{
					if ((Tile - EnemyTile).Size() > MinDistance && Combat->SubmitCommand(FDwCombatCommand::MakeMove(Captain->GetHandle(), Tile)) == EDwCommandResult::Accepted)
					{
						UE_LOG(LogDw, Log, TEXT("Dev capture 'captain': the captain steps to (%d, %d)."), Tile.X, Tile.Y);
						break;
					}
				}
				Next(2.0f); // the walk
				return true;
			}
			if (Combat->IsResolving())
			{
				Next(0.5f);
				return true;
			}
			return false;
		};

		if (CaptainName.Contains(TEXT("Fearmongerer")))
		{
			// DW-206: Smoke and Fire every turn until every enemy is at 3 Fear and the crew surrenders.
			What = TEXT("Smoke and Fire");
			Result = UseCaptainAbility(*Combat, *Captain, DwTags::Ability_Captain_SmokeAndFire, Enemies.IsEmpty() ? nullptr : Enemies[0]);
		}
		else if (CaptainName.Contains(TEXT("TeamLeader")) && Turn == 0)
		{
			// DW-207 turn 1: Defend the crewmate beside him (Guarded and Synergy show on the HUD).
			What = TEXT("Defend");
			Result = EDwCommandResult::InvalidTarget;
			for (const ADwCombatUnit* Ally : LivingUnits(*Combat, EDwSide::Player, Captain->GetTile()))
			{
				if (Ally != Captain && Result != EDwCommandResult::Accepted)
				{
					Result = UseCaptainAbility(*Combat, *Captain, DwTags::Ability_Captain_Defend, Ally);
				}
			}
		}
		else if (CaptainName.Contains(TEXT("TeamLeader")) && Turn == 1)
		{
			// DW-207 turn 2: step toward the enemy ship, then Grapple Hook the nearest enemy it can reach.
			What = TEXT("Grapple Hook");
			if (StepToward(2.5f))
			{
				return;
			}
			Result = EDwCommandResult::InvalidTarget;
			for (const ADwCombatUnit* Enemy : LivingUnits(*Combat, EDwSide::Enemy, Captain->GetTile()))
			{
				if (Result != EDwCommandResult::Accepted)
				{
					Result = UseCaptainAbility(*Combat, *Captain, DwTags::Ability_Captain_GrappleHook, Enemy);
				}
			}
		}
		else if (CaptainName.Contains(TEXT("TeamLeader")) && Turn >= 2)
		{
			Capture(); // final frame: the board after the pull and the enemy turn
			return;
		}
		else if (CaptainName.Contains(TEXT("Conqueror")) && Turn == 0)
		{
			// DW-208 turn 1: step up and Charge the first enemy a straight line reaches. Demo staging: that pirate is
			// already hurt (30 HP), so the charge kills and the log shows the kill bonus rising.
			What = TEXT("Charge");
			if (!bCaptainDemoMoved)
			{
				// Demo staging (like the "run" demo): open a lane - the first direction with 3+ free tiles from the captain -
				// and put the nearest enemy at its end, so the charge has someone to hit.
				bCaptainDemoMoved = true;
				const FDwCombatSnapshot Board = Combat->GetSnapshot();
				for (const FIntPoint& Dir : FDwGridModel::Directions)
				{
					const FDwLineHit Lane = FDwTargetingQuery::FirstUnitAlongLine(Board.Grid, Board.GridRules, Captain->GetTile(), Dir, 6);
					const FIntPoint Run3 = Lane.StopTile - Captain->GetTile();
					if (!Lane.Unit.IsSet() && FMath::Abs(Run3.X) + FMath::Abs(Run3.Y) >= 3 && !Enemies.IsEmpty())
					{
						Combat->MoveUnit(Enemies[0]->GetHandle(), Lane.StopTile, /*bSnapActor*/ true);
						UE_LOG(LogDw, Log, TEXT("Dev capture captain: staged an enemy at (%d, %d) for the charge."), Lane.StopTile.X, Lane.StopTile.Y);
						break;
					}
				}
			}
			if (Combat->IsResolving())
			{
				Next(0.5f);
				return;
			}
			const FDwCombatSnapshot Snapshot = Combat->GetSnapshot();
			const UDwAbilityDefinition* Charge = nullptr;
			for (const UDwAbilityDefinition* Ability : Run->GetState().Captain->CaptainAbilities)
			{
				Charge = Ability && Ability->AbilityTag == DwTags::Ability_Captain_Charge ? Ability : Charge;
			}
			TArray<FDwAbilityTarget> Reachable;
			if (const UDwGameplayAbility* Cdo = FDwCombatQueries::GetAbilityCDO(Charge))
			{
				FDwTargetingContext Context;
				Context.Snapshot = &Snapshot;
				Context.Ability = Charge;
				Context.Source = Captain->GetHandle();
				Cdo->GatherValidTargets(Context, Reachable);
			}
			Result = EDwCommandResult::InvalidTarget;
			if (ADwCombatUnit* Target = Reachable.IsEmpty() ? nullptr : Combat->FindUnit(Reachable[0].Unit))
			{
				Target->GetDwAbilitySystem()->SetNumericAttributeBase(UDwUnitAttributeSet::GetHealthAttribute(), 30.f);
				Result = UseCaptainAbility(*Combat, *Captain, DwTags::Ability_Captain_Charge, Target);
			}
			UE_CLOG(Reachable.IsEmpty(), LogDw, Warning, TEXT("Dev capture 'captain': no enemy on a straight line from the Conqueror."));
		}
		else if (CaptainName.Contains(TEXT("Conqueror")) && Turn == 1)
		{
			// DW-208 turn 2: Eye of the Tiger on the farthest enemy - range and sight don't matter.
			What = TEXT("Eye of the Tiger");
			Result = UseCaptainAbility(*Combat, *Captain, DwTags::Ability_Captain_EyeOfTheTiger, Enemies.IsEmpty() ? nullptr : Enemies.Last());
		}
		else if (CaptainName.Contains(TEXT("Conqueror")))
		{
			Capture(); // final frame
			return;
		}
		else
		{
			UE_LOG(LogDw, Error, TEXT("Dev capture 'captain': no script for %s yet."), *CaptainName);
			Capture();
			return;
		}
		UE_LOG(LogDw, Log, TEXT("Dev capture 'captain': %s uses %s: %s."), *CaptainName, *What, *StaticEnum<EDwCommandResult>()->GetNameStringByValue(static_cast<int64>(Result)));
		++CaptainStage;
		Next(1.5f); // the presentation beat
		return;
	}
	CaptureFrame(++CaptainFrame);
	UE_LOG(LogDw, Log, TEXT("Dev capture 'captain': frame %02d (round %d)."), CaptainFrame, Combat->GetRound());
	++CaptainStage;
	Next(0.5f);
}

void UDwDevCaptureSubsystem::ShellDemoStep()
{
	UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	UDwUIManagerSubsystem* UI = PC ? UDwUIManagerSubsystem::Get(PC->GetLocalPlayer()) : nullptr;
	auto Next = [this](float Seconds)
	{
		// The core ticker, not the world timer: world timers stop while the pause menu has the game paused.
		TWeakObjectPtr<UDwDevCaptureSubsystem> Self = this;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self](float)
		{
			if (Self.IsValid())
			{
				Self->ShellDemoStep();
			}
			return false; // once
		}), Seconds);
	};
	auto Top = [UI]() { return UI && UI->GetTopShellScreen() ? UI->GetTopShellScreen()->GetViewModel() : nullptr; };
	auto Press = [&Top](const TCHAR* Button)
	{
		UDwMenuViewModel* Menu = Cast<UDwMenuViewModel>(Top());
		UDwChoiceViewModel* Choice = Menu ? Menu->FindButton(Button) : nullptr;
		const bool bOk = Choice && Choice->Select();
		UE_CLOG(!bOk, LogDw, Error, TEXT("Dev capture 'shell': no '%s' button to press."), Button);
		return bOk;
	};
	if (!UI)
	{
		UE_LOG(LogDw, Error, TEXT("Dev capture 'shell': no UI manager."));
		Capture();
		return;
	}
	// Frames continue across the travel: the menu level takes 01-04, the sea level 05 on.
	const bool bMenuLevel = Cast<ADwGameMode>(World->GetAuthGameMode()) == nullptr;
	const int32 Base = bMenuLevel ? 0 : 4;
	const int32 Step = RunStep++;
	UE_LOG(LogDw, Log, TEXT("Dev capture 'shell': %s step %d"), bMenuLevel ? TEXT("menu") : TEXT("sea"), Step);
	// Each level gets a fresh subsystem; this counts main menu arrivals across the travels (the 2nd one ends the demo).
	static int32 MenuArrivals = 0;
	if (bMenuLevel && Step == 0 && ++MenuArrivals > 1)
	{
		const UDwRunSubsystem* Runs = UDwRunSubsystem::Get(World);
		UE_LOG(LogDw, Log, TEXT("Dev capture 'shell': back on the main menu, run %s."), Runs && Runs->GetRun() ? TEXT("still set (bug)") : TEXT("cleared"));
		Capture(); // final frame: the main menu again
		return;
	}
	if (bMenuLevel)
	{
		switch (Step)
		{
		case 0: CaptureFrame(Base + 1); Next(0.5f); return;                                // main menu
		case 1: Press(TEXT("Settings")); Next(0.5f); return;
		case 2: CaptureFrame(Base + 2); Next(0.5f); return;                                // settings
		case 3:
			if (UDwSettingsViewModel* Settings = Cast<UDwSettingsViewModel>(Top())) { Settings->HandleBack(); } // Esc
			Press(TEXT("New Run"));
			Next(0.5f);
			return;
		case 4: CaptureFrame(Base + 3); Next(0.5f); return;                                // captain select, nothing picked
		case 5:
			if (UDwCaptainSelectViewModel* Select = Cast<UDwCaptainSelectViewModel>(Top())) { Select->SelectCaptain(0); }
			Next(0.5f);
			return;
		case 6: CaptureFrame(Base + 4); Next(0.5f); return;                                // Team Leader picked
		default:
			if (UDwCaptainSelectViewModel* Select = Cast<UDwCaptainSelectViewModel>(Top()))
			{
				UE_CLOG(!Select->GetSetSail()->Select(), LogDw, Error, TEXT("Dev capture 'shell': Set Sail refused."));
			}
			return; // travel to L_Sea; its own capture subsystem carries on
		}
	}
	switch (Step)
	{
	case 0: CaptureFrame(Base + 1); Next(0.5f); return;                                    // the map
	case 1: UE_CLOG(!UI->TogglePause(), LogDw, Error, TEXT("Dev capture 'shell': pause refused.")); Next(0.5f); return;
	case 2: CaptureFrame(Base + 2); Next(0.5f); return;                                    // pause
	case 3: Press(TEXT("Abandon Run")); Next(0.5f); return;
	case 4: CaptureFrame(Base + 3); Next(0.5f); return;                                    // confirm
	case 5: Press(TEXT("Abandon")); Next(0.8f); return;
	case 6: CaptureFrame(Base + 4); Next(0.5f); return;                                    // summary
	case 7: Press(TEXT("New Run")); Next(0.5f); return;
	case 8:
		if (UDwCaptainSelectViewModel* Select = Cast<UDwCaptainSelectViewModel>(Top())) { Select->SelectCaptain(2); }
		Next(0.5f);
		return;
	case 9: CaptureFrame(Base + 5); Next(0.5f); return;                                    // New Run's captain select
	case 10:
		if (UDwCaptainSelectViewModel* Select = Cast<UDwCaptainSelectViewModel>(Top()))
		{
			UE_CLOG(!Select->GetSetSail()->Select(), LogDw, Error, TEXT("Dev capture 'shell': second Set Sail refused."));
		}
		Next(1.0f);
		return;
	case 11:
		if (const UDwRunSubsystem* Runs = UDwRunSubsystem::Get(World); Runs && Runs->GetRun())
		{
			const FDwRunState& S = Runs->GetRun()->GetState();
			UE_LOG(LogDw, Log, TEXT("Dev capture 'shell': new run with %s, phase %s, %d gold, %d crew, %d stops cleared."),
				*GetNameSafe(S.Captain.Get()), *DwRun::PhaseName(S.Phase), S.Gold, S.Crew.Num(), S.Stats.NodesCleared);
		}
		CaptureFrame(Base + 6); // the fresh run's map
		Next(0.5f);
		return;
	// Then out through the summary's Main Menu: pause, abandon, summary, Main Menu -> L_MainMenu (the final frame).
	case 12: UI->TogglePause(); Next(0.5f); return;
	case 13: Press(TEXT("Abandon Run")); Next(0.5f); return;
	case 14: Press(TEXT("Abandon")); Next(0.8f); return;
	default:
		Press(TEXT("Main Menu")); // travel; the menu level's subsystem takes the final frame
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
		UE_LOG(LogDw, Error, TEXT("Dev capture '%s': no run (start the game with -DwCaptain=<name>)."), *Demo);
		Capture();
		return;
	}
	const EDwRunPhase Phase = Run->GetPhase();
	if (Phase == EDwRunPhase::Combat)
	{
		if (Run->GetState().CurrentNode == Run->GetState().Map.BossNode)
		{
			BossFightStep();
			return;
		}
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
		const bool bVictory = Run->GetPhase() == EDwRunPhase::Victory;
		UE_CLOG(!bVictory, LogDw, Error, TEXT("Dev capture '%s': the run ended (%s)."), *Demo, *DwRun::EndReasonName(Run->GetState().Stats.EndReason));
		UE_CLOG(bVictory, LogDw, Log, TEXT("Dev capture '%s': Victory after %d stops; %d of %d crew alive, %d nodes cleared."), *Demo, VoyageStops,
			Run->GetState().CountLivingCrew(), Run->GetState().Crew.Num(), Run->GetState().Stats.NodesCleared);
		Capture(); // final frame: the run summary
		return;
	}

	// Capture the screen first; act on it on the next step (screenshots are taken at the end of the frame).
	// Demo "boss" only captures from the last stop of the chart on.
	const bool bBossOnly = Demo == TEXT("boss");
	const TArray<int32> Choices = Run->GetChoices();
	const bool bAtBoss = Phase == EDwRunPhase::Map && Choices.Num() == 1 && Choices[0] == Run->GetState().Map.BossNode;
	const bool bWanted = bAtBoss || (!bBossOnly && (Phase == EDwRunPhase::Map || !VoyageScreensSeen.Contains(static_cast<uint8>(Phase))));
	if (!bVoyageCaptured && bWanted)
	{
		if (bAtBoss)
		{
			UE_LOG(LogDw, Log, TEXT("Dev capture '%s': the Kraken is the only route left after %d stops (%d layers); %d gold, %d crew alive."),
				*Demo, VoyageStops, Run->GetState().Map.LayerCount, Run->GetState().Gold, Run->GetState().CountLivingCrew());
		}
		VoyageScreensSeen.Add(static_cast<uint8>(Phase));
		CaptureFrame(++VoyageFrame);
		UE_LOG(LogDw, Log, TEXT("Dev capture '%s': frame %02d = %s"), *Demo, VoyageFrame, *DwRun::PhaseName(Phase));
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
	case EDwRunPhase::Event:
		// Two screens: the choices (captured above), then the result (captured on the next step).
		if (Run->GetEvent().Event && !Run->GetEvent().IsResolved())
		{
			FGuid Crew;
			bOk = Run->ChooseEventOption(FDwEventResolver::PickDefaultChoice(Run->GetState(), Crew), Crew);
			if (!bVoyageEventResultShown) // the first event's result gets its own frame too
			{
				VoyageScreensSeen.Remove(static_cast<uint8>(EDwRunPhase::Event));
				bVoyageEventResultShown = true;
			}
		}
		else
		{
			bOk = Run->LeaveEvent();
		}
		break;
	default: break;
	}
	UE_CLOG(!bOk, LogDw, Error, TEXT("Dev capture '%s': could not act in phase %s."), *Demo, *DwRun::PhaseName(Phase));
	Next(0.6f);
}

void UDwDevCaptureSubsystem::ShopDemoStep()
{
	UWorld* World = GetWorld();
	const UDwRunSubsystem* Runs = UDwRunSubsystem::Get(World);
	UDwRun* Run = Runs ? Runs->GetRun() : nullptr;
	UDwCombatSubsystem* Combat = World->GetSubsystem<UDwCombatSubsystem>();
	auto Next = [this, World](float Seconds)
	{
		World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::ShopDemoStep), Seconds, false);
	};
	if (!Run || !Combat || Run->IsOver())
	{
		UE_LOG(LogDw, Error, TEXT("Dev capture 'shop': %s."), Run ? *FString::Printf(TEXT("the run ended (%s)"), *DwRun::EndReasonName(Run->GetState().Stats.EndReason))
			: TEXT("no run (start the game with -DwCaptain=<name>)"));
		Capture();
		return;
	}
	const FDwRunState& S = Run->GetState();
	auto Frame = [this](const TCHAR* What)
	{
		CaptureFrame(++VoyageFrame);
		UE_LOG(LogDw, Log, TEXT("Dev capture 'shop': frame %02d = %s"), VoyageFrame, What);
	};
	auto LogState = [&S](const TCHAR* When)
	{
		UE_LOG(LogDw, Log, TEXT("Dev capture 'shop': %s: %d gold, hull %.0f / %.0f, cannons %.0f, %d upgrade(s), %d crew alive, %d item(s)."), When, S.Gold,
			S.Ship.Health, S.Ship.MaxHealth, S.Ship.CannonDamage, S.Ship.Upgrades.Num(), S.CountLivingCrew(), S.CountItems());
	};

	if (ShopStage == 5 && Run->GetPhase() != EDwRunPhase::Shop)
	{
		UE_LOG(LogDw, Log, TEXT("Dev capture 'shop': LEAVE CLICK PASS - the Slate click left the shop (phase %s)."), *UEnum::GetValueAsString(Run->GetPhase()));
		ShopStage = 6;
	}
	switch (Run->GetPhase())
	{
	case EDwRunPhase::Combat:
		if (!bShopFightStaged)
		{
			bShopFightStaged = true;
			Next(2.5f); // spawn and settle
			return;
		}
		bShopFightStaged = false;
		if (ShopStage >= 4)
		{
			const ADwShip* Ship = Combat->GetShip(EDwSide::Player);
			UE_LOG(LogDw, Log, TEXT("Dev capture 'shop': the next fight with %d crew and %d player cannon(s)."), S.CountLivingCrew(),
				Ship ? Ship->GetCannonTiles().Num() : 0);
			Capture(); // final frame
			return;
		}
		if (ADwShip* Ship = Combat->GetShip(EDwSide::Player))
		{
			// Knock the hull down so the shop's repair has something to fix (the result writes it back to the run).
			Ship->GetAbilitySystemComponent()->SetNumericAttributeBase(UDwShipAttributeSet::GetHealthAttribute(), FMath::RoundToFloat(Ship->GetMaxHealth() * 0.6f));
		}
		WinFightByCaptain();
		Next(1.0f + 2.5f); // ADwGameMode::PostCombatSeconds, then the rewards
		return;
	case EDwRunPhase::Rewards:
		UE_CLOG(!Run->ClaimRewards(INDEX_NONE, FGuid()), LogDw, Error, TEXT("Dev capture 'shop': could not claim the rewards."));
		Next(0.6f);
		return;
	case EDwRunPhase::LevelUp:
		Run->ChoosePerk(Run->GetCurrentLevelUp() && !Run->GetCurrentLevelUp()->PerkChoices.IsEmpty() ? 0 : INDEX_NONE);
		Next(0.6f);
		return;
	case EDwRunPhase::Map:
	{
		if (ShopStage == 4)
		{
			LogState(TEXT("back on the map"));
			Frame(TEXT("map after the shop"));
			ShopStage = 5;
			Next(0.5f);
			return;
		}
		// Gold for the shopping list: a repair, the dearest recruit, the dearest upgrade and the first item.
		const UDwShopDefinition* Shop = Run->GetGameData() ? Run->GetGameData()->DefaultShop.Get() : nullptr;
		const FDwEconomyTuningRow* Economy = Run->GetGameData() && Run->GetGameData()->TuningTables.Economy
			? Run->GetGameData()->TuningTables.Economy->FindRow<FDwEconomyTuningRow>(UDwRun::DefaultTuningRow, TEXT("Capture")) : nullptr;
		int32 Needed = Economy ? Economy->RepairCost : 0;
		if (Shop)
		{
			int32 Recruit = 0, Upgrade = 0;
			for (const UDwUnitDefinition* Unit : Shop->RecruitPool)
			{
				const FDwUnitTuningRow* Row = Unit ? Unit->GetTuningRow<FDwUnitTuningRow>() : nullptr;
				Recruit = FMath::Max(Recruit, Row ? Row->RecruitCost : 0);
			}
			for (const UDwShipUpgradeDefinition* Up : Shop->UpgradePool)
			{
				const FDwShipUpgradeTuningRow* Row = Up ? Up->GetTuningRow<FDwShipUpgradeTuningRow>() : nullptr;
				Upgrade = FMath::Max(Upgrade, Row ? Row->Price : 0);
			}
			const FDwItemTuningRow* ItemRow = !Shop->Items.IsEmpty() && Shop->Items[0] ? Shop->Items[0]->GetTuningRow<FDwItemTuningRow>() : nullptr;
			Needed += Recruit + Upgrade + (ItemRow ? ItemRow->Price : 0);
		}
		const TArray<int32> Choices = Run->GetChoices();
		auto Of = [&S, &Choices](EDwNodeType Type) { return Choices.FindByPredicate([&S, Type](int32 Index) { return S.Map.Nodes[Index].Type == Type; }); };
		const bool bShopping = ShopStage == 0 && S.Gold >= Needed;
		const int32* Pick = bShopping && Of(EDwNodeType::Shop) ? Of(EDwNodeType::Shop) : (Of(EDwNodeType::Fight) ? Of(EDwNodeType::Fight) : Of(EDwNodeType::Event));
		if (!Pick && !Choices.IsEmpty())
		{
			Pick = &Choices[0];
		}
		UE_CLOG(!Pick || !Run->ChooseNode(*Pick), LogDw, Error, TEXT("Dev capture 'shop': could not sail on."));
		Next(0.6f);
		return;
	}
	case EDwRunPhase::Event:
		ResolveEvent(*Run);
		Next(0.6f);
		return;
	case EDwRunPhase::Shop:
	{
		if (ShopStage == 4)
		{
			// Leave the way a player does: a real mouse click through Slate at the Leave button's on-screen centre, after
			// buying and teaching (the state where the button once slid off the screen). Hit-testing decides, not the view model.
			FVector2D Centre;
			const bool bClicked = ClickShopLeave(Centre);
			UE_LOG(LogDw, Log, TEXT("Dev capture 'shop': Leave clicked through Slate at (%.0f, %.0f): %s"), Centre.X, Centre.Y,
				bClicked ? TEXT("sent") : TEXT("no Leave button on screen"));
			ShopStage = 5;
			Next(0.6f);
			return;
		}
		if (ShopStage == 5)
		{
			// The player could not leave: fail the capture (no final screenshot -> Capture.ps1 says CAPTURE FAIL), never paper over it.
			UE_LOG(LogDw, Error, TEXT("Dev capture 'shop': LEAVE CLICK FAIL - the Leave click did not leave the shop."));
			FPlatformMisc::RequestExit(false);
			return;
		}
		if (ShopStage >= 6)
		{
			Run->LeaveShop(); // a later shop on the way to the next fight
			Next(0.6f);
			return;
		}
		if (ShopStage == 0)
		{
			LogState(TEXT("arrived at the shop"));
			Frame(TEXT("shop on arrival"));
			ShopStage = 1;
			Next(0.5f);
			return;
		}
		if (ShopStage == 1)
		{
			// One purchase per step, in this order: repair, a recruit, an upgrade (the third cannon if it's offered), an item.
			const FDwShopInventory& Inventory = Run->GetShop();
			auto Buyable = [Run](const FDwShopOffer& Offer) { return Run->CanPurchase(Offer.OfferId, FGuid()) == EDwPurchaseResult::Ok; };
			auto Bought = [&S](EDwShopOfferType Type) { const int32* Count = S.Stats.Purchases.Find(Type); return Count && *Count > 0; };
			const FDwShopOffer* Buy = nullptr;
			for (EDwShopOfferType Type : { EDwShopOfferType::Repair, EDwShopOfferType::Recruit, EDwShopOfferType::ShipUpgrade, EDwShopOfferType::Item })
			{
				if (Buy || Bought(Type))
				{
					continue;
				}
				for (const FDwShopOffer& Offer : Inventory.Offers)
				{
					const bool bPreferred = Type != EDwShopOfferType::ShipUpgrade || (Offer.Upgrade && Offer.Upgrade->GrantedShipTags.HasTagExact(DwTags::Ship_Upgrade_ExtraCannon));
					if (Offer.Type == Type && bPreferred && Buyable(Offer))
					{
						Buy = &Offer;
						break;
					}
				}
				if (!Buy && Type == EDwShopOfferType::ShipUpgrade)
				{
					Buy = Inventory.Offers.FindByPredicate([&Buyable](const FDwShopOffer& Offer) { return Offer.Type == EDwShopOfferType::ShipUpgrade && Buyable(Offer); });
				}
			}
			if (Buy)
			{
				const EDwPurchaseResult Result = Run->TryPurchase(Buy->OfferId, FGuid());
				UE_CLOG(Result != EDwPurchaseResult::Ok, LogDw, Error, TEXT("Dev capture 'shop': purchase refused (%s)."), *DwRun::PurchaseResultName(Result));
				Next(0.4f);
				return;
			}
			LogState(TEXT("after shopping"));
			Frame(TEXT("shop after buying"));
			ShopStage = 2;
			Next(0.5f);
			return;
		}
		// Stages 2-3: teach a move through the shop screen's view model, the way the clicks do: the move (one with two
		// learners if offered), a learner, a move to forget if they're full, then Buy.
		const ULocalPlayer* Player = World->GetFirstLocalPlayerFromController();
		UDwShopViewModel* ShopView = Player ? UDwUIManagerSubsystem::Get(Player)->GetShopViewModel() : nullptr;
		if (ShopStage == 2)
		{
			if (ShopView)
			{
				int32 Card = INDEX_NONE, Index = 0;
				for (const FDwShopOffer& Offer : Run->GetShop().Offers)
				{
					if (Offer.Type != EDwShopOfferType::Ability)
					{
						continue;
					}
					const bool bEnabled = ShopView->GetMoves().IsValidIndex(Index) && ShopView->GetMoves()[Index]->GetView().bEnabled;
					if (bEnabled && (Card == INDEX_NONE || Offer.EligibleCrew.Num() > 1))
					{
						Card = Index;
						if (Offer.EligibleCrew.Num() > 1) { break; }
					}
					++Index;
				}
				if (Card != INDEX_NONE && ShopView->GetMoves()[Card]->Select())
				{
					if (!ShopView->GetLearners().IsEmpty() && !ShopView->GetLearners().ContainsByPredicate([](const TObjectPtr<UDwChoiceViewModel>& L) { return L->GetView().bSelected; }))
					{
						ShopView->GetLearners().Last()->Select();
					}
					if (!ShopView->GetForget().IsEmpty())
					{
						ShopView->GetForget()[0]->Select();
					}
				}
			}
			UE_CLOG(!ShopView || !ShopView->GetBuyMove(), LogDw, Warning, TEXT("Dev capture 'shop': no move to teach at this shop."));
			ShopStage = 3; // captured next step: screenshots are taken at the end of a frame, after layout settles
			Next(0.5f);
			return;
		}
		if (ShopStage == 3)
		{
			if (!bShopMoveCaptured)
			{
				Frame(TEXT("shop: teaching a move"));
				bShopMoveCaptured = true;
				Next(0.5f);
				return;
			}
			if (ShopView && ShopView->GetBuyMove())
			{
				const FText Title = ShopView->GetBuyMove()->GetView().Title;
				UE_CLOG(!ShopView->GetBuyMove()->Select(), LogDw, Error, TEXT("Dev capture 'shop': could not teach the move (%s)."),
					*ShopView->GetBuyMove()->GetView().DisabledReason.ToString());
				UE_LOG(LogDw, Log, TEXT("Dev capture 'shop': %s."), *Title.ToString());
			}
			ShopStage = 4; // the next step leaves (the branch above), then the map frame
			Next(0.5f);
			return;
		}
		Next(0.5f);
		return;
	}
	default:
		Next(0.5f);
		return;
	}
}

bool UDwDevCaptureSubsystem::ClickShopLeave(FVector2D& OutCentre) const
{
	OutCentre = FVector2D::ZeroVector;
	TArray<UUserWidget*> Screens;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Screens, UDwShopScreen::StaticClass(), /*TopLevelOnly*/ false);
	const UPanelWidget* Slot = Screens.IsEmpty() ? nullptr : Cast<UPanelWidget>(Screens[0]->GetWidgetFromName(TEXT("LeaveSlot")));
	const UUserWidget* Entry = Slot && Slot->GetChildrenCount() > 0 ? Cast<UUserWidget>(Slot->GetChildAt(0)) : nullptr;
	const UWidget* Button = Entry ? Entry->GetWidgetFromName(TEXT("ChoiceButton")) : nullptr;
	if (!Button || !FSlateApplication::IsInitialized())
	{
		return false;
	}
	OutCentre = Button->GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f));
	FSlateApplication& Slate = FSlateApplication::Get();
	const TSet<FKey> Pressed = { EKeys::LeftMouseButton };
	Slate.ProcessMouseMoveEvent(FPointerEvent(0, OutCentre, OutCentre, TSet<FKey>(), EKeys::Invalid, 0.f, FModifierKeysState()));
	Slate.ProcessMouseButtonDownEvent(nullptr, FPointerEvent(0, OutCentre, OutCentre, Pressed, EKeys::LeftMouseButton, 0.f, FModifierKeysState()));
	Slate.ProcessMouseButtonUpEvent(FPointerEvent(0, OutCentre, OutCentre, TSet<FKey>(), EKeys::LeftMouseButton, 0.f, FModifierKeysState()));
	return true;
}

void UDwDevCaptureSubsystem::BossFightStep()
{
	UWorld* World = GetWorld();
	const ADwPlayerController* PC = Cast<ADwPlayerController>(World->GetFirstPlayerController());
	UDwCombatInteraction* Interaction = PC ? PC->GetInteraction() : nullptr;
	UDwCombatSubsystem* Combat = World->GetSubsystem<UDwCombatSubsystem>();
	auto Next = [this, World](float Seconds)
	{
		World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::VoyageDemoStep), Seconds, false);
	};
	if (!Combat || !Combat->IsActive())
	{
		Next(0.5f); // between the fight's end and the Victory/Defeat phase
		return;
	}
	const FDwCombatSnapshot Snapshot = Combat->GetSnapshot();
	FDwUnitHandle Sniper;
	FDwUnitHandle Swordsman;
	FDwUnitHandle Tentacle; // the first living tentacle
	for (const FDwUnitSnapshot& Unit : Snapshot.Units)
	{
		const bool bCrew = Unit.bAlive && Unit.Side == EDwSide::Player;
		Sniper = !Sniper.IsValid() && bCrew && Unit.Tags.HasTag(DwTags::Unit_Archetype_Sniper) ? Unit.Handle : Sniper;
		Swordsman = !Swordsman.IsValid() && bCrew && Unit.Tags.HasTag(DwTags::Unit_Archetype_Swordsman) ? Unit.Handle : Swordsman;
		Tentacle = !Tentacle.IsValid() && Unit.bAlive && Unit.Tags.HasTagExact(DwTags::Unit_Kraken_Tentacle) ? Unit.Handle : Tentacle;
	}
	const FDwKrakenSnapshot& K = Snapshot.Kraken;
	const FDwUnitSnapshot* KrakenUnit = Snapshot.FindUnit(K.Body);
	const FDwUnitSnapshot* TentacleUnit = Snapshot.FindUnit(Tentacle);
	// The rail tile beside a tentacle slot (slots are the arena's first column, next to the player's boarding edge).
	auto RailBeside = [](const FDwUnitSnapshot& Slot) { return Slot.Tile - FIntPoint(1, 0); };
	// What the Kraken did on its last turn, from the command record.
	auto LastKrakenMoves = [Combat, &K]()
	{
		TArray<FString> Moves;
		for (const FDwCombatCommand& Command : Combat->GetRecord().Commands)
		{
			if (Command.Unit == K.Body && Command.Type == EDwCommandType::Ability)
			{
				Moves.Add(Command.AbilityTag.GetTagLeafName().ToString());
			}
		}
		return Moves;
	};
	auto KrakenLine = [&]()
	{
		return FString::Printf(TEXT("body %.0f HP, phase %s, tentacles %d/%d cut, damage x%.1f, moves so far [%s]"), KrakenUnit ? KrakenUnit->Health : 0.f,
			*K.PhaseId.ToString(), K.TentaclesDead, K.TentaclesTotal, K.DamageMultiplier, *FString::Join(LastKrakenMoves(), TEXT(", ")));
	};

	switch (BossStep++)
	{
	case 0:
		// Staging: a tougher crew, so the random enraged moves never end the demo before its last frame.
		for (const FDwUnitSnapshot& Unit : Snapshot.Units)
		{
			if (Unit.bAlive && Unit.Side == EDwSide::Player)
			{
				UDwAbilitySystemComponent* Asc = Combat->FindUnit(Unit.Handle)->GetDwAbilitySystem();
				Asc->SetNumericAttributeBase(UDwUnitAttributeSet::GetMaxHealthAttribute(), 400.f);
				Asc->SetNumericAttributeBase(UDwUnitAttributeSet::GetHealthAttribute(), 400.f);
			}
		}
		UE_CLOG(!KrakenUnit, LogDw, Error, TEXT("Dev capture '%s': the boss fight has no Kraken body."), *Demo);
		Next(2.5f); // spawn and settle
		return;
	case 1: // the sniper on the rail aiming at the body (B4: muskets reach it): target highlight and damage preview
		if (Interaction && Sniper.IsValid() && KrakenUnit && TentacleUnit)
		{
			Combat->MoveUnit(Sniper, RailBeside(*TentacleUnit) + FIntPoint(0, 1), /*bSnapActor*/ true);
			Interaction->SelectUnit(Sniper);
			const FDwCombatSnapshot Current = Combat->GetSnapshot(); // a local: FindUnit points into it
			const FDwUnitSnapshot* Unit = Current.FindUnit(Sniper);
			if (Unit && Interaction->SelectAbility(FDwCombatQueries::FindAbility(*Unit, DwTags::Ability_Attack_Ranged)))
			{
				Interaction->HoverTile(KrakenUnit->Tile);
			}
		}
		CaptureFrame(++VoyageFrame);
		UE_LOG(LogDw, Log, TEXT("Dev capture '%s': frame %02d = Kraken fight, sniper aiming at the body (%s)"), *Demo, VoyageFrame, *KrakenLine());
		Next(0.5f);
		return;
	case 2:
		if (Interaction && KrakenUnit)
		{
			Interaction->ClickTile(KrakenUnit->Tile);
		}
		Next(1.2f);
		return;
	case 3: // the swordsman on the rail aiming at a tentacle (melee reaches only the tentacles)
		if (Interaction && Swordsman.IsValid() && TentacleUnit)
		{
			Combat->MoveUnit(Swordsman, RailBeside(*TentacleUnit), /*bSnapActor*/ true);
			Interaction->SelectUnit(Swordsman);
			const FDwCombatSnapshot Current = Combat->GetSnapshot(); // a local: FindUnit points into it
			const FDwUnitSnapshot* Unit = Current.FindUnit(Swordsman);
			if (Unit && Interaction->SelectAbility(FDwCombatQueries::FindAbility(*Unit, DwTags::Ability_Attack_Melee)))
			{
				Interaction->HoverTile(TentacleUnit->Tile);
			}
		}
		CaptureFrame(++VoyageFrame);
		UE_LOG(LogDw, Log, TEXT("Dev capture '%s': frame %02d = swordsman aiming at a tentacle (%s)"), *Demo, VoyageFrame, *KrakenLine());
		Next(0.5f);
		return;
	case 4:
		if (Interaction && TentacleUnit)
		{
			Interaction->ClickTile(TentacleUnit->Tile);
		}
		Next(1.2f);
		return;
	case 5: // staging: finish that tentacle (one cutlass hit is 25 of its 60), then end the turn
		if (TentacleUnit && TentacleUnit->bAlive)
		{
			UDwAbilitySystemComponent* SwordAsc = Combat->FindUnit(Swordsman)->GetDwAbilitySystem();
			SwordAsc->ApplyGameplayEffectToTarget(DwGas::GetSharedEffects().Kill->GetDefaultObject<UGameplayEffect>(),
				Combat->FindUnit(Tentacle)->GetDwAbilitySystem(), 1.f, SwordAsc->MakeEffectContext());
		}
		if (Interaction)
		{
			Interaction->RequestEndTurn();
		}
		Next(3.5f);
		return;
	case 6: // after the Kraken's initial-phase turn, one tentacle short (x0.9)
		CaptureFrame(++VoyageFrame);
		UE_LOG(LogDw, Log, TEXT("Dev capture '%s': frame %02d = after the initial-phase turn (%s, round %d)"), *Demo, VoyageFrame, *KrakenLine(), Snapshot.Round);
		Next(0.5f);
		return;
	case 7: // staging: the body at 28%, so it enrages at the start of its next turn (B7)
		if (KrakenUnit)
		{
			Combat->FindUnit(K.Body)->GetDwAbilitySystem()->SetNumericAttributeBase(UDwUnitAttributeSet::GetHealthAttribute(), KrakenUnit->MaxHealth * 0.28f);
		}
		if (Interaction)
		{
			Interaction->RequestEndTurn();
		}
		Next(4.0f);
		return;
	case 8: // after the enraged turn: Arms of Fury or Eyes of the Sea
		CaptureFrame(++VoyageFrame);
		UE_LOG(LogDw, Log, TEXT("Dev capture '%s': frame %02d = after the enraged turn (%s, round %d)"), *Demo, VoyageFrame, *KrakenLine(), Snapshot.Round);
		Next(0.5f);
		return;
	default:
		WinFightByCaptain();
		Next(1.0f + 2.5f); // ADwGameMode::PostCombatSeconds, then the Victory phase and its summary
		return;
	}
}

void UDwDevCaptureSubsystem::Capture()
{
	FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ !FParse::Param(FCommandLine::Get(), TEXT("DwCaptureNoUI")), /*bAddFilenameSuffix*/ false);
	UE_LOG(LogDw, Log, TEXT("Dev capture requested: %s"), *Path);
	// Give the renderer a moment to write the file, then quit.
	GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([]() { FPlatformMisc::RequestExit(false); }), 1.5f, false);
}
