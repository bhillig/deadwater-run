// Copyright Brandon Hillig 2026

#include "Combat/DwEncounterSetupBuilder.h"

#include "AI/DwAiProfile.h"
#include "Board/DwBoardBuilder.h"
#include "Board/DwDeckLayout.h"
#include "Board/DwBoard.h"
#include "Combat/DwCombatUnit.h"
#include "Combat/DwShip.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwKrakenDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"

namespace DwEncounterSetupBuilder
{
	static bool ReadShip(const FDataTableRowHandle& Handle, const TSoftClassPtr<ADwShip>& Class, FDwShipSetup& Out, const TCHAR* What, TArray<FString>& OutErrors)
	{
		const FDwShipTuningRow* Row = Handle.DataTable ? Handle.GetRow<FDwShipTuningRow>(TEXT("FDwEncounterSetupBuilder")) : nullptr;
		if (!Row)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: missing DT_ShipTuning row '%s'."), What, *Handle.RowName.ToString()));
			return false;
		}
		Out.MaxHealth = Row->MaxHealth;
		Out.CannonDamage = Row->CannonDamage;
		Out.Health = -1.f;
		Out.ShipClass = Class.LoadSynchronous();
		return true;
	}

	/** Everything both kinds share: crew, seed, presentation, DT_CombatTuning, the player ship, common abilities. */
	static void BuildCommon(const UDwGameData& GameData, const UDwEncounterDefinition& Encounter, const TArray<FDwCombatantSetup>& Crew,
		int32 Seed, FDwEncounterSetup& Out, TArray<FString>& OutErrors)
	{
		Out = FDwEncounterSetup();
		Out.Encounter = &Encounter;
		Out.Seed = Seed;
		Out.Crew = Crew;
		Out.TileSize = GameData.TileSize;
		Out.DeckGap = GameData.DeckGap;
		Out.BoardClass = GameData.BoardClass.LoadSynchronous();

		const FDwCombatTuningRow* Combat = GameData.TuningTables.Combat
			? GameData.TuningTables.Combat->FindRow<FDwCombatTuningRow>(GameData.CombatTuningRow, TEXT("FDwEncounterSetupBuilder")) : nullptr;
		if (Combat)
		{
			Out.GridRules = FDwGridRules::FromTuning(*Combat);
			Out.MaxFear = Combat->MaxFear;
			Out.FearCowerPolicy = Combat->FearCowerPolicy;
		}
		else
		{
			OutErrors.Add(FString::Printf(TEXT("DA_GameData: missing DT_CombatTuning row '%s'."), *GameData.CombatTuningRow.ToString()));
		}

		ReadShip(GameData.PlayerShipTuning, GameData.PlayerShipClass, Out.PlayerShip, TEXT("Player ship"), OutErrors);
		// Each deck layout knows where its ship's hull sits under the grid.
		Out.PlayerShip.MeshAlignment = GameData.DefaultPlayerDeck ? GameData.DefaultPlayerDeck->MeshAlignment : FTransform::Identity;

		for (const UDwAbilityDefinition* Ability : GameData.CommonAbilities)
		{
			if (Ability)
			{
				Out.CommonAbilities.Add(Ability);
			}
		}
	}
}

bool FDwEncounterSetupBuilder::Build(const UDwGameData& GameData, const UDwEncounterDefinition& Encounter, const TArray<FDwCombatantSetup>& Crew,
	int32 Seed, FDwEncounterSetup& Out, TArray<FString>& OutErrors)
{
	switch (Encounter.Kind)
	{
	case EDwEncounterKind::ShipBattle: return BuildShipBattle(GameData, Encounter, Crew, Seed, Out, OutErrors);
	case EDwEncounterKind::Kraken:     return BuildKraken(GameData, Encounter, Crew, Seed, Out, OutErrors);
	}
	OutErrors.Add(FString::Printf(TEXT("%s: unknown encounter kind."), *Encounter.GetName()));
	return false;
}

bool FDwEncounterSetupBuilder::BuildShipBattle(const UDwGameData& GameData, const UDwEncounterDefinition& Encounter, const TArray<FDwCombatantSetup>& Crew,
	int32 Seed, FDwEncounterSetup& Out, TArray<FString>& OutErrors)
{
	using namespace DwEncounterSetupBuilder;
	const int32 ErrorsBefore = OutErrors.Num();
	if (Encounter.Kind != EDwEncounterKind::ShipBattle)
	{
		Out = FDwEncounterSetup();
		OutErrors.Add(FString::Printf(TEXT("%s is not a ship battle (use Build or BuildKraken)."), *Encounter.GetName()));
		return false;
	}
	BuildCommon(GameData, Encounter, Crew, Seed, Out, OutErrors);

	if (!GameData.DefaultPlayerDeck || !Encounter.ShipDeck)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: needs DA_GameData.DefaultPlayerDeck and its own ShipDeck."), *Encounter.GetName()));
	}
	else
	{
		Out.Grid = FDwBoardBuilder::BuildShipBattle(*GameData.DefaultPlayerDeck, *Encounter.ShipDeck);
	}

	Out.bHasEnemyShip = true;
	ReadShip(Encounter.ShipTuning, Encounter.ShipClass, Out.EnemyShip, *Encounter.GetName(), OutErrors);
	Out.EnemyShip.MeshAlignment = Encounter.ShipDeck ? Encounter.ShipDeck->MeshAlignment : FTransform::Identity;

	for (const FDwEnemySlot& Slot : Encounter.Units)
	{
		if (!Slot.UnitDefinition)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: an enemy slot has no unit definition."), *Encounter.GetName()));
			continue;
		}
		FDwCombatantSetup& Enemy = Out.Enemies.AddDefaulted_GetRef();
		Enemy.Definition = Slot.UnitDefinition;
		Enemy.Level = Slot.Level;
		Enemy.UnitClassOverride = Slot.UnitClassOverride.LoadSynchronous();
		Enemy.AiProfile = Slot.AiProfileOverride ? Slot.AiProfileOverride.Get() : Encounter.AiProfile.Get();
	}
	return OutErrors.Num() == ErrorsBefore;
}

bool FDwEncounterSetupBuilder::BuildKraken(const UDwGameData& GameData, const UDwEncounterDefinition& Encounter, const TArray<FDwCombatantSetup>& Crew,
	int32 Seed, FDwEncounterSetup& Out, TArray<FString>& OutErrors)
{
	using namespace DwEncounterSetupBuilder;
	const int32 ErrorsBefore = OutErrors.Num();
	if (Encounter.Kind != EDwEncounterKind::Kraken)
	{
		Out = FDwEncounterSetup();
		OutErrors.Add(FString::Printf(TEXT("%s is not a Kraken encounter."), *Encounter.GetName()));
		return false;
	}
	BuildCommon(GameData, Encounter, Crew, Seed, Out, OutErrors);

	const UDwKrakenDefinition* Kraken = Encounter.Kraken;
	if (!Kraken || !Kraken->Body || !Kraken->Arena)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: needs a Kraken definition with a Body and an Arena."), *Encounter.GetName()));
		return false;
	}
	if (!GameData.DefaultPlayerDeck)
	{
		OutErrors.Add(TEXT("DA_GameData: needs DefaultPlayerDeck."));
	}
	else
	{
		Out.Grid = FDwBoardBuilder::BuildKrakenArena(*GameData.DefaultPlayerDeck, *Kraken->Arena);
	}
	const FDwKrakenTuningRow* Tuning = Kraken->GetTuningRow<FDwKrakenTuningRow>();
	const int32 TentacleCount = Tuning ? Tuning->TentacleCount : 0;
	if (TentacleCount > 0 && !Kraken->Tentacle)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: %d tentacles in DT_KrakenTuning but no Tentacle unit."), *Kraken->GetName(), TentacleCount));
	}

	// The body on its anchor, tentacles on the slots by the rail in board order (Docs/Architecture/08 "Arena build").
	TOptional<FIntPoint> Anchor;
	TArray<FIntPoint> Slots;
	for (int32 Index = 0; Index < Out.Grid.Tiles.Num(); ++Index)
	{
		const FDwTile& Tile = Out.Grid.Tiles[Index];
		if (Tile.Tags.HasTagExact(DwTags::Tile_Kraken_Body) && !Anchor.IsSet()) { Anchor = Out.Grid.ToTile(Index); }
		if (Tile.Tags.HasTagExact(DwTags::Tile_Kraken_Tentacle)) { Slots.Add(Out.Grid.ToTile(Index)); }
	}
	if (!Anchor.IsSet())
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the arena has no body anchor (B)."), *Kraken->Arena->GetName()));
	}
	if (Slots.Num() < TentacleCount)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: %d tentacles need %d tentacle slots (T), the arena has %d."), *Kraken->GetName(), TentacleCount,
			TentacleCount, Slots.Num()));
	}

	// No enemy ship: no cannons to aim at a hull and no sinking win (Docs/Architecture/08 "Arena build").
	Out.bHasEnemyShip = false;
	Out.Kraken = Kraken;
	FDwCombatantSetup& Body = Out.Enemies.AddDefaulted_GetRef();
	Body.Definition = Kraken->Body;
	Body.AiProfile = Encounter.AiProfile; // the crew brain until DW-302's Kraken brain
	Body.bUseSpawnTile = Anchor.IsSet();
	Body.SpawnTile = Anchor.Get(FIntPoint::ZeroValue);
	for (int32 Index = 0; Index < TentacleCount && Index < Slots.Num() && Kraken->Tentacle; ++Index)
	{
		FDwCombatantSetup& Tentacle = Out.Enemies.AddDefaulted_GetRef();
		Tentacle.Definition = Kraken->Tentacle;
		Tentacle.bUseSpawnTile = true;
		Tentacle.SpawnTile = Slots[Index];
		// No AI profile: tentacles never act; they are targets (08 "Behaviour").
	}
	Out.OutcomeConfig.bIsBossFight = true;
	return OutErrors.Num() == ErrorsBefore;
}
