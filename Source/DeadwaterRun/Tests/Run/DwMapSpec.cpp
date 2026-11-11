// Copyright Brandon Hillig 2026

// DW-202: seeded sea chart and its validator (Docs/Architecture/09-Run-Map.md "How it's tested"). Pure.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/DwMapRules.h"
#include "Run/DwRunRandom.h"
#include "Run/Map/DwMapGenerator.h"
#include "Tests/Run/DwRunTestContent.h"

BEGIN_DEFINE_SPEC(FDwMapSpec, "DeadwaterRun.Rules.Map",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	DwRunTest::FContent C;
	UDwMapRules* Rules = nullptr;

	FDwMapGraph Make(int32 Seed) const
	{
		return FDwMapGenerator::Generate(Seed, *Rules, C.Pool(), C.Kraken);
	}

	/** True if some problem line contains Fragment (and logs them all on failure). */
	bool Reports(const TArray<FString>& Problems, const TCHAR* Fragment)
	{
		const bool bFound = Problems.ContainsByPredicate([Fragment](const FString& Line) { return Line.Contains(Fragment); });
		return TestTrue(*FString::Printf(TEXT("Reports '%s' (got: %s)"), Fragment, *FString::Join(Problems, TEXT(" | "))), bFound);
	}

	int32 NodeAt(const FDwMapGraph& Graph, int32 Layer, int32 Lane) const
	{
		return Graph.Nodes.IndexOfByPredicate([Layer, Lane](const FDwMapNode& N) { return N.Layer == Layer && N.Lane == Lane && N.Type != EDwNodeType::Boss; });
	}
END_DEFINE_SPEC(FDwMapSpec)

void FDwMapSpec::Define()
{
	BeforeEach([this]()
	{
		C.Create();
		Rules = C.Make<UDwMapRules>(TEXT("DA_MapRules_Test"));
		Rules->TypeWeights.Fight = 2.f; // DA_MapRules placeholder weights
	});
	AfterEach([this]() { C.Destroy(); Rules = nullptr; });

	Describe("Generator", [this]()
	{
		It("passes the validator for 10,000 seeds on the first try, with 8- and 9-node charts both appearing", [this]()
		{
			int32 Failures = 0, Eight = 0, Nine = 0, Shops = 0, Events = 0;
			for (int32 Seed = 0; Seed < 10000; ++Seed)
			{
				const FDwMapGraph Graph = Make(FDwRunRandom::ForMap(Seed, 0));
				const TArray<FString> Problems = FDwMapValidator::Validate(Graph, *Rules);
				if (!Problems.IsEmpty() && Failures++ < 3)
				{
					AddError(FString::Printf(TEXT("Seed %d: %s"), Seed, *FString::Join(Problems, TEXT(" | "))));
				}
				Eight += Graph.LayerCount == 8 ? 1 : 0;
				Nine += Graph.LayerCount == 9 ? 1 : 0;
				for (const FDwMapNode& Node : Graph.Nodes)
				{
					Shops += Node.Type == EDwNodeType::Shop ? 1 : 0;
					Events += Node.Type == EDwNodeType::Event ? 1 : 0;
				}
			}
			TestEqual(TEXT("No invalid chart"), Failures, 0);
			TestTrue(*FString::Printf(TEXT("Both lengths (8: %d, 9: %d)"), Eight, Nine), Eight > 4000 && Nine > 4000);
			TestTrue(TEXT("Shops beyond the forced one"), Shops > 10000);
			TestTrue(TEXT("Events"), Events > 10000);
		});

		It("keeps the rules even when the weights barely want fights (forcing, not luck)", [this]()
		{
			Rules->TypeWeights.Fight = 0.01f;
			int32 Failures = 0;
			for (int32 Seed = 0; Seed < 2000; ++Seed)
			{
				const TArray<FString> Problems = FDwMapValidator::Validate(Make(Seed), *Rules);
				if (!Problems.IsEmpty() && Failures++ < 3)
				{
					AddError(FString::Printf(TEXT("Seed %d: %s"), Seed, *FString::Join(Problems, TEXT(" | "))));
				}
			}
			TestEqual(TEXT("No invalid chart"), Failures, 0);
		});

		It("is identical for identical seeds and differs between seeds", [this]()
		{
			int32 Different = 0;
			for (int32 Seed = 0; Seed < 200; ++Seed)
			{
				const FDwMapGraph A = Make(Seed), B = Make(Seed), Other = Make(Seed + 1000);
				if (!TestEqual(TEXT("Same size"), A.Nodes.Num(), B.Nodes.Num())) { return; }
				bool bSame = true, bOther = A.Nodes.Num() == Other.Nodes.Num();
				for (int32 Index = 0; Index < A.Nodes.Num(); ++Index)
				{
					const FDwMapNode& X = A.Nodes[Index];
					const FDwMapNode& Y = B.Nodes[Index];
					bSame &= X.Type == Y.Type && X.Encounter == Y.Encounter && X.Next == Y.Next && X.Layer == Y.Layer && X.Lane == Y.Lane;
					bOther &= bOther && Other.Nodes.IsValidIndex(Index) && Other.Nodes[Index].Type == X.Type;
				}
				TestTrue(TEXT("Identical"), bSame && A.BossNode == B.BossNode && A.LayerCount == B.LayerCount);
				Different += bOther ? 0 : 1;
			}
			TestTrue(*FString::Printf(TEXT("Seeds give different charts (%d of 200)"), Different), Different > 150);
		});

		It("builds the lane shape: first layer fights, a shop before the boss, the boss after the last layer", [this]()
		{
			const FDwMapGraph Graph = Make(7);
			const int32 Lanes = Rules->LaneCount;
			TestEqual(TEXT("Nodes"), Graph.Nodes.Num(), Graph.LayerCount * Lanes + 1);
			TestEqual(TEXT("Boss last"), Graph.BossNode, Graph.Nodes.Num() - 1);
			TestTrue(TEXT("Boss encounter"), Graph.Nodes[Graph.BossNode].Encounter == C.Kraken);
			for (int32 Lane = 0; Lane < Lanes; ++Lane)
			{
				TestEqual(TEXT("First layer fights (M3)"), Graph.Nodes[NodeAt(Graph, 0, Lane)].Type, EDwNodeType::Fight);
			}
			TestEqual(TEXT("Shop in the middle of the last layer (M5)"), Graph.Nodes[NodeAt(Graph, Graph.LayerCount - 1, Lanes / 2)].Type, EDwNodeType::Shop);
			TestTrue(TEXT("Only fights carry encounters"), !Graph.Nodes.ContainsByPredicate([](const FDwMapNode& N)
				{ return (N.Type == EDwNodeType::Shop || N.Type == EDwNodeType::Event) && N.Encounter; }));
		});

		It("tiers encounters by layer and avoids the previous node's crew when the tier allows", [this]()
		{
			for (int32 Seed = 0; Seed < 300; ++Seed)
			{
				const FDwMapGraph Graph = Make(Seed);
				for (int32 Index = 0; Index < Graph.Nodes.Num(); ++Index)
				{
					const FDwMapNode& Node = Graph.Nodes[Index];
					if (Node.Type != EDwNodeType::Fight)
					{
						continue;
					}
					if (Node.Layer < 4 && !TestTrue(TEXT("No late crew before layer 4"), Node.Encounter == C.Early)) { return; }
					for (const int32 To : Node.Next)
					{
						const FDwMapNode& Child = Graph.Nodes[To];
						if (Child.Type != EDwNodeType::Fight || Child.Layer < 4 || Child.Encounter != Node.Encounter)
						{
							continue;
						}
						// From layer 4 both crews are allowed: a repeat is only fine if the child's parents already used both.
						TSet<const UDwEncounterDefinition*> ParentCrews;
						for (const FDwMapNode& Parent : Graph.Nodes)
						{
							if (Parent.Next.Contains(To) && Parent.Encounter)
							{
								ParentCrews.Add(Parent.Encounter);
							}
						}
						if (ParentCrews.Num() < 2 && !TestTrue(FString::Printf(TEXT("Seed %d: node %d repeats node %d's crew"), Seed, To, Index), false)) { return; }
					}
				}
			}
		});

		It("PickEncounter returns nothing for an empty tier and a repeat only when nothing else fits", [this]()
		{
			FRandomStream Random(1);
			const TArray<const UDwEncounterDefinition*> OnlyLate = { C.Late };
			TestNull(TEXT("Layer 2 has none"), FDwMapGenerator::PickEncounter(OnlyLate, 2, Random));
			TestTrue(TEXT("Layer 5 has it"), FDwMapGenerator::PickEncounter(OnlyLate, 5, Random) == C.Late);
			const TArray<const UDwEncounterDefinition*> Avoid = { C.Late };
			TestTrue(TEXT("Repeats when it's the only one"), FDwMapGenerator::PickEncounter(OnlyLate, 5, Random, Avoid) == C.Late);
			const TArray<const UDwEncounterDefinition*> Both = { C.Early, C.Late };
			for (int32 Each = 0; Each < 50; ++Each)
			{
				TestTrue(TEXT("Avoided"), FDwMapGenerator::PickEncounter(Both, 5, Random, Avoid) == C.Early);
			}
		});

		It("Build retries with derived seeds and reports problems it can't fix", [this]()
		{
			TArray<FString> Problems;
			int32 Attempts = 0;
			FDwMapGenerator::Build(7, *Rules, C.Pool(), C.Kraken, &Problems, &Attempts);
			TestTrue(TEXT("Valid at once"), Problems.IsEmpty() && Attempts == 1);
			Rules->MinFightsPerPath = 20; // impossible
			Rules->MaxAttempts = 4;
			FDwMapGenerator::Build(7, *Rules, C.Pool(), C.Kraken, &Problems, &Attempts);
			TestEqual(TEXT("Gave up after MaxAttempts"), Attempts, 4);
			Reports(Problems, TEXT("fights (20 required"));
		});
	});

	Describe("Validator", [this]()
	{
		It("accepts a generated chart", [this]()
		{
			const TArray<FString> Problems = FDwMapValidator::Validate(Make(7), *Rules);
			TestTrue(*FString::Join(Problems, TEXT(" | ")), Problems.IsEmpty());
		});

		It("catches Shop -> Shop and Event -> Event", [this]()
		{
			FDwMapGraph Graph = Make(7);
			const int32 A = NodeAt(Graph, 2, 0), B = NodeAt(Graph, 3, 0), E1 = NodeAt(Graph, 4, 2), E2 = NodeAt(Graph, 5, 2);
			Graph.Nodes[A].Type = Graph.Nodes[B].Type = EDwNodeType::Shop;
			Graph.Nodes[E1].Type = Graph.Nodes[E2].Type = EDwNodeType::Event;
			const TArray<FString> Problems = FDwMapValidator::Validate(Graph, *Rules);
			Reports(Problems, *FString::Printf(TEXT("Shop %d leads to shop %d"), A, B));
			Reports(Problems, *FString::Printf(TEXT("Event %d leads to event %d"), E1, E2));
			Rules->bNoConsecutiveShops = Rules->bNoConsecutiveEvents = false;
			TestFalse(TEXT("Allowed when the rules say so"), FDwMapValidator::Validate(Graph, *Rules).ContainsByPredicate(
				[](const FString& Line) { return Line.Contains(TEXT("leads to shop")) || Line.Contains(TEXT("leads to event")); }));
		});

		It("catches too few fights on a path, and path lengths out of range", [this]()
		{
			const FDwMapGraph Graph = Make(7);
			Rules->MinFightsPerPath = Graph.LayerCount; // the forced shop alone breaks it
			Rules->MinPathNodes = Rules->MaxPathNodes = Graph.LayerCount + 1;
			const TArray<FString> Problems = FDwMapValidator::Validate(Graph, *Rules);
			Reports(Problems, TEXT("required, M2"));
			Reports(Problems, TEXT("nodes long"));
		});

		It("catches bad edges: too few choices, skipped layers, dead ends, missing nodes", [this]()
		{
			FDwMapGraph Graph = Make(7);
			const int32 One = NodeAt(Graph, 1, 0), Skip = NodeAt(Graph, 2, 1), Dead = NodeAt(Graph, 3, 2), Missing = NodeAt(Graph, 4, 1);
			Graph.Nodes[One].Next.SetNum(1);
			Graph.Nodes[Skip].Next[0] = NodeAt(Graph, 4, 0);
			Graph.Nodes[Dead].Next.Reset();
			Graph.Nodes[Missing].Next.Add(999);
			const TArray<FString> Problems = FDwMapValidator::Validate(Graph, *Rules);
			Reports(Problems, *FString::Printf(TEXT("Node %d offers 1 choices"), One));
			Reports(Problems, *FString::Printf(TEXT("Node %d (layer 2) links to node"), Skip));
			Reports(Problems, *FString::Printf(TEXT("Node %d never reaches the boss"), Dead));
			Reports(Problems, *FString::Printf(TEXT("Node %d links to missing node 999"), Missing));
		});

		It("catches a missing shop before the boss (M5), a non-fight first layer (M3) and a fight without a crew", [this]()
		{
			FDwMapGraph Graph = Make(7);
			const int32 Shop = NodeAt(Graph, Graph.LayerCount - 1, 1);
			Graph.Nodes[Shop].Type = EDwNodeType::Fight;
			Graph.Nodes[Shop].Encounter = nullptr;
			Graph.Nodes[NodeAt(Graph, 0, 2)].Type = EDwNodeType::Event;
			const TArray<FString> Problems = FDwMapValidator::Validate(Graph, *Rules);
			Reports(Problems, TEXT("offers no shop before the boss"));
			Reports(Problems, TEXT("on the first layer is a Event"));
			Reports(Problems, *FString::Printf(TEXT("Fight node %d"), Shop));
			Rules->bShopOptionBeforeBoss = false;
			TestFalse(TEXT("No shop needed when the rule is off"), FDwMapValidator::Validate(Graph, *Rules).ContainsByPredicate(
				[](const FString& Line) { return Line.Contains(TEXT("no shop before the boss")); }));
		});

		It("catches the last layer not leading to the boss, a boss that leads on, and no boss at all", [this]()
		{
			FDwMapGraph Graph = Make(7);
			const int32 Last = NodeAt(Graph, Graph.LayerCount - 1, 0);
			Graph.Nodes[Last].Next.Add(NodeAt(Graph, Graph.LayerCount - 1, 1));
			Graph.Nodes[Graph.BossNode].Next.Add(0);
			const TArray<FString> Problems = FDwMapValidator::Validate(Graph, *Rules);
			Reports(Problems, *FString::Printf(TEXT("Node %d on the last layer must lead only to the boss"), Last));
			Reports(Problems, TEXT("The boss leads somewhere"));
			Graph.BossNode = INDEX_NONE;
			Reports(FDwMapValidator::Validate(Graph, *Rules), TEXT("No boss node"));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
