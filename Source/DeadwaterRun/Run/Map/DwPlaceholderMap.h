// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Run/DwRunTypes.h"

class UDwEncounterDefinition;
class UDwMapRules;

/**
 * Stand-in for FDwMapGenerator (DW-202) so the run loop is playable: the 09 D2 shape (LaneCount lanes x MinPathNodes
 * layers, each node linked to lanes j-1, j, j+1 of the next layer, every last-layer node linked to the boss) with every
 * node a fight. Fights get a weighted, layer-tiered pick from the encounter pool. Seeded and deterministic.
 * DW-202 replaces the call in UDwRun::Initialize with generate + validate; the graph types stay.
 */
struct DEADWATERRUN_API FDwPlaceholderMap
{
	/** Rules null = the design defaults (8 layers, 3 lanes). Boss may be null until the Kraken encounter exists (DW-209). */
	static FDwMapGraph Build(int32 MapSeed, const UDwMapRules* Rules, TConstArrayView<const UDwEncounterDefinition*> EncounterPool,
		const UDwEncounterDefinition* Boss);

	/** A weighted pick of a ship battle allowed on Layer (MinLayer..MaxLayer), or null if none is. */
	static const UDwEncounterDefinition* PickEncounter(TConstArrayView<const UDwEncounterDefinition*> Pool, int32 Layer, FRandomStream& Random);
};
