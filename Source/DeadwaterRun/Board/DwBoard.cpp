// Copyright Brandon Hillig 2026

#include "Board/DwBoard.h"

#include "Board/DwBoardBuilder.h"
#include "Board/DwDeckLayout.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
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

void ADwBoard::SetGrid(const FDwGridModel& InGrid, float InTileSize)
{
	Grid = InGrid;
	TileSize = FMath::Max(InTileSize, 1.f);
	ClearAllHighlights();
}

FVector ADwBoard::TileToWorld(FIntPoint Tile) const
{
	const FDwTile* T = Grid.GetTile(Tile);
	const float Z = T ? T->Elevation * ElevationHeight : 0.f;
	return GetActorTransform().TransformPosition(FVector((Tile.X + 0.5f) * TileSize, (Tile.Y + 0.5f) * TileSize, Z));
}

bool ADwBoard::WorldToTile(const FVector& WorldPoint, FIntPoint& OutTile) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldPoint);
	const FIntPoint Tile(FMath::FloorToInt(Local.X / TileSize), FMath::FloorToInt(Local.Y / TileSize));
	if (!Grid.IsInBounds(Tile))
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
		const FIntPoint Tile(FMath::FloorToInt(Hit.X / TileSize), FMath::FloorToInt(Hit.Y / TileSize));
		const FDwTile* Data = Grid.GetTile(Tile);
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
	const FDwTile* T = Grid.GetTile(Tile);
	const float Z = (T ? T->Elevation * ElevationHeight : 0.f) + HighlightLift * (1 + static_cast<int32>(Layer));
	const float Scale = TileSize * HighlightFill / DwBoardPresentation::PlaneSize;
	return FTransform(FQuat::Identity, FVector((Tile.X + 0.5f) * TileSize, (Tile.Y + 0.5f) * TileSize, Z), FVector(Scale, Scale, 1.f));
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
		}
	}
}
