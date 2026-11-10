// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "DwMeshPiece.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;

/**
 * One static mesh of a visual that is assembled from data instead of Blueprint components (ship hulls, deck dressing,
 * character attachments). Data so the content library can write it and Brandon can retune it on the Blueprint defaults.
 */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwMeshPiece
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Piece")
	TObjectPtr<UStaticMesh> Mesh;

	/** Relative to the parent the owner attaches it to (hull, tile centre, bone). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Piece")
	FTransform Transform;

	/** Bone or socket on a skeletal parent; ignored otherwise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Piece")
	FName Socket;
};

namespace DwMeshPieces
{
	/**
	 * Creates, attaches and registers one component per piece (no collision; presentation only). Offset is applied on top
	 * of each piece's own transform (parent space). Components are owned by Parent's actor.
	 */
	DEADWATERRUN_API void Spawn(USceneComponent* Parent, TConstArrayView<FDwMeshPiece> Pieces, const FTransform& Offset,
		TArray<TObjectPtr<UStaticMeshComponent>>& OutComponents);

	DEADWATERRUN_API void Destroy(TArray<TObjectPtr<UStaticMeshComponent>>& Components);
}
