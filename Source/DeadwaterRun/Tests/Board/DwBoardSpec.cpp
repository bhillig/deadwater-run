// Copyright Brandon Hillig 2026

// DW-102: ADwBoard in a transient world, using the shipped placeholder layouts.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Board/DwBoard.h"
#include "Board/DwBoardBuilder.h"
#include "Board/DwDeckLayout.h"
#include "Board/DwGridQueries.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

BEGIN_DEFINE_SPEC(FDwBoardSpec, "DeadwaterRun.Board",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UWorld* World = nullptr;
	ADwBoard* Board = nullptr;
	FDwGridModel Grid;
END_DEFINE_SPEC(FDwBoardSpec)

void FDwBoardSpec::Define()
{
	BeforeEach([this]()
	{
		World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);

		const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
		const UDwDeckLayout* PlayerDeck = GameData ? GameData->DefaultPlayerDeck.Get() : nullptr;
		const UDwDeckLayout* EnemyDeck = LoadObject<UDwDeckLayout>(nullptr, TEXT("/Game/DeadwaterRun/Data/Boards/DA_Deck_EnemyPlaceholder.DA_Deck_EnemyPlaceholder"));
		if (TestNotNull(TEXT("DA_GameData.DefaultPlayerDeck"), PlayerDeck) && TestNotNull(TEXT("Enemy placeholder deck"), EnemyDeck))
		{
			Grid = FDwBoardBuilder::BuildShipBattle(*PlayerDeck, *EnemyDeck);
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Board = World->SpawnActor<ADwBoard>(FVector(1000.f, -500.f, 50.f), FRotator(0.f, 30.f, 0.f), Params);
		Board->SetGrid(Grid, 120.f);
	});

	AfterEach([this]()
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		World = nullptr;
		Board = nullptr;
	});

	It("ships placeholder layouts that validate and join on 3 boarding rows", [this]()
	{
		TestEqual(TEXT("Board is 12x4"), Grid.Size, FIntPoint(12, 4));
		int32 Crossings = 0;
		TArray<FDwGridStep> Steps;
		for (int32 Y = 0; Y < Grid.Size.Y; ++Y)
		{
			Grid.GetNeighbors(FIntPoint(5, Y), FDwGridRules(), Steps);
			Crossings += Steps.ContainsByPredicate([Y](const FDwGridStep& S) { return S.Tile == FIntPoint(6, Y); }) ? 1 : 0;
		}
		TestEqual(TEXT("Rows with a boarding edge on both ships"), Crossings, 3);
	});

	It("maps every tile to world and back, through the actor transform", [this]()
	{
		for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
		{
			const FIntPoint Tile = Grid.ToTile(Index);
			if (!Grid.IsStandable(Tile) || Grid.Tiles[Index].Elevation > 0)
			{
				continue;
			}
			FIntPoint Back(-1, -1);
			TestTrue(TEXT("Inside the board"), Board->WorldToTile(Board->TileToWorld(Tile), Back));
			TestEqual(TEXT("Round trip"), Back, Tile);
		}
		FIntPoint Ignored;
		TestFalse(TEXT("Points off the board map to nothing"), Board->WorldToTile(Board->TileToWorld(FIntPoint(-3, 0)), Ignored));
	});

	It("picks the nest before the deck under a cursor ray", [this]()
	{
		const FIntPoint Nest(4, 0); // 'N' in DA_Deck_PlayerShip row 0
		TestEqual(TEXT("Layout nest"), Grid.GetTile(Nest)->Kind, EDwTileKind::Nest);

		const FVector Above = Board->TileToWorld(Nest) + FVector(0.f, 0.f, 1000.f);
		FIntPoint Hit(-1, -1);
		TestTrue(TEXT("Ray hits the board"), Board->RayToTile(Above, FVector::DownVector, Hit));
		TestEqual(TEXT("Nest picked"), Hit, Nest);

		const FIntPoint Deck(0, 2);
		TestTrue(TEXT("Ray hits a deck tile"), Board->RayToTile(Board->TileToWorld(Deck) + FVector(0.f, 0.f, 1000.f), FVector::DownVector, Hit));
		TestEqual(TEXT("Deck picked"), Hit, Deck);
	});

	It("draws one highlight per tile and clears by layer", [this]()
	{
		const TMap<FIntPoint, int32> Reach = FDwPathfinder::ComputeReachable(Grid, FDwGridRules(), FIntPoint(0, 0), 5, EDwSide::Player);
		TArray<FIntPoint> Tiles;
		Reach.GetKeys(Tiles);
		Board->ShowHighlights(EDwHighlightLayer::Move, Tiles);
		Board->ShowHighlights(EDwHighlightLayer::Target, { FIntPoint(6, 0) });
		TestEqual(TEXT("Move layer"), Board->GetHighlightCount(EDwHighlightLayer::Move), Tiles.Num());
		TestEqual(TEXT("Target layer"), Board->GetHighlightCount(EDwHighlightLayer::Target), 1);

		Board->ShowHighlights(EDwHighlightLayer::Move, { FIntPoint(1, 1) });
		TestEqual(TEXT("Show replaces the layer"), Board->GetHighlightCount(EDwHighlightLayer::Move), 1);

		Board->ClearHighlights(EDwHighlightLayer::Move);
		TestEqual(TEXT("Move cleared"), Board->GetHighlightCount(EDwHighlightLayer::Move), 0);
		TestEqual(TEXT("Other layers untouched"), Board->GetHighlightCount(EDwHighlightLayer::Target), 1);
		Board->ClearAllHighlights();
		TestEqual(TEXT("All cleared"), Board->GetHighlightCount(EDwHighlightLayer::Target), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
