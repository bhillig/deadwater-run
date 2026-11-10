// Copyright Brandon Hillig 2026

#include "Combat/DwCombatUnit.h"

#include "Abilities/DwAbilitySystemComponent.h"
#include "Abilities/DwAttributeSets.h"
#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/DwUnitDefinition.h"
#include "Core/DwGameplayTags.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ADwCombatUnit::ADwCombatUnit()
{
	// Turn logic never ticks (architecture rule 4); presentation components enable their own tick if needed.
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	AbilitySystem = CreateDefaultSubobject<UDwAbilitySystemComponent>(TEXT("AbilitySystem"));
	Attributes = CreateDefaultSubobject<UDwUnitAttributeSet>(TEXT("Attributes")); // found by the ASC as an owner subobject

	// Placeholder: a 0.6 x 0.6 x 1.6 m cylinder standing on the tile. BP_Unit_* replace the mesh.
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetRootComponent());
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterial.Succeeded())
	{
		TintBase = BasicMaterial.Object;
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Body->SetStaticMesh(Cylinder.Object);
		Body->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.6f));
		Body->SetRelativeLocation(FVector(0.f, 0.f, 80.f)); // the engine cylinder is 100 tall, centred
	}

	// Synty characters face +Y with the feet at the origin; the actor's forward is +X.
	Character = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Character"));
	Character->SetupAttachment(GetRootComponent());
	Character->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Character->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Character->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	// Dedicated servers don't exist here, but commandlets and tests do: never tick poses nobody renders.
	Character->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	SideRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SideRing"));
	SideRing->SetupAttachment(GetRootComponent());
	SideRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SideRing->SetCastShadow(false);
	SideRing->SetUsingAbsoluteRotation(true); // the ring doesn't turn with the pirate
	SideRing->SetRelativeLocation(FVector(0.f, 0.f, 4.f));
	SideRing->SetRelativeScale3D(FVector(0.9f, 0.9f, 1.f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded())
	{
		SideRing->SetStaticMesh(Plane.Object);
	}
	SideRing->SetVisibility(false);
}

void ADwCombatUnit::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const bool bHasCharacter = Character->GetSkeletalMeshAsset() != nullptr;
	Body->SetVisibility(!bHasCharacter);
	SideRing->SetVisibility(RingMaterial != nullptr);
	DwMeshPieces::Destroy(AttachmentComponents);
	if (bHasCharacter)
	{
		DwMeshPieces::Spawn(Character, Attachments, FTransform::Identity, AttachmentComponents);
	}
}

void ADwCombatUnit::BeginPlay()
{
	Super::BeginPlay();
	PlayLoop(IdleAnimation, /*bRandomStart*/ true); // a crew breathing in step looks like a parade
}

void ADwCombatUnit::SetMoving(bool bInMoving)
{
	if (bMoving != bInMoving)
	{
		bMoving = bInMoving;
		PlayLoop(bMoving && MoveAnimation ? MoveAnimation : IdleAnimation, /*bRandomStart*/ false);
	}
}

void ADwCombatUnit::PlayLoop(UAnimationAsset* Animation, bool bRandomStart)
{
	if (!Animation || !Character->GetSkeletalMeshAsset())
	{
		return;
	}
	Character->PlayAnimation(Animation, /*bLooping*/ true);
	if (bRandomStart)
	{
		Character->SetPosition(FMath::FRand() * Animation->GetPlayLength(), /*bFireNotifies*/ false);
	}
}

UAbilitySystemComponent* ADwCombatUnit::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ADwCombatUnit::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AbilitySystem->InitAbilityActorInfo(this, this);
}

void ADwCombatUnit::InitializeFromSpawnParams(const FDwUnitSpawnParams& Params)
{
	Handle = Params.Handle;
	Side = Params.Side;
	Tile = Params.Tile;
	RosterId = Params.RosterId;
	Definition = Params.Definition;
	bIsCaptain = Params.bIsCaptain;
	GrantedAbilities.Reset();
	ApplySideColour();
}

bool ADwCombatUnit::IsAlive() const
{
	return AbilitySystem && !AbilitySystem->HasMatchingGameplayTag(DwTags::State_Dead);
}

void ADwCombatUnit::OnDied_Implementation()
{
	SetActorHiddenInGame(true);
}

void ADwCombatUnit::ApplySideColour()
{
	// The captain's ring is gold only on the player's side: enemy crews have no captain to protect.
	const FLinearColor Colour = bIsCaptain && Side == EDwSide::Player ? CaptainColor : (Side == EDwSide::Player ? PlayerColor : EnemyColor);
	if (RingMaterial)
	{
		UMaterialInstanceDynamic* Ring = UMaterialInstanceDynamic::Create(RingMaterial, this);
		Ring->SetVectorParameterValue(TEXT("Color"), FLinearColor(Colour.R, Colour.G, Colour.B, RingFillOpacity)); // alpha = opacity inside the ring
		SideRing->SetMaterial(0, Ring);
	}
	// Pirate meshes keep their own materials; only the grey-box cylinder is tinted.
	if (TintBase && !Character->GetSkeletalMeshAsset())
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(TintBase, this);
		Material->SetVectorParameterValue(TEXT("Color"), bIsCaptain ? CaptainColor : Colour);
		Body->SetMaterial(0, Material);
	}
}
