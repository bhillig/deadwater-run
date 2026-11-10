// Copyright Brandon Hillig 2026

#include "Run/Map/DwPlaceholderMap.h"

#include "Data/DwEncounterDefinition.h"
#include "Data/DwMapRules.h"

FDwMapGraph FDwPlaceholderMap::Build(int32 MapSeed, const UDwMapRules* Rules, TConstArrayView<const UDwEncounterDefinition*> EncounterPool,
	const UDwEncounterDefinition* Boss)
{
	const UDwMapRules* Defaults = GetDefault<UDwMapRules>();
	const int32 Layers = FMath::Max(1, (Rules ? Rules : Defaults)->MinPathNodes);
	const int32 Lanes = FMath::Max(1, (Rules ? Rules : Defaults)->LaneCount);
	FRandomStream Random(MapSeed);

	FDwMapGraph Graph;
	Graph.LayerCount = Layers;
	Graph.Nodes.Reserve(Layers * Lanes + 1);
	for (int32 Layer = 0; Layer < Layers; ++Layer)
	{
		for (int32 Lane = 0; Lane < Lanes; ++Lane)
		{
			FDwMapNode& Node = Graph.Nodes.AddDefaulted_GetRef();
			Node.Layer = Layer;
			Node.Lane = Lane;
			Node.Type = EDwNodeType::Fight;
			Node.Encounter = PickEncounter(EncounterPool, Layer, Random);
		}
	}
	Graph.BossNode = Graph.Nodes.Num();
	FDwMapNode& BossNode = Graph.Nodes.AddDefaulted_GetRef();
	BossNode.Layer = Layers;
	BossNode.Lane = Lanes / 2;
	BossNode.Type = EDwNodeType::Boss;
	BossNode.Encounter = Boss;

	for (int32 Layer = 0; Layer < Layers; ++Layer)
	{
		for (int32 Lane = 0; Lane < Lanes; ++Lane)
		{
			TArray<int32>& Next = Graph.Nodes[Layer * Lanes + Lane].Next;
			if (Layer == Layers - 1)
			{
				Next.Add(Graph.BossNode);
				continue;
			}
			for (int32 To = FMath::Max(0, Lane - 1); To <= FMath::Min(Lanes - 1, Lane + 1); ++To)
			{
				Next.Add((Layer + 1) * Lanes + To);
			}
		}
	}
	return Graph;
}

const UDwEncounterDefinition* FDwPlaceholderMap::PickEncounter(TConstArrayView<const UDwEncounterDefinition*> Pool, int32 Layer, FRandomStream& Random)
{
	float Total = 0.f;
	for (const UDwEncounterDefinition* Encounter : Pool)
	{
		if (Encounter && Encounter->Kind == EDwEncounterKind::ShipBattle && Layer >= Encounter->MinLayer && Layer <= Encounter->MaxLayer)
		{
			Total += FMath::Max(0.f, Encounter->Weight);
		}
	}
	// One draw per node whatever the pool holds, so an empty tier never shifts the picks of later nodes.
	const float Roll = Random.FRand() * Total;
	if (Total <= 0.f)
	{
		return nullptr;
	}
	float Acc = 0.f;
	const UDwEncounterDefinition* Last = nullptr;
	for (const UDwEncounterDefinition* Encounter : Pool)
	{
		if (Encounter && Encounter->Kind == EDwEncounterKind::ShipBattle && Layer >= Encounter->MinLayer && Layer <= Encounter->MaxLayer
			&& Encounter->Weight > 0.f)
		{
			Last = Encounter;
			Acc += Encounter->Weight;
			if (Roll < Acc)
			{
				return Encounter;
			}
		}
	}
	return Last; // Roll == Total (float edge)
}
