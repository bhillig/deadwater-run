// Copyright Brandon Hillig 2026

#include "Combat/DwEncounterSetupBuilder.h"

#include "Board/DwBoardBuilder.h"
#include "Board/DwDeckLayout.h"
#include "Combat/DwShip.h"
#include "Core/DwGameData.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwEncounterDefinition.h"
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
}

bool FDwEncounterSetupBuilder::BuildShipBattle(const UDwGameData& GameData, const UDwEncounterDefinition& Encounter, const TArray<FDwCombatantSetup>& Crew,
	int32 Seed, FDwEncounterSetup& Out, TArray<FString>& OutErrors)
{
	using namespace DwEncounterSetupBuilder;
	const int32 ErrorsBefore = OutErrors.Num();
	Out = FDwEncounterSetup();
	Out.Encounter = &Encounter;
	Out.Seed = Seed;
	Out.Crew = Crew;
	Out.TileSize = GameData.TileSize;

	if (Encounter.Kind != EDwEncounterKind::ShipBattle)
	{
		OutErrors.Add(FString::Printf(TEXT("%s is not a ship battle (the Kraken arena is DW-301)."), *Encounter.GetName()));
		return false;
	}
	if (!GameData.DefaultPlayerDeck || !Encounter.ShipDeck)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: needs DA_GameData.DefaultPlayerDeck and its own ShipDeck."), *Encounter.GetName()));
	}
	else
	{
		Out.Grid = FDwBoardBuilder::BuildShipBattle(*GameData.DefaultPlayerDeck, *Encounter.ShipDeck);
	}

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
	Out.bHasEnemyShip = true;
	ReadShip(Encounter.ShipTuning, Encounter.ShipClass, Out.EnemyShip, *Encounter.GetName(), OutErrors);

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
	}

	for (const UDwAbilityDefinition* Ability : GameData.CommonAbilities)
	{
		if (Ability)
		{
			Out.CommonAbilities.Add(Ability);
		}
	}
	return OutErrors.Num() == ErrorsBefore;
}
