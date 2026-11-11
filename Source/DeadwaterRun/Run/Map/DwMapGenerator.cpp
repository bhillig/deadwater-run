// Copyright Brandon Hillig 2026

#include "Run/Map/DwMapGenerator.h"

#include "Data/DwEncounterDefinition.h"
#include "Data/DwMapRules.h"
#include "Run/DwRunRandom.h"

namespace DwMapGen
{
	static FString TypeName(EDwNodeType Type)
	{
		return StaticEnum<EDwNodeType>()->GetNameStringByValue(static_cast<int64>(Type));
	}

	static bool Allowed(const UDwEncounterDefinition* Encounter, int32 Layer)
	{
		return Encounter && Encounter->Kind == EDwEncounterKind::ShipBattle && Encounter->Weight > 0.f && Layer >= Encounter->MinLayer
			&& Layer <= Encounter->MaxLayer;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Generator

FDwMapGraph FDwMapGenerator::Generate(int32 MapSeed, const UDwMapRules& Rules, TConstArrayView<const UDwEncounterDefinition*> EncounterPool,
	const UDwEncounterDefinition* Boss)
{
	FRandomStream Random(MapSeed);
	const int32 MinLayers = FMath::Max(1, Rules.MinPathNodes);
	const int32 Layers = Random.RandRange(MinLayers, FMath::Max(MinLayers, Rules.MaxPathNodes)); // M1: 8 or 9 per seed
	const int32 Lanes = FMath::Max(1, Rules.LaneCount);
	auto Index = [Lanes](int32 Layer, int32 Lane) { return Layer * Lanes + Lane; };

	// Shape.
	FDwMapGraph Graph;
	Graph.LayerCount = Layers;
	Graph.Nodes.SetNum(Layers * Lanes + 1);
	Graph.BossNode = Layers * Lanes;
	TArray<TArray<int32>> Parents;
	Parents.SetNum(Graph.Nodes.Num());
	for (int32 Layer = 0; Layer < Layers; ++Layer)
	{
		for (int32 Lane = 0; Lane < Lanes; ++Lane)
		{
			FDwMapNode& Node = Graph.Nodes[Index(Layer, Lane)];
			Node.Layer = Layer;
			Node.Lane = Lane;
			if (Layer == Layers - 1)
			{
				Node.Next.Add(Graph.BossNode);
				continue;
			}
			for (int32 To = FMath::Max(0, Lane - 1); To <= FMath::Min(Lanes - 1, Lane + 1); ++To)
			{
				Node.Next.Add(Index(Layer + 1, To));
				Parents[Index(Layer + 1, To)].Add(Index(Layer, Lane));
			}
		}
	}
	FDwMapNode& BossNode = Graph.Nodes[Graph.BossNode];
	BossNode.Layer = Layers;
	BossNode.Lane = Lanes / 2;
	BossNode.Type = EDwNodeType::Boss;
	BossNode.Encounter = Boss;

	// Types, layer by layer. MinFights[n] = the fewest fights on any path from the start through n (inclusive).
	const int32 ForcedShop = Rules.bShopOptionBeforeBoss ? Index(Layers - 1, Lanes / 2) : INDEX_NONE;
	TArray<int32> MinFights;
	MinFights.SetNumZeroed(Graph.Nodes.Num());
	for (int32 Layer = 0; Layer < Layers; ++Layer)
	{
		for (int32 Lane = 0; Lane < Lanes; ++Lane)
		{
			const int32 NodeIndex = Index(Layer, Lane);
			FDwMapNode& Node = Graph.Nodes[NodeIndex];
			int32 FightsBefore = MAX_int32;
			bool bShopParent = false, bEventParent = false;
			for (const int32 Parent : Parents[NodeIndex])
			{
				FightsBefore = FMath::Min(FightsBefore, MinFights[Parent]);
				bShopParent |= Graph.Nodes[Parent].Type == EDwNodeType::Shop;
				bEventParent |= Graph.Nodes[Parent].Type == EDwNodeType::Event;
			}
			FightsBefore = Parents[NodeIndex].IsEmpty() ? 0 : FightsBefore;

			// The most fights any path could still pick up after this node: the later layers, minus the forced shop
			// (a path may end through it). A non-fight here is only allowed if even the worst path keeps up.
			const int32 LaterLayers = Layers - 1 - Layer;
			const int32 Capacity = LaterLayers - (ForcedShop != INDEX_NONE && LaterLayers > 0 ? 1 : 0);
			const bool bMustFight = FightsBefore + Capacity < Rules.MinFightsPerPath;

			const float Roll = Random.GetFraction(); // one draw per node, whatever is decided
			if (NodeIndex == ForcedShop)
			{
				Node.Type = EDwNodeType::Shop;
			}
			else if (Layer == 0)
			{
				Node.Type = bMustFight ? EDwNodeType::Fight : Rules.FirstLayerType;
			}
			else
			{
				const bool bFeedsForcedShop = ForcedShop != INDEX_NONE && Node.Next.Contains(ForcedShop);
				const bool bShopOk = !bMustFight && !(Rules.bNoConsecutiveShops && (bShopParent || bFeedsForcedShop));
				const bool bEventOk = !bMustFight && !(Rules.bNoConsecutiveEvents && bEventParent);
				const float WFight = FMath::Max(0.f, Rules.TypeWeights.Fight);
				const float WShop = bShopOk ? FMath::Max(0.f, Rules.TypeWeights.Shop) : 0.f;
				const float WEvent = bEventOk ? FMath::Max(0.f, Rules.TypeWeights.Event) : 0.f;
				const float Total = WFight + WShop + WEvent;
				const float Pick = Roll * Total;
				Node.Type = Total <= 0.f || Pick < WFight ? EDwNodeType::Fight : Pick < WFight + WShop ? EDwNodeType::Shop : EDwNodeType::Event;
			}
			MinFights[NodeIndex] = FightsBefore + (Node.Type == EDwNodeType::Fight ? 1 : 0);
		}
	}

	// Encounters: tiered by layer, not the same crew as the node before where the tier allows.
	for (int32 NodeIndex = 0; NodeIndex < Graph.BossNode; ++NodeIndex)
	{
		FDwMapNode& Node = Graph.Nodes[NodeIndex];
		TArray<const UDwEncounterDefinition*> Avoid;
		for (const int32 Parent : Parents[NodeIndex])
		{
			if (const UDwEncounterDefinition* Previous = Graph.Nodes[Parent].Encounter)
			{
				Avoid.AddUnique(Previous);
			}
		}
		const UDwEncounterDefinition* Encounter = PickEncounter(EncounterPool, Node.Layer, Random, Avoid);
		Node.Encounter = Node.Type == EDwNodeType::Fight ? Encounter : nullptr;
	}
	return Graph;
}

FDwMapGraph FDwMapGenerator::Build(int32 RunSeed, const UDwMapRules& Rules, TConstArrayView<const UDwEncounterDefinition*> EncounterPool,
	const UDwEncounterDefinition* Boss, TArray<FString>* OutProblems, int32* OutAttempts)
{
	FDwMapGraph Graph;
	TArray<FString> Problems;
	const int32 Attempts = FMath::Max(1, Rules.MaxAttempts);
	int32 Attempt = 0;
	for (; Attempt < Attempts; ++Attempt)
	{
		Graph = Generate(FDwRunRandom::ForMap(RunSeed, Attempt), Rules, EncounterPool, Boss);
		Problems = FDwMapValidator::Validate(Graph, Rules);
		if (Problems.IsEmpty())
		{
			break;
		}
	}
	if (OutProblems)
	{
		*OutProblems = MoveTemp(Problems);
	}
	if (OutAttempts)
	{
		*OutAttempts = FMath::Min(Attempt + 1, Attempts);
	}
	return Graph;
}

const UDwEncounterDefinition* FDwMapGenerator::PickEncounter(TConstArrayView<const UDwEncounterDefinition*> Pool, int32 Layer, FRandomStream& Random,
	TConstArrayView<const UDwEncounterDefinition*> Avoid)
{
	auto Pickable = [&](const UDwEncounterDefinition* Encounter, bool bAvoid)
	{
		return DwMapGen::Allowed(Encounter, Layer) && !(bAvoid && Avoid.Contains(Encounter));
	};
	const float Roll = Random.GetFraction();
	for (const bool bAvoid : { true, false }) // fresh crews first; repeats only if the tier has nothing else
	{
		float Total = 0.f;
		for (const UDwEncounterDefinition* Encounter : Pool)
		{
			Total += Pickable(Encounter, bAvoid) ? Encounter->Weight : 0.f;
		}
		if (Total <= 0.f)
		{
			continue;
		}
		float Left = Roll * Total;
		const UDwEncounterDefinition* Last = nullptr;
		for (const UDwEncounterDefinition* Encounter : Pool)
		{
			if (Pickable(Encounter, bAvoid))
			{
				Last = Encounter;
				Left -= Encounter->Weight;
				if (Left < 0.f)
				{
					return Encounter;
				}
			}
		}
		return Last;
	}
	return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
// Validator

TArray<FString> FDwMapValidator::Validate(const FDwMapGraph& Graph, const UDwMapRules& Rules)
{
	using DwMapGen::TypeName;
	TArray<FString> Problems;
	const TArray<FDwMapNode>& Nodes = Graph.Nodes;
	if (!Graph.IsValidNode(Graph.BossNode) || Nodes[Graph.BossNode].Type != EDwNodeType::Boss)
	{
		Problems.Add(TEXT("No boss node."));
		return Problems; // everything below measures paths to the boss
	}
	if (!Nodes[Graph.BossNode].Next.IsEmpty())
	{
		Problems.Add(TEXT("The boss leads somewhere: it must be the end."));
	}

	// Edges: forward one layer (or into the boss from the last layer), 2-3 choices, no Shop -> Shop / Event -> Event.
	const int32 LastLayer = Graph.LayerCount - 1;
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const FDwMapNode& Node = Nodes[Index];
		if (Index == Graph.BossNode)
		{
			continue;
		}
		if (Node.Type == EDwNodeType::Boss)
		{
			Problems.Add(FString::Printf(TEXT("Node %d is a second boss."), Index));
		}
		if (Node.Layer == 0 && Node.Type != Rules.FirstLayerType)
		{
			Problems.Add(FString::Printf(TEXT("Node %d on the first layer is a %s, not a %s (M3)."), Index, *TypeName(Node.Type), *TypeName(Rules.FirstLayerType)));
		}
		if (Node.Type == EDwNodeType::Fight && !Node.Encounter)
		{
			Problems.Add(FString::Printf(TEXT("Fight node %d (layer %d) has no encounter."), Index, Node.Layer));
		}
		if (Node.Layer == LastLayer)
		{
			if (Node.Next.Num() != 1 || Node.Next[0] != Graph.BossNode)
			{
				Problems.Add(FString::Printf(TEXT("Node %d on the last layer must lead only to the boss."), Index));
			}
		}
		else if (Rules.LaneCount > 1 && (Node.Next.Num() < 2 || Node.Next.Num() > 3))
		{
			Problems.Add(FString::Printf(TEXT("Node %d offers %d choices (2-3 required)."), Index, Node.Next.Num()));
		}
		for (const int32 To : Node.Next)
		{
			if (!Nodes.IsValidIndex(To))
			{
				Problems.Add(FString::Printf(TEXT("Node %d links to missing node %d."), Index, To));
				continue;
			}
			const FDwMapNode& Child = Nodes[To];
			if (To != Graph.BossNode && Child.Layer != Node.Layer + 1)
			{
				Problems.Add(FString::Printf(TEXT("Node %d (layer %d) links to node %d on layer %d: forward one layer only."), Index, Node.Layer, To, Child.Layer));
			}
			if (Rules.bNoConsecutiveShops && Node.Type == EDwNodeType::Shop && Child.Type == EDwNodeType::Shop)
			{
				Problems.Add(FString::Printf(TEXT("Shop %d leads to shop %d."), Index, To));
			}
			if (Rules.bNoConsecutiveEvents && Node.Type == EDwNodeType::Event && Child.Type == EDwNodeType::Event)
			{
				Problems.Add(FString::Printf(TEXT("Event %d leads to event %d."), Index, To));
			}
		}
		// M5: the last step before the boss always offers a shop.
		if (Rules.bShopOptionBeforeBoss && Graph.LayerCount > 1 && Node.Layer == LastLayer - 1
			&& !Node.Next.ContainsByPredicate([&Nodes](int32 To) { return Nodes.IsValidIndex(To) && Nodes[To].Type == EDwNodeType::Shop; }))
		{
			Problems.Add(FString::Printf(TEXT("Node %d (before the last layer) offers no shop before the boss (M5)."), Index));
		}
	}

	// Paths (DP from the boss backwards, layers only go forward): shortest/longest length and fewest fights to the boss.
	struct FReach { int32 MinLen = MAX_int32; int32 MaxLen = -1; int32 MinFights = MAX_int32; };
	TArray<FReach> Reach;
	Reach.SetNum(Nodes.Num());
	Reach[Graph.BossNode] = { 0, 0, 0 };
	TArray<int32> Order;
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		if (Index != Graph.BossNode)
		{
			Order.Add(Index);
		}
	}
	Order.Sort([&Nodes](int32 A, int32 B) { return Nodes[A].Layer > Nodes[B].Layer; });
	for (const int32 Index : Order)
	{
		FReach& Here = Reach[Index];
		for (const int32 To : Nodes[Index].Next)
		{
			if (!Nodes.IsValidIndex(To) || Reach[To].MaxLen < 0 || (To != Graph.BossNode && Nodes[To].Layer <= Nodes[Index].Layer))
			{
				continue;
			}
			const int32 Fight = Nodes[Index].Type == EDwNodeType::Fight ? 1 : 0;
			Here.MinLen = FMath::Min(Here.MinLen, Reach[To].MinLen + 1);
			Here.MaxLen = FMath::Max(Here.MaxLen, Reach[To].MaxLen + 1);
			Here.MinFights = FMath::Min(Here.MinFights, Reach[To].MinFights + Fight);
		}
		if (Here.MaxLen < 0)
		{
			Problems.Add(FString::Printf(TEXT("Node %d never reaches the boss."), Index));
		}
	}
	const TArray<int32> Starts = Graph.GetChoices(INDEX_NONE);
	if (Starts.IsEmpty())
	{
		Problems.Add(TEXT("No first-layer nodes to start from."));
	}
	for (const int32 Start : Starts)
	{
		const FReach& Path = Reach[Start];
		if (Path.MaxLen < 0)
		{
			continue; // reported above
		}
		if (Path.MinLen < Rules.MinPathNodes || Path.MaxLen > Rules.MaxPathNodes)
		{
			Problems.Add(FString::Printf(TEXT("Paths from node %d are %d-%d nodes long (%d-%d required)."), Start, Path.MinLen, Path.MaxLen,
				Rules.MinPathNodes, Rules.MaxPathNodes));
		}
		if (Path.MinFights < Rules.MinFightsPerPath)
		{
			Problems.Add(FString::Printf(TEXT("A path from node %d has only %d fights (%d required, M2)."), Start, Path.MinFights, Rules.MinFightsPerPath));
		}
	}
	return Problems;
}
