// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Run/DwRunTypes.h"

class UDwEncounterDefinition;
class UDwMapRules;

/**
 * Seeded sea chart (Docs/Architecture/09-Run-Map.md "Map generation", D2). Pure and deterministic: the same seed, rules
 * and pool give the same graph on every platform.
 *
 * Shape: LayerCount (MinPathNodes..MaxPathNodes, from the seed) layers of LaneCount lanes; each node links to lanes
 * j-1, j, j+1 of the next layer; every last-layer node links to the boss. Every path is therefore LayerCount nodes long.
 * Types: the first layer is FirstLayerType; with bShopOptionBeforeBoss the middle lane of the last layer is a shop
 * (every second-to-last node links to it). Other slots are drawn by TypeWeights, never making Shop -> Shop or
 * Event -> Event, and a slot is forced to Fight whenever a non-fight could leave some path short of MinFightsPerPath.
 * So the rules hold by construction; FDwMapValidator proves it, and Build retries with a derived seed if it ever doesn't.
 */
struct DEADWATERRUN_API FDwMapGenerator
{
	/** One graph from MapSeed. Rules is required (UDwRun passes the class defaults when DA_GameData has none). */
	static FDwMapGraph Generate(int32 MapSeed, const UDwMapRules& Rules, TConstArrayView<const UDwEncounterDefinition*> EncounterPool,
		const UDwEncounterDefinition* Boss);

	/**
	 * Generate + FDwMapValidator with FDwRunRandom::ForMap(RunSeed, Attempt) until the graph is valid or MaxAttempts run
	 * out. Returns the first valid graph; on failure the last one, with its problems in OutProblems.
	 */
	static FDwMapGraph Build(int32 RunSeed, const UDwMapRules& Rules, TConstArrayView<const UDwEncounterDefinition*> EncounterPool,
		const UDwEncounterDefinition* Boss, TArray<FString>* OutProblems = nullptr, int32* OutAttempts = nullptr);

	/**
	 * A weighted pick of a ship battle allowed on Layer (MinLayer..MaxLayer), preferring ones not in Avoid (the
	 * previous nodes' encounters: no same crew twice in a row while the tier has another). Null if none is allowed.
	 * Always draws exactly one number, so an empty tier never shifts later picks.
	 */
	static const UDwEncounterDefinition* PickEncounter(TConstArrayView<const UDwEncounterDefinition*> Pool, int32 Layer, FRandomStream& Random,
		TConstArrayView<const UDwEncounterDefinition*> Avoid = {});
};

/** Checks a graph against UDwMapRules (09 "FDwMapValidator"). Reports every broken rule, not just the first. */
struct DEADWATERRUN_API FDwMapValidator
{
	/** Empty = valid. One readable line per problem, naming the node index where there is one. */
	static TArray<FString> Validate(const FDwMapGraph& Graph, const UDwMapRules& Rules);
};
