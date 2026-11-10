// Copyright Brandon Hillig 2026

// DW-307: the Synty art pass. The deck seam, sea moods, and the generated ship / pirate content (Tools/CreateCombatContent.ps1)
// spawned through the same setup builder the game uses.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimationAsset.h"
#include "Board/DwBoard.h"
#include "Board/DwBoardBuilder.h"
#include "Board/DwDeckLayout.h"
#include "Combat/DwEncounterSetupBuilder.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Data/DwEncounterDefinition.h"
#include "Presentation/DwPresentationSubsystem.h"
#include "Presentation/DwSeaEnvironment.h"
#include "Tests/Combat/DwCombatTestFixture.h"

BEGIN_DEFINE_SPEC(FDwArtSpec, "DeadwaterRun.Art",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	FDwCombatFixture F;
	const UDwGameData* GameData = nullptr;
	TArray<UObject*> Rooted;

	UDwSeaMood* MakeMood(const TCHAR* Name)
	{
		UDwSeaMood* Mood = NewObject<UDwSeaMood>(GetTransientPackage(), Name);
		Mood->DisplayName = FText::FromString(FString(Name).RightChop(FString(TEXT("DA_SeaMood_")).Len()));
		Mood->AddToRoot();
		Rooted.Add(Mood);
		return Mood;
	}

	/** The dev fight's content through the real builder. bWatched = presentation on, as when a player looks at it. */
	bool StartContentFight(bool bWatched)
	{
		const UDwEncounterDefinition* Encounter = GameData ? GameData->DevEncounter.Get() : nullptr;
		if (!TestNotNull(TEXT("DA_GameData.DevEncounter"), Encounter))
		{
			return false;
		}
		TArray<FDwCombatantSetup> Crew;
		for (const UDwUnitDefinition* Unit : GameData->DevCrew)
		{
			FDwCombatantSetup& Member = Crew.AddDefaulted_GetRef();
			Member.Definition = Unit;
			Member.RosterId = FGuid::NewGuid();
			Member.bIsCaptain = Crew.Num() == 1;
		}
		FDwEncounterSetup Setup;
		TArray<FString> Errors;
		if (!TestTrue(FString::Printf(TEXT("Setup built: %s"), *FString::Join(Errors, TEXT("; "))),
			FDwEncounterSetupBuilder::BuildShipBattle(*GameData, *Encounter, Crew, 7, Setup, Errors)))
		{
			return false;
		}
		F.World->GetSubsystem<UDwPresentationSubsystem>()->SetInstant(!bWatched);
		return TestTrue(TEXT("Started"), F.Combat->StartEncounter(Setup));
	}

END_DEFINE_SPEC(FDwArtSpec)

void FDwArtSpec::Define()
{
	BeforeEach([this]()
	{
		F.Create();
		GameData = UDwAssetManager::Get().GetGameData();
	});
	AfterEach([this]()
	{
		F.Destroy();
		for (UObject* Object : Rooted)
		{
			Object->RemoveFromRoot();
		}
		Rooted.Reset();
	});

	Describe("Deck seam", [this]()
	{
		It("draws the enemy deck a gap further along X and still maps every tile to world and back", [this]()
		{
			const FDwGridModel Grid = FDwBoardBuilder::BuildShipBattle(FDwDeckAscii::Parse(FDwCombatFixture::DefaultDeck()), FDwDeckAscii::Parse(FDwCombatFixture::DefaultDeck()));
			ADwBoard* Board = F.World->SpawnActor<ADwBoard>(FVector(300.f, -200.f, 40.f), FRotator(0.f, 25.f, 0.f));
			Board->SetGrid(Grid, 100.f);
			const FVector PlayerEdge = Board->TileToWorld(FIntPoint(4, 1));
			const FVector EnemyEdge = Board->TileToWorld(FIntPoint(5, 1));
			TestEqual(TEXT("No seam: neighbours are one tile apart"), FVector::Dist(PlayerEdge, EnemyEdge), 100.0, 0.01);

			Board->SetGrid(Grid, 100.f, /*SeamColumn*/ 5, /*SeamGap*/ 150.f);
			TestTrue(TEXT("The player's deck doesn't move"), Board->TileToWorld(FIntPoint(4, 1)).Equals(PlayerEdge, 0.01));
			TestEqual(TEXT("Across the seam: one tile plus the gap"), FVector::Dist(PlayerEdge, Board->TileToWorld(FIntPoint(5, 1))), 250.0, 0.01);
			TestEqual(TEXT("Board is wider by the gap"), Board->GetLocalBounds().GetSize().X, 10 * 100.0 + 150.0, 0.01);

			for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
			{
				const FIntPoint Tile = Grid.ToTile(Index);
				FIntPoint Back(-1, -1);
				TestTrue(TEXT("World to tile"), Board->WorldToTile(Board->TileToWorld(Tile), Back));
				TestEqual(TEXT("Round trip"), Back, Tile);
				TestTrue(TEXT("Cursor ray"), Board->RayToTile(Board->TileToWorld(Tile) + FVector(0.f, 0.f, 500.f), FVector::DownVector, Back));
				TestEqual(TEXT("Ray round trip"), Back, Tile);
			}

			const FVector InSeam = (Board->TileToWorld(FIntPoint(4, 1)) + Board->TileToWorld(FIntPoint(5, 1))) * 0.5f;
			FIntPoint Ignored;
			TestFalse(TEXT("The water between the ships is no tile"), Board->WorldToTile(InSeam, Ignored));
			TestFalse(TEXT("...for the cursor either"), Board->RayToTile(InSeam + FVector(0.f, 0.f, 500.f), FVector::DownVector, Ignored));
			Board->Destroy();
		});

		It("ignores a seam on a board that has no second deck", [this]()
		{
			const FDwGridModel Grid = FDwBoardBuilder::BuildSingleDeck(FDwDeckAscii::Parse(FDwCombatFixture::DefaultDeck()), EDwSide::Player);
			ADwBoard* Board = F.World->SpawnActor<ADwBoard>();
			Board->SetGrid(Grid, 100.f, /*SeamColumn*/ 9, /*SeamGap*/ 150.f);
			TestEqual(TEXT("Width"), Board->GetLocalBounds().GetSize().X, 5 * 100.0, 0.01);
			Board->Destroy();
		});
	});

	Describe("Sea mood", [this]()
	{
		It("rolls by weight from the seed alone, and never rolls a zero-weight mood", [this]()
		{
			const TArray<FDwSeaMoodChance> Moods = { { MakeMood(TEXT("DA_SeaMood_Day")), 4.f }, { MakeMood(TEXT("DA_SeaMood_Storm")), 1.f }, { MakeMood(TEXT("DA_SeaMood_Fog")), 0.f } };
			int32 Days = 0, Storms = 0, Fogs = 0;
			for (int32 Seed = 0; Seed < 2000; ++Seed)
			{
				const UDwSeaMood* Mood = ADwSeaEnvironment::PickMood(Moods, Seed);
				TestTrue(TEXT("Same seed, same sea"), Mood == ADwSeaEnvironment::PickMood(Moods, Seed));
				Days += Mood == Moods[0].Mood ? 1 : 0;
				Storms += Mood == Moods[1].Mood ? 1 : 0;
				Fogs += Mood == Moods[2].Mood ? 1 : 0;
			}
			TestEqual(TEXT("Zero weight never rolls"), Fogs, 0);
			TestEqual(TEXT("Every roll is a mood"), Days + Storms, 2000);
			TestTrue(FString::Printf(TEXT("About four days per storm (%d : %d)"), Days, Storms), Days > 1500 && Days < 1700);
		});

		It("takes a mood by name, whatever its weight", [this]()
		{
			const TArray<FDwSeaMoodChance> Moods = { { MakeMood(TEXT("DA_SeaMood_Day")), 1.f }, { MakeMood(TEXT("DA_SeaMood_Storm")), 0.f } };
			TestTrue(TEXT("By short name"), ADwSeaEnvironment::PickMood(Moods, 1, TEXT("storm")) == Moods[1].Mood);
			TestTrue(TEXT("By asset name"), ADwSeaEnvironment::PickMood(Moods, 1, TEXT("DA_SeaMood_Storm")) == Moods[1].Mood);
			TestTrue(TEXT("Unknown name falls back to the roll"), ADwSeaEnvironment::PickMood(Moods, 1, TEXT("Blizzard")) == Moods[0].Mood);
			TestNull(TEXT("No moods, no mood"), ADwSeaEnvironment::PickMood({}, 1));
		});

		It("ships a day and a storm, with day the likelier", [this]()
		{
			if (!TestNotNull(TEXT("DA_GameData"), GameData) || !TestEqual(TEXT("Two moods"), GameData->SeaMoods.Num(), 2))
			{
				return;
			}
			TestTrue(TEXT("Day outweighs the storm"), GameData->SeaMoods[0].Weight > GameData->SeaMoods[1].Weight && GameData->SeaMoods[1].Weight > 0.f);
			for (const FDwSeaMoodChance& Entry : GameData->SeaMoods)
			{
				if (TestNotNull(TEXT("Mood"), Entry.Mood.Get()))
				{
					TestNotNull(*FString::Printf(TEXT("%s water"), *Entry.Mood->GetName()), Entry.Mood->WaterMaterial.Get());
				}
			}
			ADwSeaEnvironment* Sea = F.World->SpawnActor<ADwSeaEnvironment>();
			Sea->ApplyMood(GameData->SeaMoods[1].Mood);
			TestTrue(TEXT("Applied"), Sea->GetMood() == GameData->SeaMoods[1].Mood);
			Sea->Destroy();
		});
	});

	Describe("Ships", [this]()
	{
		It("stand at their deck centres, the enemy mirrored like its layout, with a gap for the hulls", [this]()
		{
			if (!StartContentFight(/*bWatched*/ false)) { return; }
			const ADwBoard* Board = F.Combat->GetBoard();
			const ADwShip* Player = F.Combat->GetShip(EDwSide::Player);
			const ADwShip* Enemy = F.Combat->GetShip(EDwSide::Enemy);
			TestTrue(TEXT("BP_Board"), Board->GetClass() == GameData->BoardClass.Get());
			TestTrue(TEXT("Deck gap in the data"), GameData->DeckGap > 0.f);

			// DA_Deck_PlayerShip and the enemy deck are both 6 x 4.
			const FVector PlayerCentre = (Board->TileToWorld(FIntPoint(0, 0)) + Board->TileToWorld(FIntPoint(5, 3))) * 0.5f;
			const FVector EnemyCentre = (Board->TileToWorld(FIntPoint(6, 0)) + Board->TileToWorld(FIntPoint(11, 3))) * 0.5f;
			TestTrue(TEXT("Player ship at its deck centre"), FVector::DistXY(Player->GetActorLocation(), PlayerCentre) < 0.1);
			TestTrue(TEXT("Enemy ship at its deck centre"), FVector::DistXY(Enemy->GetActorLocation(), EnemyCentre) < 0.1);
			TestEqual(TEXT("Decks are a gap apart"), EnemyCentre.X - PlayerCentre.X, 6.0 * Board->GetTileSize() + GameData->DeckGap, 0.1);
			TestEqual(TEXT("Player ship is not mirrored"), Player->GetActorScale3D().X, 1.0, 0.001);
			TestEqual(TEXT("Enemy ship is mirrored"), Enemy->GetActorScale3D().X, -1.0, 0.001);
		});

		It("carry a hull, a mast and props for every deck feature", [this]()
		{
			if (!StartContentFight(/*bWatched*/ false)) { return; }
			for (EDwSide Side : { EDwSide::Player, EDwSide::Enemy })
			{
				const ADwShip* Ship = F.Combat->GetShip(Side);
				TestTrue(TEXT("Hull pieces"), Ship->HullPieces.Num() > 5);
				TestTrue(TEXT("Mast"), Ship->Dressing.Mast.Num() > 0);
				TestTrue(TEXT("Cannon"), Ship->Dressing.Cannon.Num() > 0);
				TestTrue(TEXT("Crates"), Ship->Dressing.LowObstacle.Num() > 0);
				TestTrue(TEXT("Ladder"), Ship->Dressing.Ladder.Num() > 0);
				TestTrue(TEXT("Nest"), Ship->Dressing.Nest.Num() > 0);
				for (const FDwMeshPiece& Piece : Ship->HullPieces)
				{
					TestNotNull(TEXT("Hull mesh"), Piece.Mesh.Get());
					TestFalse(TEXT("Masts are dressing, rigging is left out"), Piece.Mesh && (Piece.Mesh->GetName().Contains(TEXT("_Mast_")) || Piece.Mesh->GetName().Contains(TEXT("_Rigging_"))));
				}
				TestEqual(TEXT("Nothing is built for a fight nobody watches"), Ship->GetDressingCount(), 0);
			}
			const UDwDeckLayout* Deck = GameData->DefaultPlayerDeck;
			TestTrue(TEXT("The layout places its hull"), Deck && !Deck->MeshAlignment.GetLocation().IsNearlyZero() && Deck->MeshAlignment.GetScale3D().X >= 1.f);
		});

		It("dress exactly their own feature tiles when the fight is watched", [this]()
		{
			if (!StartContentFight(/*bWatched*/ true)) { return; }
			const FDwGridModel& Grid = F.Combat->GetGrid();
			for (EDwSide Side : { EDwSide::Player, EDwSide::Enemy })
			{
				const ADwShip* Ship = F.Combat->GetShip(Side);
				int32 Expected = 0;
				for (const FDwTile& Tile : Grid.Tiles)
				{
					if (Tile.Owner != Side) { continue; }
					Expected += Tile.Tags.HasTagExact(DwTags::Tile_Cannon) ? Ship->Dressing.Cannon.Num() : 0;
					Expected += Tile.Kind == EDwTileKind::Obstacle ? (Tile.BlockerHeight >= 2 ? Ship->Dressing.Mast.Num() : Ship->Dressing.LowObstacle.Num()) : 0;
					Expected += Tile.Kind == EDwTileKind::Ladder ? Ship->Dressing.Ladder.Num() : 0;
					Expected += Tile.Kind == EDwTileKind::Nest ? Ship->Dressing.Nest.Num() : 0;
				}
				TestTrue(TEXT("Some props"), Expected > 0);
				TestEqual(TEXT("One component per piece per feature tile"), Ship->GetDressingCount(), Expected);
			}
		});
	});

	Describe("Pirates", [this]()
	{
		It("have a mesh, a weapon in hand, idle and run loops and a side ring", [this]()
		{
			if (!StartContentFight(/*bWatched*/ false)) { return; }
			for (const ADwCombatUnit* Unit : F.Combat->GetUnits())
			{
				TestNotNull(TEXT("Pirate mesh"), Unit->Character->GetSkeletalMeshAsset());
				TestFalse(TEXT("Grey-box body hidden"), Unit->Body->IsVisible());
				TestTrue(TEXT("Ring shown"), Unit->SideRing->IsVisible() && Unit->RingMaterial);
				TestNotNull(TEXT("Idle"), Unit->IdleAnimation.Get());
				TestNotNull(TEXT("Run"), Unit->MoveAnimation.Get());
				TestTrue(TEXT("Something in the right hand"), Unit->Attachments.ContainsByPredicate([](const FDwMeshPiece& Piece) { return Piece.Socket == TEXT("hand_r") && Piece.Mesh; }));
			}
		});

		It("look different on the enemy ship", [this]()
		{
			if (!StartContentFight(/*bWatched*/ false)) { return; }
			TSet<UClass*> Crew, Raiders;
			for (const ADwCombatUnit* Unit : F.Combat->GetUnits())
			{
				(Unit->GetSide() == EDwSide::Player ? Crew : Raiders).Add(Unit->GetClass());
			}
			TestEqual(TEXT("No pirate model on both sides"), Crew.Intersect(Raiders).Num(), 0);
		});

		It("switch between the idle and the run loop", [this]()
		{
			if (!StartContentFight(/*bWatched*/ false)) { return; }
			ADwCombatUnit* Unit = F.Combat->GetUnits()[0];
			TestFalse(TEXT("Starts idle"), Unit->IsMoving());
			Unit->SetMoving(true);
			TestTrue(TEXT("Moving"), Unit->IsMoving());
			Unit->SetMoving(false);
			TestFalse(TEXT("Idle again"), Unit->IsMoving());
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
