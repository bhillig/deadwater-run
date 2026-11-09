// Copyright Brandon Hillig 2026

#include "Combat/DwCombatSubsystem.h"

#include "AbilitySystemGlobals.h"
#include "Abilities/DwAbilitySystemComponent.h"
#include "Abilities/DwAttributeSets.h"
#include "Abilities/DwGameplayAbility.h"
#include "Abilities/DwGameplayEffects.h"
#include "Board/DwBoard.h"
#include "Combat/DwCombatRules.h"
#include "Combat/DwCombatSideController.h"
#include "Combat/DwCombatUnit.h"
#include "Combat/DwPassiveSideController.h"
#include "Combat/DwShip.h"
#include "Combat/DwTurnManager.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Core/DwLog.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Presentation/DwPresentationSubsystem.h"
#include "Engine/World.h"
#include "GameplayEffectExtension.h"

namespace DwCombatSubsystem
{
	/** Leaf status tags reported as StatusApplied/StatusRemoved (parents and State.* are not statuses). */
	static bool IsReportedStatus(const FGameplayTag& Tag)
	{
		return Tag == DwTags::Status_Afraid || Tag == DwTags::Status_Incapacitated_Cowering || Tag == DwTags::Status_Incapacitated_Frozen
			|| Tag == DwTags::Status_Rooted || Tag == DwTags::Status_Guarded;
	}

	static FString SideName(EDwSide Side)
	{
		return StaticEnum<EDwSide>()->GetNameStringByValue(static_cast<int64>(Side));
	}

	/** Health is written from meta attributes, so its change data has no GE; the ASC remembers who caused it. */
	static AActor* GetInstigator(const FOnAttributeChangeData& Data, const UDwAbilitySystemComponent* Changed)
	{
		if (Data.GEModData)
		{
			return Data.GEModData->EffectSpec.GetContext().GetOriginalInstigator();
		}
		return Changed ? Changed->GetLastHealthInstigator() : nullptr;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Lifecycle

bool UDwCombatSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDwCombatSubsystem::Deinitialize()
{
	EndEncounter();
	Super::Deinitialize();
}

bool UDwCombatSubsystem::StartEncounter(const FDwEncounterSetup& InSetup)
{
	if (bActive)
	{
		EndEncounter();
	}
	Setup = InSetup;
	Random.Initialize(Setup.Seed);
	Record = FDwCombatRecord();
	Record.Seed = Setup.Seed;
	Result = FDwCombatResult();
	Outcome = EDwCombatOutcome::None;
	Items.Reset();
	ItemDefinitions.Reset();
	for (const FDwItemStack& Stack : Setup.Items)
	{
		if (Stack.Item)
		{
			Items.FindOrAdd(Stack.Item->GetFName()) += Stack.Count;
			ItemDefinitions.Add(Stack.Item->GetFName(), Stack.Item);
		}
	}
	ItemsUsed.Reset();
	Kills.Reset();
	CannonsUsed.Reset();
	CoweringEffects.Reset();
	bResolving = false;
	SubmitDepth = 0;
	bEndTurnSuggested = false;

	bActive = true; // so a failed spawn can be torn down by EndEncounter
	if (!SpawnFight())
	{
		EndEncounter();
		return false;
	}

	TArray<FDwUnitHandle> EnemyOrder;
	for (const ADwCombatUnit* Unit : Units)
	{
		if (Unit->GetSide() == EDwSide::Enemy)
		{
			EnemyOrder.Add(Unit->GetHandle()); // spawn order = encounter order (C14)
		}
	}
	TurnManager = NewObject<UDwTurnManager>(this);
	TurnManager->Init(*this, MoveTemp(EnemyOrder));
	if (!DefaultEnemyController)
	{
		DefaultEnemyController = NewObject<UDwPassiveSideController>(this);
	}

	RefreshDerivedState();
	UE_LOG(LogDwCombat, Log, TEXT("Encounter started: %d crew vs %d enemies, seed %d"), PlayerCrewCount, Units.Num() - PlayerCrewCount, Setup.Seed);
	OnCombatStarted.Broadcast();
	TurnManager->BeginCombat();
	return true;
}

void UDwCombatSubsystem::EndEncounter()
{
	if (!bActive)
	{
		return;
	}
	bActive = false;
	if (TurnManager)
	{
		TurnManager->Stop();
		TurnManager = nullptr;
	}
	for (ADwCombatUnit* Unit : Units)
	{
		if (IsValid(Unit))
		{
			Unit->Destroy();
		}
	}
	Units.Reset();
	for (ADwShip* Ship : { PlayerShip.Get(), EnemyShip.Get() })
	{
		if (IsValid(Ship))
		{
			Ship->Destroy();
		}
	}
	PlayerShip = nullptr;
	EnemyShip = nullptr;
	if (IsValid(Board))
	{
		Board->Destroy();
	}
	Board = nullptr;
	Grid = FDwGridModel();
	PlayerCrewCount = 0;
	bResolving = false;
}

void UDwCombatSubsystem::SetSideController(EDwSide Side, TScriptInterface<IDwCombatSideController> Controller)
{
	(Side == EDwSide::Player ? PlayerController : EnemyController) = Controller;
}

IDwCombatSideController* UDwCombatSubsystem::GetSideController(EDwSide Side) const
{
	if (Side == EDwSide::Player)
	{
		return PlayerController.GetInterface();
	}
	if (IDwCombatSideController* Controller = EnemyController.GetInterface())
	{
		return Controller;
	}
	return DefaultEnemyController ? static_cast<IDwCombatSideController*>(DefaultEnemyController.Get()) : nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
// Spawn

bool UDwCombatSubsystem::SpawnFight()
{
	Grid = Setup.Grid;
	GridRules = Setup.GridRules;
	if (Grid.Size.X <= 0 || Grid.Size.Y <= 0 || Grid.Tiles.Num() != Grid.Size.X * Grid.Size.Y)
	{
		UE_LOG(LogDwCombat, Error, TEXT("StartEncounter: the setup has no board."));
		return false;
	}
	Grid.Occupants.Init(FDwOccupant(), Grid.Tiles.Num()); // occupancy is the fight's, never the setup's

	TArray<FIntPoint> PlayerSpawns;
	TArray<FIntPoint> EnemySpawns;
	for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
	{
		const FDwTile& Tile = Grid.Tiles[Index];
		if (!Grid.IsStandable(Grid.ToTile(Index)))
		{
			continue;
		}
		if (Tile.Tags.HasTagExact(DwTags::Tile_Spawn_Player)) { PlayerSpawns.Add(Grid.ToTile(Index)); }
		if (Tile.Tags.HasTagExact(DwTags::Tile_Spawn_Enemy)) { EnemySpawns.Add(Grid.ToTile(Index)); }
	}
	if (PlayerSpawns.Num() < Setup.Crew.Num() || EnemySpawns.Num() < Setup.Enemies.Num())
	{
		UE_LOG(LogDwCombat, Error, TEXT("StartEncounter: %d crew need %d player spawn tiles (%d), %d enemies need enemy spawn tiles (%d)."),
			Setup.Crew.Num(), Setup.Crew.Num(), PlayerSpawns.Num(), Setup.Enemies.Num(), EnemySpawns.Num());
		return false;
	}

	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Board = World->SpawnActor<ADwBoard>(Params);
	Board->SetGrid(Grid, Setup.TileSize > 0.f ? Setup.TileSize : 100.f);

	PlayerShip = SpawnShip(EDwSide::Player, Setup.PlayerShip);
	if (Setup.bHasEnemyShip)
	{
		EnemyShip = SpawnShip(EDwSide::Enemy, Setup.EnemyShip);
	}

	for (int32 Index = 0; Index < Setup.Crew.Num(); ++Index)
	{
		if (!SpawnUnit(Setup.Crew[Index], EDwSide::Player, PlayerSpawns[Index]))
		{
			return false;
		}
	}
	PlayerCrewCount = Setup.Crew.Num();
	for (int32 Index = 0; Index < Setup.Enemies.Num(); ++Index)
	{
		if (!SpawnUnit(Setup.Enemies[Index], EDwSide::Enemy, EnemySpawns[Index]))
		{
			return false;
		}
	}
	return true;
}

ADwShip* UDwCombatSubsystem::SpawnShip(EDwSide Side, const FDwShipSetup& ShipSetup)
{
	TArray<FIntPoint> CannonTiles;
	for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
	{
		const FDwTile& Tile = Grid.Tiles[Index];
		if (Tile.Owner == Side && Tile.Tags.HasTagExact(DwTags::Tile_Cannon))
		{
			CannonTiles.Add(Grid.ToTile(Index));
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* Class = ShipSetup.ShipClass ? ShipSetup.ShipClass.Get() : ADwShip::StaticClass();
	// The ship actor stands at the centre of its own deck (the Synty mesh aligns from there, DW-307).
	FVector Centre = FVector::ZeroVector;
	int32 DeckTiles = 0;
	for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
	{
		if (Grid.Tiles[Index].Owner == Side && Grid.Tiles[Index].Elevation == 0 && Grid.IsStandable(Grid.ToTile(Index)))
		{
			Centre += Board->TileToWorld(Grid.ToTile(Index));
			++DeckTiles;
		}
	}
	FTransform Transform = Board->GetActorTransform();
	if (DeckTiles > 0)
	{
		Transform.SetLocation(Centre / DeckTiles);
	}
	ADwShip* Ship = GetWorld()->SpawnActor<ADwShip>(Class, Transform, Params);
	Ship->GetDwAbilitySystem()->bSuppressGameplayCues = UDwPresentationSubsystem::IsInstant(this);
	Ship->InitializeFromSetup(Side, ShipSetup, CannonTiles);
	BindShip(*Ship);
	return Ship;
}

ADwCombatUnit* UDwCombatSubsystem::SpawnUnit(const FDwCombatantSetup& Combatant, EDwSide Side, FIntPoint Tile)
{
	const UDwUnitDefinition* Definition = Combatant.Definition;
	const FDwUnitTuningRow* Row = Definition ? Definition->GetTuningRow<FDwUnitTuningRow>() : nullptr;
	if (!Row)
	{
		UE_LOG(LogDwCombat, Error, TEXT("StartEncounter: unit %s has no definition or no DT_UnitTuning row."), *GetNameSafe(Definition));
		return nullptr;
	}

	UClass* Class = Combatant.UnitClassOverride ? Combatant.UnitClassOverride.Get() : Definition->UnitClass.LoadSynchronous();
	if (!Class)
	{
		Class = ADwCombatUnit::StaticClass();
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Both crews face the other ship: the player deck is on the left (-X), the enemy deck on the right.
	const FRotator Facing = Board->GetActorRotation() + FRotator(0.f, Side == EDwSide::Player ? 0.f : 180.f, 0.f);
	ADwCombatUnit* Unit = GetWorld()->SpawnActor<ADwCombatUnit>(Class, Board->TileToWorld(Tile), Facing, Params);

	FDwUnitSpawnParams Spawn;
	Spawn.Handle = FDwUnitHandle(Units.Num() + 1);
	Spawn.Side = Side;
	Spawn.Tile = Tile;
	Spawn.RosterId = Combatant.RosterId;
	Spawn.Definition = Definition;
	Spawn.bIsCaptain = Combatant.bIsCaptain || Definition->UnitTags.HasTagExact(DwTags::Unit_Role_Captain);
	Units.Add(Unit);
	Unit->InitializeFromSpawnParams(Spawn);

	UDwAbilitySystemComponent* Asc = Unit->GetDwAbilitySystem();
	Asc->bSuppressGameplayCues = UDwPresentationSubsystem::IsInstant(this); // nothing to watch in tests and the simulator
	Asc->AddLooseGameplayTags(Definition->UnitTags);
	if (Spawn.bIsCaptain && !Asc->HasMatchingGameplayTag(DwTags::Unit_Role_Captain))
	{
		Asc->AddLooseGameplayTag(DwTags::Unit_Role_Captain);
	}

	// Stats: tuning + level gains; Health carries over (C9), Mana always starts full, no regen.
	const int32 Gains = FMath::Max(0, Combatant.Level - 1);
	FDwUnitInitValues Values;
	Values.MaxHealth = Row->MaxHealth + Row->HealthPerLevel * Gains;
	Values.Health = Combatant.Health < 0.f ? Values.MaxHealth : FMath::Clamp(Combatant.Health, 0.f, Values.MaxHealth);
	Values.MaxMana = Row->MaxMana + Row->ManaPerLevel * Gains;
	Values.Mana = Values.MaxMana;
	Values.AttackDamage = Row->AttackDamage + Row->AttackDamagePerLevel * Gains;
	Values.MoveRange = Row->MoveRange;
	Values.MovePoints = Row->MoveRange;
	Values.MaxActionPoints = Row->MaxActionPoints;
	Values.ActionPoints = Row->MaxActionPoints;
	Values.MaxFear = Setup.MaxFear;
	Values.Fear = FMath::Clamp(Combatant.StartingFear, 0.f, Setup.MaxFear);
	FGameplayEffectSpecHandle InitSpec = Asc->MakeOutgoingSpec(DwGas::GetSharedEffects().InitUnitStats, 1, Asc->MakeEffectContext());
	UDwGE_InitUnitStats::SetValues(*InitSpec.Data, Values);
	Asc->ApplyGameplayEffectSpecToSelf(*InitSpec.Data);
	for (const TSubclassOf<UGameplayEffect>& Effect : Combatant.SpawnEffects)
	{
		if (Effect)
		{
			ApplyEffectToSelf(*Asc, Effect);
		}
	}

	// Abilities: basic attack, definition defaults, learned/captain abilities, then Move/Cannon/Item. One per tag.
	TArray<const UDwAbilityDefinition*> Grants;
	Grants.Add(Definition->BasicAttack);
	Grants.Append(Definition->DefaultAbilities);
	Grants.Append(Combatant.ExtraAbilities);
	Grants.Append(Setup.CommonAbilities);
	FGameplayTagContainer Granted;
	for (const UDwAbilityDefinition* Ability : Grants)
	{
		if (!Ability || !Ability->AbilityTag.IsValid() || Granted.HasTagExact(Ability->AbilityTag))
		{
			continue;
		}
		if (Asc->GrantAbility(Ability).IsValid())
		{
			Granted.AddTag(Ability->AbilityTag);
			Unit->AddGrantedAbility(Ability);
		}
	}

	Grid.SetOccupant(Tile, Spawn.Handle, Side);
	BindUnit(*Unit);
	return Unit;
}

void UDwCombatSubsystem::BindUnit(ADwCombatUnit& Unit)
{
	UDwAbilitySystemComponent* Asc = Unit.GetDwAbilitySystem();
	const FDwUnitHandle Handle = Unit.GetHandle();
	Asc->OnHealthDepleted.AddUObject(this, &ThisClass::HandleHealthDepleted);
	Asc->OnDwAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);
	Asc->GetGameplayAttributeValueChangeDelegate(UDwUnitAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleUnitHealthChanged, Handle);
	Asc->GetGameplayAttributeValueChangeDelegate(UDwUnitAttributeSet::GetFearAttribute()).AddUObject(this, &ThisClass::HandleUnitFearChanged, Handle);
	Asc->RegisterGenericGameplayTagEvent().AddUObject(this, &ThisClass::HandleUnitTagChanged, Handle);
}

void UDwCombatSubsystem::BindShip(ADwShip& Ship)
{
	UDwAbilitySystemComponent* Asc = Ship.GetDwAbilitySystem();
	Asc->OnHealthDepleted.AddUObject(this, &ThisClass::HandleHealthDepleted);
	Asc->GetGameplayAttributeValueChangeDelegate(UDwShipAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleShipHealthChanged, Ship.GetSide());
}

// ---------------------------------------------------------------------------------------------------------------------
// Commands

EDwCommandResult UDwCombatSubsystem::SubmitCommand(const FDwCombatCommand& Command, FText* OutReason)
{
	if (!bActive)
	{
		return EDwCommandResult::CombatOver;
	}
	if (bResolving)
	{
		if (OutReason) { *OutReason = NSLOCTEXT("DwCombat", "Busy", "Wait for the action to finish"); }
		return EDwCommandResult::Busy;
	}

	const EDwCommandResult Validation = FDwCombatQueries::ValidateCommand(GetSnapshot(), Command, OutReason);
	if (Validation != EDwCommandResult::Accepted)
	{
		UE_LOG(LogDwCombat, Verbose, TEXT("Rejected %s: %s"), *Command.ToString(), *StaticEnum<EDwCommandResult>()->GetNameStringByValue(static_cast<int64>(Validation)));
		return Validation;
	}

	switch (Command.Type)
	{
	case EDwCommandType::EndTurn:
		Record.Commands.Add(Command);
		OnCommandResolved.Broadcast(Command);
		TurnManager->EndSideTurn(Command.Side);
		return EDwCommandResult::Accepted;

	case EDwCommandType::EndUnit:
		Record.Commands.Add(Command);
		OnCommandResolved.Broadcast(Command);
		TurnManager->NotifyUnitDone(Command.Unit);
		SuggestEndTurnIfIdle();
		return EDwCommandResult::Accepted;

	default:
		break;
	}

	ADwCombatUnit* Unit = FindUnit(Command.Unit);
	UDwAbilitySystemComponent* Asc = Unit ? Unit->GetDwAbilitySystem() : nullptr;
	const FGameplayTag AbilityTag = Command.GetAbilityTag();
	FGameplayAbilitySpec* Spec = Asc ? Asc->FindSpecByAbilityTag(AbilityTag) : nullptr;
	if (!Spec)
	{
		return EDwCommandResult::UnknownAbility;
	}
	const FGameplayAbilitySpecHandle SpecHandle = Spec->Handle;

	Record.Commands.Add(Command);
	bResolving = true;
	ResolvingCommand = Command;
	ResolvingUnit = Command.Unit;
	ResolvingSpec = SpecHandle;

	FDwCombatEvent Used;
	Used.Type = EDwCombatEventType::AbilityUsed;
	Used.Source = Command.Unit;
	Used.Target = Command.Target.Unit;
	Used.Tag = AbilityTag;
	Used.Tile = Command.Target.Tile;
	Emit(Used);

	FGameplayEventData Payload;
	Payload.EventTag = DwTags::Event_Ability_Activate;
	Payload.Instigator = Unit;
	Payload.Target = GetTargetActor(Command.Target);
	Payload.TargetData = FDwGameplayAbilityTargetData_Grid::MakeHandle(Command.Target, Command.ItemId);

	bool bActivated = false;
	{
		TGuardValue<int32> Depth(SubmitDepth, SubmitDepth + 1);
		bActivated = Asc->TriggerAbilityFromGameplayEvent(SpecHandle, Asc->AbilityActorInfo.Get(), DwTags::Event_Ability_Activate, &Payload, *Asc);
	}
	if (!bActivated)
	{
		// GAS refused (a check our validation doesn't mirror). Nothing happened; forget the command.
		if (bResolving && ResolvingSpec == SpecHandle)
		{
			bResolving = false;
			Record.Commands.Pop();
		}
		UE_LOG(LogDwCombat, Warning, TEXT("GAS refused to activate %s"), *Command.ToString());
		return EDwCommandResult::Blocked;
	}
	return EDwCommandResult::Accepted;
}

void UDwCombatSubsystem::HandleAbilityEnded(UDwAbilitySystemComponent* Owner, FGameplayAbilitySpecHandle Ability)
{
	const ADwCombatUnit* Unit = FindUnit(ResolvingUnit);
	if (bResolving && Unit && Unit->GetDwAbilitySystem() == Owner && Ability == ResolvingSpec)
	{
		FinishCommand();
	}
}

void UDwCombatSubsystem::FinishCommand()
{
	const FDwCombatCommand Command = ResolvingCommand;
	const ADwCombatUnit* Unit = FindUnit(ResolvingUnit);
	const EDwSide Side = Unit ? Unit->GetSide() : GetActiveSide();
	bResolving = false;
	ResolvingSpec = FGameplayAbilitySpecHandle();
	ResolvingUnit = FDwUnitHandle();

	RefreshDerivedState();
	const bool bEnded = EvaluateOutcome();
	OnCommandResolved.Broadcast(Command);
	if (bEnded)
	{
		OnCombatEnded.Broadcast(Result);
		return;
	}
	SuggestEndTurnIfIdle();

	// Latent resolution: hand control back to the side's controller. Synchronous resolution returns to the caller instead.
	if (SubmitDepth == 0)
	{
		if (IDwCombatSideController* Controller = GetSideController(Side))
		{
			Controller->OnCommandResolved(*this);
		}
	}
}

void UDwCombatSubsystem::SuggestEndTurnIfIdle()
{
	if (bEndTurnSuggested || Outcome != EDwCombatOutcome::None || GetActiveSide() != EDwSide::Player || bResolving)
	{
		return;
	}
	if (!FDwCombatQueries::SideHasAvailableAction(GetSnapshot(), EDwSide::Player))
	{
		bEndTurnSuggested = true;
		OnEndTurnSuggested.Broadcast();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Turn steps

void UDwCombatSubsystem::HandleSideTurnBegin(EDwSide Side)
{
	bEndTurnSuggested = false;
	CannonsUsed.Reset();
	const TSubclassOf<UGameplayEffect> Refresh = DwGas::GetSharedEffects().TurnBudgetRefresh;
	for (ADwCombatUnit* Unit : Units)
	{
		if (Unit->GetSide() == Side && Unit->IsAlive())
		{
			Unit->GetDwAbilitySystem()->HandleTurnBoundary(EDwTurnTickPoint::OwnerSideTurnStart);
			ApplyEffectToSelf(*Unit->GetDwAbilitySystem(), Refresh);
		}
	}
	if (ADwShip* Ship = GetShip(Side))
	{
		Ship->GetDwAbilitySystem()->HandleTurnBoundary(EDwTurnTickPoint::OwnerSideTurnStart);
	}

	FDwCombatEvent Started;
	Started.Type = EDwCombatEventType::TurnStarted;
	Started.Side = Side;
	Started.Magnitude = static_cast<float>(TurnManager ? TurnManager->GetRound() : 1);
	Emit(Started);

	CheckOutcome();
	if (Side == EDwSide::Player)
	{
		SuggestEndTurnIfIdle(); // e.g. the whole crew is frozen
	}
}

void UDwCombatSubsystem::HandleSideTurnEnd(EDwSide Side)
{
	for (ADwCombatUnit* Unit : Units)
	{
		if (Unit->GetSide() == Side && Unit->IsAlive())
		{
			Unit->GetDwAbilitySystem()->HandleTurnBoundary(EDwTurnTickPoint::OwnerSideTurnEnd);
		}
	}
	if (ADwShip* Ship = GetShip(Side))
	{
		Ship->GetDwAbilitySystem()->HandleTurnBoundary(EDwTurnTickPoint::OwnerSideTurnEnd);
	}

	// Cowering lasts exactly the turn it skipped; then the Fear policy applies (K8).
	for (auto It = CoweringEffects.CreateIterator(); It; ++It)
	{
		ADwCombatUnit* Unit = FindUnit(It.Key());
		if (!Unit || Unit->GetSide() != Side)
		{
			continue;
		}
		UDwAbilitySystemComponent* Asc = Unit->GetDwAbilitySystem();
		Asc->RemoveActiveGameplayEffect(It.Value());
		if (Unit->IsAlive())
		{
			const float Fear = FDwFearRules::FearAfterCower(Asc->GetNumericAttribute(UDwUnitAttributeSet::GetFearAttribute()), Setup.FearCowerPolicy);
			Asc->SetNumericAttributeBase(UDwUnitAttributeSet::GetFearAttribute(), Fear);
		}
		It.RemoveCurrent();
	}
	CheckOutcome();
}

bool UDwCombatSubsystem::BeginEnemyUnitTurn(FDwUnitHandle Handle)
{
	ADwCombatUnit* Unit = FindUnit(Handle);
	if (!Unit || !Unit->IsAlive())
	{
		return false;
	}
	UDwAbilitySystemComponent* Asc = Unit->GetDwAbilitySystem();
	FDwCombatEvent Skipped;
	Skipped.Type = EDwCombatEventType::UnitTurnSkipped;
	Skipped.Target = Handle;

	const float Fear = Asc->GetNumericAttribute(UDwUnitAttributeSet::GetFearAttribute());
	const float MaxFear = Asc->GetNumericAttribute(UDwUnitAttributeSet::GetMaxFearAttribute());
	if (!CoweringEffects.Contains(Handle) && FDwFearRules::ShouldCower(Fear, MaxFear, Asc->HasMatchingGameplayTag(DwTags::Unit_Immune_Fear)))
	{
		CoweringEffects.Add(Handle, ApplyEffectToSelf(*Asc, DwGas::GetSharedEffects().Cowering));
		Skipped.Tag = DwTags::Status_Incapacitated_Cowering;
		Emit(Skipped);
		return false;
	}
	if (Asc->HasMatchingGameplayTag(DwTags::Status_Incapacitated))
	{
		Skipped.Tag = Asc->HasMatchingGameplayTag(DwTags::Status_Incapacitated_Frozen) ? DwTags::Status_Incapacitated_Frozen : DwTags::Status_Incapacitated;
		Emit(Skipped);
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Death, sinking and events

void UDwCombatSubsystem::HandleHealthDepleted(UDwAbilitySystemComponent* Victim, AActor* Instigator)
{
	if (ADwCombatUnit* Unit = FindUnitByAbilitySystem(Victim))
	{
		HandleUnitDied(*Unit, Instigator);
	}
	else if (ADwShip* Ship = Victim ? Cast<ADwShip>(Victim->GetOwnerActor()) : nullptr)
	{
		HandleShipSunk(*Ship);
	}
}

void UDwCombatSubsystem::HandleUnitDied(ADwCombatUnit& Victim, AActor* Instigator)
{
	// Dead units stay registered (results, run summary) but leave the grid.
	if (Grid.GetOccupant(Victim.GetTile()).Unit == Victim.GetHandle())
	{
		Grid.ClearOccupant(Victim.GetTile());
	}

	// Kill credit goes to the effect's instigator: a damage-over-time kill credits its caster; sinking credits nobody (C11).
	FDwCombatEvent Died;
	Died.Type = EDwCombatEventType::UnitDied;
	Died.Target = Victim.GetHandle();
	Died.Tile = Victim.GetTile();
	const ADwCombatUnit* Killer = Cast<ADwCombatUnit>(Instigator);
	if (Killer && Killer != &Victim)
	{
		FDwKillRecord& Kill = Kills.AddDefaulted_GetRef();
		Kill.Killer = Killer->GetHandle();
		Kill.KillerRosterId = Killer->GetRosterId();
		Kill.Victim = Victim.GetHandle();
		Kill.VictimDefinition = Victim.GetDefinition() ? Victim.GetDefinition()->GetPrimaryAssetId() : FPrimaryAssetId();
		Kill.VictimSide = Victim.GetSide();
		Died.Source = Killer->GetHandle();
		// Conqueror kill bonus (FDwCaptainRules::OnEnemyKilled) hooks in here with DW-208.
	}
	UE_LOG(LogDwCombat, Log, TEXT("Unit %d (%s) died, killer %d"), Victim.GetHandle().Id, *DwCombatSubsystem::SideName(Victim.GetSide()), Died.Source.Id);

	FGameplayCueParameters Cue;
	Cue.Location = Victim.GetActorLocation();
	Victim.GetDwAbilitySystem()->ExecuteGameplayCue(DwTags::GameplayCue_Unit_Death, Cue);
	Emit(Died);
	Victim.OnDied();
}

void UDwCombatSubsystem::HandleShipSunk(ADwShip& Ship)
{
	const EDwSide Side = Ship.GetSide();
	UE_LOG(LogDwCombat, Log, TEXT("%s ship sunk"), *DwCombatSubsystem::SideName(Side));
	FDwCombatEvent Sunk;
	Sunk.Type = EDwCombatEventType::ShipSunk;
	Sunk.Side = Side;
	Emit(Sunk);
	Ship.GetDwAbilitySystem()->ExecuteGameplayCue(DwTags::GameplayCue_Ship_Sink, FGameplayCueParameters());
	Ship.OnSunk();

	// Everyone standing on that ship's tiles goes down with it - its crew and any boarders. No kill credit (C11).
	const TSubclassOf<UGameplayEffect> Kill = DwGas::GetSharedEffects().Kill;
	for (ADwCombatUnit* Unit : TArray<TObjectPtr<ADwCombatUnit>>(Units))
	{
		const FDwTile* Tile = Grid.GetTile(Unit->GetTile());
		if (Unit->IsAlive() && Tile && Tile->Owner == Side)
		{
			ApplyEffectToSelf(*Unit->GetDwAbilitySystem(), Kill, /*bNoInstigator*/ true);
		}
	}
}

void UDwCombatSubsystem::HandleUnitHealthChanged(const FOnAttributeChangeData& Data, FDwUnitHandle Unit)
{
	const float Delta = Data.NewValue - Data.OldValue;
	if (FMath::IsNearlyZero(Delta))
	{
		return;
	}
	FDwCombatEvent Event;
	Event.Type = Delta < 0.f ? EDwCombatEventType::UnitDamaged : EDwCombatEventType::UnitHealed;
	Event.Target = Unit;
	Event.Magnitude = FMath::Abs(Delta);
	const ADwCombatUnit* Target = FindUnit(Unit);
	if (const ADwCombatUnit* Source = Cast<ADwCombatUnit>(DwCombatSubsystem::GetInstigator(Data, Target ? Target->GetDwAbilitySystem() : nullptr)))
	{
		Event.Source = Source->GetHandle();
	}
	Emit(Event);
}

void UDwCombatSubsystem::HandleUnitFearChanged(const FOnAttributeChangeData& Data, FDwUnitHandle Unit)
{
	if (!FMath::IsNearlyEqual(Data.NewValue, Data.OldValue))
	{
		FDwCombatEvent Event;
		Event.Type = EDwCombatEventType::FearChanged;
		Event.Target = Unit;
		Event.Magnitude = Data.NewValue - Data.OldValue;
		Emit(Event);
	}
}

void UDwCombatSubsystem::HandleShipHealthChanged(const FOnAttributeChangeData& Data, EDwSide Side)
{
	if (Data.NewValue < Data.OldValue)
	{
		FDwCombatEvent Event;
		Event.Type = EDwCombatEventType::ShipDamaged;
		Event.Side = Side;
		Event.Magnitude = Data.OldValue - Data.NewValue;
		const ADwShip* Ship = GetShip(Side);
		if (const ADwCombatUnit* Source = Cast<ADwCombatUnit>(DwCombatSubsystem::GetInstigator(Data, Ship ? Ship->GetDwAbilitySystem() : nullptr)))
		{
			Event.Source = Source->GetHandle();
		}
		Emit(Event);
	}
}

void UDwCombatSubsystem::HandleUnitTagChanged(const FGameplayTag Tag, int32 NewCount, FDwUnitHandle Unit)
{
	if (DwCombatSubsystem::IsReportedStatus(Tag))
	{
		FDwCombatEvent Event;
		Event.Type = NewCount > 0 ? EDwCombatEventType::StatusApplied : EDwCombatEventType::StatusRemoved;
		Event.Target = Unit;
		Event.Tag = Tag;
		Emit(Event);
	}
}

void UDwCombatSubsystem::Emit(const FDwCombatEvent& Event)
{
	OnCombatEvent.Broadcast(Event);
}

// ---------------------------------------------------------------------------------------------------------------------
// Derived state and outcome

void UDwCombatSubsystem::RefreshDerivedState()
{
	for (ADwCombatUnit* Unit : Units)
	{
		if (!Unit->IsAlive())
		{
			continue;
		}
		const FDwTile* Tile = Grid.GetTile(Unit->GetTile());
		const bool bOnEnemyDeck = Tile && Tile->Owner != EDwSide::Neutral && Tile->Owner != Unit->GetSide();
		const bool bInNest = Tile && Tile->Elevation > 0;
		UDwAbilitySystemComponent* Asc = Unit->GetDwAbilitySystem();
		Asc->SetLooseGameplayTagCount(DwTags::State_OnEnemyDeck, bOnEnemyDeck ? 1 : 0);
		Asc->SetLooseGameplayTagCount(DwTags::State_InNest, bInNest ? 1 : 0);
		// State.Synergy.Adjacent (UI indicator) joins here with the Team Leader, DW-207.
	}
}

bool UDwCombatSubsystem::EvaluateOutcome()
{
	if (Outcome != EDwCombatOutcome::None || !bActive)
	{
		return false;
	}
	const EDwCombatOutcome NewOutcome = FDwCombatOutcomeRules::Evaluate(GetSnapshot(), Setup.OutcomeConfig);
	if (NewOutcome == EDwCombatOutcome::None)
	{
		return false;
	}
	Outcome = NewOutcome;
	if (TurnManager)
	{
		TurnManager->Stop();
	}
	BuildResult();
	UE_LOG(LogDwCombat, Log, TEXT("Outcome: %s"), *Result.ToString());

	FDwCombatEvent Reached;
	Reached.Type = EDwCombatEventType::OutcomeReached;
	Reached.Magnitude = static_cast<float>(static_cast<uint8>(Outcome));
	Emit(Reached);
	return true;
}

void UDwCombatSubsystem::CheckOutcome()
{
	if (EvaluateOutcome())
	{
		OnCombatEnded.Broadcast(Result);
	}
}

void UDwCombatSubsystem::BuildResult()
{
	Result = FDwCombatResult();
	Result.Outcome = Outcome;
	Result.Rounds = TurnManager ? TurnManager->GetRound() : 0;
	for (int32 Index = 0; Index < PlayerCrewCount && Index < Units.Num(); ++Index)
	{
		const ADwCombatUnit* Unit = Units[Index];
		FDwCrewResult& Crew = Result.Crew.AddDefaulted_GetRef();
		Crew.RosterId = Unit->GetRosterId();
		Crew.bAlive = Unit->IsAlive();
		Crew.Health = Crew.bAlive ? Unit->GetAttributes()->GetHealth() : 0.f;
	}
	Result.Kills = Kills;
	Result.PlayerShipHealth = PlayerShip ? PlayerShip->GetHealth() : 0.f;
	Result.ItemsUsed = ItemsUsed;
}

// ---------------------------------------------------------------------------------------------------------------------
// Queries

FDwCombatSnapshot UDwCombatSubsystem::GetSnapshot() const
{
	FDwCombatSnapshot Snapshot;
	if (!bActive)
	{
		return Snapshot;
	}
	Snapshot.Grid = Grid;
	Snapshot.GridRules = GridRules;
	Snapshot.Round = GetRound();
	Snapshot.ActiveSide = GetActiveSide();
	Snapshot.ActingUnit = TurnManager ? TurnManager->GetActingUnit() : FDwUnitHandle();
	Snapshot.OutcomeConfig = Setup.OutcomeConfig;
	Snapshot.CannonsUsed = CannonsUsed;
	Snapshot.Items = Items;
	Snapshot.ItemDefinitions = ItemDefinitions;
	Snapshot.Outcome = Outcome;

	const FGameplayEffectQuery GuardQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(DwTags::Status_Guarded));
	Snapshot.Units.Reserve(Units.Num());
	for (const ADwCombatUnit* Unit : Units)
	{
		const UDwAbilitySystemComponent* Asc = Unit->GetDwAbilitySystem();
		const UDwUnitAttributeSet* A = Unit->GetAttributes();
		FDwUnitSnapshot& S = Snapshot.Units.AddDefaulted_GetRef();
		S.Handle = Unit->GetHandle();
		S.Side = Unit->GetSide();
		S.Tile = Unit->GetTile();
		S.Definition = Unit->GetDefinition() ? Unit->GetDefinition()->GetPrimaryAssetId() : FPrimaryAssetId();
		Asc->GetOwnedGameplayTags(S.Tags);
		S.bAlive = !S.Tags.HasTagExact(DwTags::State_Dead);
		S.Health = A->GetHealth();
		S.MaxHealth = A->GetMaxHealth();
		S.Mana = A->GetMana();
		S.MaxMana = A->GetMaxMana();
		S.Fear = A->GetFear();
		S.MaxFear = A->GetMaxFear();
		S.ActionPoints = A->GetActionPoints();
		S.MovePoints = A->GetMovePoints();
		S.AttackDamage = A->GetAttackDamage();
		S.DamageBonus = A->GetDamageBonus();
		S.DamageReduction = A->GetDamageReduction();
		S.Abilities = Unit->GetGrantedAbilities();
		for (const UDwAbilityDefinition* Ability : S.Abilities)
		{
			S.GrantedAbilityTags.AddTag(Ability->AbilityTag);
		}
		if (S.Tags.HasTagExact(DwTags::Status_Guarded))
		{
			for (const FActiveGameplayEffectHandle& Handle : Asc->GetActiveEffects(GuardQuery))
			{
				const FActiveGameplayEffect* Effect = Asc->GetActiveGameplayEffect(Handle);
				if (const ADwCombatUnit* Guardian = Effect ? Cast<ADwCombatUnit>(Effect->Spec.GetContext().GetOriginalInstigator()) : nullptr)
				{
					S.GuardedBy = Guardian->GetHandle();
				}
			}
		}
	}
	for (const ADwShip* Ship : { PlayerShip.Get(), EnemyShip.Get() })
	{
		if (Ship)
		{
			FDwShipSnapshot& S = Snapshot.Ships.AddDefaulted_GetRef();
			S.Side = Ship->GetSide();
			S.Health = Ship->GetHealth();
			S.MaxHealth = Ship->GetMaxHealth();
			S.CannonDamage = Ship->GetCannonDamage();
			S.CannonTiles = Ship->GetCannonTiles();
		}
	}
	return Snapshot;
}

ADwCombatUnit* UDwCombatSubsystem::FindUnit(FDwUnitHandle Handle) const
{
	return Units.IsValidIndex(Handle.Id - 1) ? Units[Handle.Id - 1].Get() : nullptr;
}

ADwCombatUnit* UDwCombatSubsystem::FindUnitByAbilitySystem(const UDwAbilitySystemComponent* AbilitySystem) const
{
	ADwCombatUnit* Unit = AbilitySystem ? Cast<ADwCombatUnit>(AbilitySystem->GetOwnerActor()) : nullptr;
	return Unit && FindUnit(Unit->GetHandle()) == Unit ? Unit : nullptr;
}

ADwShip* UDwCombatSubsystem::GetShip(EDwSide Side) const
{
	return Side == EDwSide::Player ? PlayerShip.Get() : (Side == EDwSide::Enemy ? EnemyShip.Get() : nullptr);
}

AActor* UDwCombatSubsystem::GetTargetActor(const FDwAbilityTarget& Target) const
{
	if (Target.bShip)
	{
		return GetShip(Target.ShipSide);
	}
	return Target.Unit.IsValid() ? FindUnit(Target.Unit) : nullptr;
}

int32 UDwCombatSubsystem::GetRound() const
{
	return TurnManager ? TurnManager->GetRound() : 0;
}

EDwSide UDwCombatSubsystem::GetActiveSide() const
{
	return TurnManager ? TurnManager->GetActiveSide() : EDwSide::Player;
}

// ---------------------------------------------------------------------------------------------------------------------
// Ability services

void UDwCombatSubsystem::MoveUnit(FDwUnitHandle Handle, FIntPoint Tile, bool bSnapActor)
{
	ADwCombatUnit* Unit = FindUnit(Handle);
	if (!Unit || !Grid.IsStandable(Tile))
	{
		return;
	}
	if (Grid.GetOccupant(Unit->GetTile()).Unit == Handle)
	{
		Grid.ClearOccupant(Unit->GetTile());
	}
	Grid.SetOccupant(Tile, Handle, Unit->GetSide());
	Unit->SetTile(Tile);
	if (bSnapActor && Board)
	{
		Unit->SetActorLocation(Board->TileToWorld(Tile));
	}

	FDwCombatEvent Moved;
	Moved.Type = EDwCombatEventType::UnitMoved;
	Moved.Source = Handle;
	Moved.Target = Handle;
	Moved.Tile = Tile;
	Emit(Moved);
}

void UDwCombatSubsystem::MarkCannonUsed(FIntPoint CannonTile)
{
	CannonsUsed.AddUnique(CannonTile);
}

bool UDwCombatSubsystem::ConsumeItem(FName ItemId)
{
	int32* Count = Items.Find(ItemId);
	if (!Count || *Count <= 0)
	{
		return false;
	}
	--*Count;
	++ItemsUsed.FindOrAdd(ItemId);
	return true;
}

FActiveGameplayEffectHandle UDwCombatSubsystem::ApplyEffectToSelf(UDwAbilitySystemComponent& Target, TSubclassOf<UGameplayEffect> EffectClass, bool bNoInstigator)
{
	if (!EffectClass)
	{
		return FActiveGameplayEffectHandle();
	}
	FGameplayEffectContextHandle Context = bNoInstigator
		? FGameplayEffectContextHandle(UAbilitySystemGlobals::Get().AllocGameplayEffectContext())
		: Target.MakeEffectContext();
	FGameplayEffectSpecHandle Spec = Target.MakeOutgoingSpec(EffectClass, 1, Context);
	return Spec.IsValid() ? Target.ApplyGameplayEffectSpecToSelf(*Spec.Data) : FActiveGameplayEffectHandle();
}
