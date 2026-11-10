// Copyright Brandon Hillig 2026

#include "Presentation/DwMeshPiece.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

void DwMeshPieces::Spawn(USceneComponent* Parent, TConstArrayView<FDwMeshPiece> Pieces, const FTransform& Offset,
	TArray<TObjectPtr<UStaticMeshComponent>>& OutComponents)
{
	AActor* Owner = Parent ? Parent->GetOwner() : nullptr;
	if (!Owner)
	{
		return;
	}
	for (const FDwMeshPiece& Piece : Pieces)
	{
		if (!Piece.Mesh)
		{
			continue;
		}
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		Component->SetStaticMesh(Piece.Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetupAttachment(Parent, Piece.Socket);
		Component->SetRelativeTransform(Piece.Transform * Offset);
		Component->RegisterComponent();
		OutComponents.Add(Component);
	}
}

void DwMeshPieces::Destroy(TArray<TObjectPtr<UStaticMeshComponent>>& Components)
{
	for (UStaticMeshComponent* Component : Components)
	{
		if (IsValid(Component))
		{
			Component->DestroyComponent();
		}
	}
	Components.Reset();
}
