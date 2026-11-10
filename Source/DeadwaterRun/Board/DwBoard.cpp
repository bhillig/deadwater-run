// Copyright Brandon Hillig 2026

#include "Board/DwBoard.h"

#include "Board/DwBoardBuilder.h"
#include "Board/DwDeckLayout.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Core/DwGameplayTags.h"
#include "UObject/ConstructorHelpers.h"

namespace DwBoardPresentation
{
	// The engine plane is 100x100 units.
	constexpr float PlaneSize = 100.f;
}

ADwBoard::ADwBoard()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		TileMesh = PlaneMesh.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterial.Succeeded())
	{
		HighlightBaseMaterial = BasicMaterial.Object;
	}
	LayerColors = {
		{ EDwHighlightLayer::Move, FLinearColor(0.1f, 0.4f, 0.9f) },
		{ EDwHighlightLayer::Path, FLinearColor(0.95f, 0.85f, 0.2f) },
		{ EDwHighlightLayer::Target, FLinearColor(0.95f, 0.1f, 0.85f) },
		{ EDwHighlightLayer::AreaOfEffect, FLinearColor(0.95f, 0.45f, 0.1f) },
		{ EDwHighlightLayer::Danger, FLinearColor(0.6f, 0.05f, 0.35f) },
		{ EDwHighlightLayer::Hover, FLinearColor(0.9f, 0.9f, 0.9f) },
		{ EDwHighlightLayer::Preview, FLinearColor(0.3f, 0.8f, 0.3f) } };

	auto MakeTileLayer = [this](const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* Layer = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Layer->SetupAttachment(Root);
		Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Layer->SetCastShadow(false);
		Layer->SetStaticMesh(TileMesh);
		return Layer;
	};
	PlayerDeck = MakeTileLayer(TEXT("PlaceholderPlayerDeck"));
	EnemyDeck = MakeTileLayer(TEXT("PlaceholderEnemyDeck"));
	Features = MakeTileLayer(TEXT("PlaceholderFeatures"));
	GridOverlay = MakeTileLayer(TEXT("GridOverlay"));
	Water = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderWater"));
	Water->SetupAttachment(Root);
	Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Water->SetCastShadow(false);
	Water->SetStaticMesh(TileMesh);
	Water->SetVisibility(false);

	const UEnum* LayerEnum = StaticEnum<EDwHighlightLayer>();
	for (int32 Index = 0; Index < static_cast<int32>(EDwHighlightLayer::Count); ++Index)
	{
		const FName Name(*FString::Printf(TEXT("Highlight_%s"), *LayerEnum->GetNameStringByValue(Index)));
		UInstancedStaticMeshComponent* Layer = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Layer->SetupAttachment(Root);
		Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Layer->SetCastShadow(false);
		Layer->SetStaticMesh(TileMesh);
		HighlightLayers.Add(Layer);
	}
}

void ADwBoard::BeginPlay()
{
	Super::BeginPlay();
	GetLayer(EDwHighlightLayer::Preview)->ClearInstances();
}

void ADwBoard::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLayerMaterials();

	UInstancedStaticMeshComponent* Preview = GetLayer(EDwHighlightLayer::Preview);
	Preview->ClearInstances();
	if (PreviewLayout && PreviewLayout->Tiles.Num() == PreviewLayout->Size.X * PreviewLayout->Size.Y)
	{
		const float Size = PreviewLayout->TileSize > 0.f ? PreviewLayout->TileSize : TileSize;
		SetGrid(FDwBoardBuilder::BuildSingleDeck(FDwDeckSource::FromLayout(*PreviewLayout), EDwSide::Player), Size);
		for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
		{
			if (Grid.IsStandable(Grid.ToTile(Index)))
			{
				Preview->AddInstance(MakeInstanceTransform(Grid.ToTile(Index), EDwHighlightLayer::Preview));
			}
		}
	}
}

void ADwBoard::SetGrid(const FDwGridModel& InGrid, float InTileSize, int32 InSeamColumn, float InSeamGap)
{
	Grid = InGrid;
	TileSize = FMath::Max(InTileSize, 1.f);
	SeamColumn = InSeamColumn > 0 && InSeamColumn < Grid.Size.X ? InSeamColumn : INDEX_NONE;
	SeamGap = SeamColumn != INDEX_NONE ? FMath::Max(InSeamGap, 0.f) : 0.f;
	ClearAllHighlights();
	ApplyLayerMaterials();
	PlayerDeck->ClearInstances();
	EnemyDeck->ClearInstances();
	Features->ClearInstances();
	Water->SetVisibility(false);
	GridOverlay->ClearInstances();
	if (bDrawPlaceholderDecks)
	{
		DrawPlaceholderDecks();
	}
	if (bDrawGrid)
	{
		DrawGrid();
	}
}

float ADwBoard::ColumnToLocalX(int32 Column) const
{
	return Column * TileSize + (SeamColumn != INDEX_NONE && Column >= SeamColumn ? SeamGap : 0.f);
}

bool ADwBoard::LocalXToColumn(float LocalX, int32& OutColumn) const
{
	if (SeamColumn != INDEX_NONE && LocalX >= SeamColumn * TileSize)
	{
		if (LocalX < SeamColumn * TileSize + SeamGap)
		{
			return false; // between the two decks
		}
		LocalX -= SeamGap;
	}
	OutColumn = FMath::FloorToInt(LocalX / TileSize);
	return true;
}

FVector ADwBoard::TileCentreLocal(FIntPoint Tile) const
{
	const FDwTile* T = Grid.GetTile(Tile);
	return FVector(ColumnToLocalX(Tile.X) + 0.5f * TileSize, (Tile.Y + 0.5f) * TileSize, T ? T->Elevation * ElevationHeight : 0.f);
}

FBox2D ADwBoard::GetLocalBounds() const
{
	return FBox2D(FVector2D::ZeroVector, FVector2D(ColumnToLocalX(FMath::Max(Grid.Size.X - 1, 0)) + TileSize, Grid.Size.Y * TileSize));
}

FVector ADwBoard::TileToWorld(FIntPoint Tile) const
{
	return GetActorTransform().TransformPosition(TileCentreLocal(Tile));
}

bool ADwBoard::WorldToTile(const FVector& WorldPoint, FIntPoint& OutTile) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldPoint);
	FIntPoint Tile(0, FMath::FloorToInt(Local.Y / TileSize));
	if (!LocalXToColumn(Local.X, Tile.X) || !Grid.IsInBounds(Tile))
	{
		return false;
	}
	OutTile = Tile;
	return true;
}

bool ADwBoard::RayToTile(const FVector& RayOrigin, const FVector& RayDirection, FIntPoint& OutTile) const
{
	const FVector LocalOrigin = GetActorTransform().InverseTransformPosition(RayOrigin);
	const FVector LocalDir = GetActorTransform().InverseTransformVectorNoScale(RayDirection).GetSafeNormal();
	if (FMath::IsNearlyZero(LocalDir.Z))
	{
		return false;
	}

	// Highest elevation first, so the nest is picked before the deck tile it overhangs.
	for (int32 Elevation = 1; Elevation >= 0; --Elevation)
	{
		const float T = (Elevation * ElevationHeight - LocalOrigin.Z) / LocalDir.Z;
		if (T < 0.f)
		{
			continue;
		}
		const FVector Hit = LocalOrigin + LocalDir * T;
		FIntPoint Tile(0, FMath::FloorToInt(Hit.Y / TileSize));
		const FDwTile* Data = LocalXToColumn(Hit.X, Tile.X) ? Grid.GetTile(Tile) : nullptr;
		if (Data && Data->Elevation == Elevation && Data->Kind != EDwTileKind::None)
		{
			OutTile = Tile;
			return true;
		}
	}
	return false;
}

void ADwBoard::ShowHighlights(EDwHighlightLayer Layer, const TArray<FIntPoint>& Tiles)
{
	UInstancedStaticMeshComponent* Component = GetLayer(Layer);
	if (!Component)
	{
		return;
	}
	Component->ClearInstances();
	TArray<FTransform> Transforms;
	Transforms.Reserve(Tiles.Num());
	for (const FIntPoint& Tile : Tiles)
	{
		if (Grid.IsInBounds(Tile))
		{
			Transforms.Add(MakeInstanceTransform(Tile, Layer));
		}
	}
	Component->AddInstances(Transforms, /*bShouldReturnIndices*/ false);
}

void ADwBoard::ClearHighlights(EDwHighlightLayer Layer)
{
	if (UInstancedStaticMeshComponent* Component = GetLayer(Layer))
	{
		Component->ClearInstances();
	}
}

void ADwBoard::ClearAllHighlights()
{
	for (int32 Index = 0; Index < static_cast<int32>(EDwHighlightLayer::Preview); ++Index)
	{
		ClearHighlights(static_cast<EDwHighlightLayer>(Index));
	}
}

int32 ADwBoard::GetHighlightCount(EDwHighlightLayer Layer) const
{
	const UInstancedStaticMeshComponent* Component = GetLayer(Layer);
	return Component ? Component->GetInstanceCount() : 0;
}

UInstancedStaticMeshComponent* ADwBoard::GetLayer(EDwHighlightLayer Layer) const
{
	const int32 Index = static_cast<int32>(Layer);
	return HighlightLayers.IsValidIndex(Index) ? HighlightLayers[Index].Get() : nullptr;
}

FTransform ADwBoard::MakeInstanceTransform(FIntPoint Tile, EDwHighlightLayer Layer) const
{
	// Instances are in component space = board space. Each layer sits a little higher so overlaps stay readable.
	const float Scale = TileSize * HighlightFill / DwBoardPresentation::PlaneSize;
	return FTransform(FQuat::Identity, TileCentreLocal(Tile) + FVector(0.f, 0.f, HighlightLift * (1 + static_cast<int32>(Layer))), FVector(Scale, Scale, 1.f));
}

void ADwBoard::ApplyLayerMaterials()
{
	for (int32 Index = 0; Index < HighlightLayers.Num(); ++Index)
	{
		if (HighlightLayers[Index])
		{
			HighlightLayers[Index]->SetStaticMesh(TileMesh);
			if (const TObjectPtr<UMaterialInterface>* Material = LayerMaterials.Find(static_cast<EDwHighlightLayer>(Index)))
			{
				HighlightLayers[Index]->SetMaterial(0, *Material);
			}
			else if (const FLinearColor* Color = LayerColors.Find(static_cast<EDwHighlightLayer>(Index)))
			{
				HighlightLayers[Index]->SetMaterial(0, MakeTint(*Color, HighlightEdgeOpacity));
			}
		}
	}
}

UMaterialInterface* ADwBoard::MakeTint(const FLinearColor& Color, float EdgeOpacity)
{
	if (!HighlightBaseMaterial)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Tint = UMaterialInstanceDynamic::Create(HighlightBaseMaterial, this);
	Tint->SetVectorParameterValue(TEXT("Color"), Color);
	if (EdgeOpacity >= 0.f)
	{
		Tint->SetScalarParameterValue(TEXT("EdgeOpacity"), EdgeOpacity); // ignored by parents without the parameter
	}
	return Tint;
}

void ADwBoard::DrawGrid()
{
	GridOverlay->SetMaterial(0, MakeTint(GridColor, GridEdgeOpacity));
	const float Scale = TileSize * HighlightFill / DwBoardPresentation::PlaneSize;
	TArray<FTransform> Transforms;
	for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
	{
		const FIntPoint Tile = Grid.ToTile(Index);
		if (Grid.IsStandable(Tile))
		{
			Transforms.Add(FTransform(FQuat::Identity, TileCentreLocal(Tile) + FVector(0.f, 0.f, HighlightLift * 0.5f), FVector(Scale, Scale, 1.f)));
		}
	}
	GridOverlay->AddInstances(Transforms, /*bShouldReturnIndices*/ false);
}

void ADwBoard::DrawPlaceholderDecks()
{
	PlayerDeck->SetMaterial(0, MakeTint(PlayerDeckColor));
	EnemyDeck->SetMaterial(0, MakeTint(EnemyDeckColor));
	Features->SetMaterial(0, MakeTint(FeatureColor));

	const float DeckScale = TileSize * 0.97f / DwBoardPresentation::PlaneSize;
	const float FeatureScale = TileSize * 0.45f / DwBoardPresentation::PlaneSize;
	for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
	{
		const FDwTile& Tile = Grid.Tiles[Index];
		if (Tile.Kind == EDwTileKind::None || Tile.Kind == EDwTileKind::Water)
		{
			continue;
		}
		const FIntPoint Coord = Grid.ToTile(Index);
		const FVector Centre = TileCentreLocal(Coord) + FVector(0.f, 0.f, 0.5f);
		(Tile.Owner == EDwSide::Enemy ? EnemyDeck : PlayerDeck)->AddInstance(FTransform(FQuat::Identity, Centre, FVector(DeckScale, DeckScale, 1.f)));
		const bool bFeature = Tile.Kind == EDwTileKind::Ladder || Tile.Kind == EDwTileKind::Obstacle || Tile.BlockerHeight > 0
			|| Tile.Tags.HasTagExact(DwTags::Tile_Cannon);
		if (bFeature)
		{
			Features->AddInstance(FTransform(FQuat::Identity, Centre + FVector(0.f, 0.f, 1.f), FVector(FeatureScale, FeatureScale, 1.f)));
		}
	}

	// A sea under both ships, wide enough to fill the framed view.
	const FVector2D Size = GetLocalBounds().GetSize();
	Water->SetRelativeLocation(FVector(Size.X * 0.5f, Size.Y * 0.5f, -20.f));
	Water->SetRelativeScale3D(FVector(Size.X * 4.f / DwBoardPresentation::PlaneSize, Size.Y * 10.f / DwBoardPresentation::PlaneSize, 1.f));
	Water->SetMaterial(0, MakeTint(WaterColor));
	Water->SetVisibility(Grid.Tiles.Num() > 0);
}
