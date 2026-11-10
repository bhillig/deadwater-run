// Copyright Brandon Hillig 2026

#include "Combat/DwShip.h"

#include "Abilities/DwAbilitySystemComponent.h"
#include "Abilities/DwAttributeSets.h"
#include "Abilities/DwGameplayAbility.h"
#include "Abilities/DwGameplayEffects.h"
#include "Board/DwBoard.h"
#include "Combat/DwCombatTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Engine/World.h"
#include "Presentation/DwPresentationSubsystem.h"

ADwShip::ADwShip()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Hull = CreateDefaultSubobject<USceneComponent>(TEXT("Hull"));
	Hull->SetupAttachment(GetRootComponent());
	AbilitySystem = CreateDefaultSubobject<UDwAbilitySystemComponent>(TEXT("AbilitySystem"));
	Attributes = CreateDefaultSubobject<UDwShipAttributeSet>(TEXT("Attributes"));
}

bool ADwShip::WantsVisuals() const
{
	// Tests and the simulator run fights nobody watches; the Blueprint editor preview has no presentation subsystem.
	const UWorld* World = GetWorld();
	return World && !(World->IsGameWorld() && UDwPresentationSubsystem::IsInstant(this));
}

void ADwShip::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	DwMeshPieces::Destroy(HullComponents);
	if (WantsVisuals())
	{
		DwMeshPieces::Spawn(Hull, HullPieces, FTransform::Identity, HullComponents);
	}
}

void ADwShip::BuildVisuals(const ADwBoard& Board, const FTransform& MeshAlignment)
{
	DwMeshPieces::Destroy(DressingComponents);
	if (!WantsVisuals())
	{
		return;
	}
	Hull->SetRelativeTransform(MeshAlignment);

	const FDwGridModel& Grid = Board.GetGrid();
	const FTransform ToDeck = GetActorTransform().Inverse(); // world -> deck space (mirrored for the enemy)
	auto DeckPoint = [&](FIntPoint Tile) { return ToDeck.TransformPosition(Board.TileToWorld(Tile)); };

	for (int32 Index = 0; Index < Grid.Tiles.Num(); ++Index)
	{
		const FDwTile& Tile = Grid.Tiles[Index];
		if (Tile.Owner != Side)
		{
			continue;
		}
		const FIntPoint Coord = Grid.ToTile(Index);
		const FVector Centre = DeckPoint(Coord);
		if (Tile.Tags.HasTagExact(DwTags::Tile_Cannon))
		{
			DwMeshPieces::Spawn(GetRootComponent(), Dressing.Cannon, FTransform(Centre), DressingComponents);
		}
		switch (Tile.Kind)
		{
		case EDwTileKind::Obstacle:
			if (Tile.BlockerHeight >= 2)
			{
				// Hull space, so the mast stands on the tile but keeps the hull's height and scale.
				const FVector Foot = MeshAlignment.InverseTransformPosition(Centre);
				DwMeshPieces::Spawn(Hull, Dressing.Mast, FTransform(FVector(Foot.X, Foot.Y, 0.f)), DressingComponents);
			}
			else
			{
				DwMeshPieces::Spawn(GetRootComponent(), Dressing.LowObstacle, FTransform(Centre), DressingComponents);
			}
			break;
		case EDwTileKind::Ladder:
		{
			// Lean the ladder against the nest it climbs to.
			FRotator Facing = FRotator::ZeroRotator;
			for (const FIntPoint& Direction : FDwGridModel::Directions)
			{
				const FDwTile* Neighbour = Grid.GetTile(Coord + Direction);
				if (Neighbour && Neighbour->Kind == EDwTileKind::Nest && Neighbour->Owner == Side)
				{
					const FVector ToNest = DeckPoint(Coord + Direction) - Centre;
					Facing.Yaw = FMath::RadiansToDegrees(FMath::Atan2(ToNest.Y, ToNest.X));
					break;
				}
			}
			DwMeshPieces::Spawn(GetRootComponent(), Dressing.Ladder, FTransform(Facing, Centre), DressingComponents);
			break;
		}
		case EDwTileKind::Nest:
			DwMeshPieces::Spawn(GetRootComponent(), Dressing.Nest, FTransform(Centre), DressingComponents);
			break;
		default:
			break;
		}
	}
}

UAbilitySystemComponent* ADwShip::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ADwShip::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AbilitySystem->InitAbilityActorInfo(this, this);
}

void ADwShip::InitializeFromSetup(EDwSide InSide, const FDwShipSetup& Setup, const TArray<FIntPoint>& InCannonTiles)
{
	Side = InSide;
	CannonTiles = InCannonTiles;

	FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(DwGas::GetSharedEffects().InitShipStats, 1, AbilitySystem->MakeEffectContext());
	if (Spec.IsValid())
	{
		const float Health = Setup.Health < 0.f ? Setup.MaxHealth : FMath::Min(Setup.Health, Setup.MaxHealth);
		UDwGE_InitShipStats::SetValues(*Spec.Data, Setup.MaxHealth, Health, Setup.CannonDamage);
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}
	for (const TSubclassOf<UGameplayEffect>& Upgrade : Setup.UpgradeEffects)
	{
		if (Upgrade)
		{
			AbilitySystem->ApplyGameplayEffectToSelf(Upgrade->GetDefaultObject<UGameplayEffect>(), 1, AbilitySystem->MakeEffectContext());
		}
	}
}

float ADwShip::GetHealth() const
{
	return Attributes ? Attributes->GetHealth() : 0.f;
}

float ADwShip::GetMaxHealth() const
{
	return Attributes ? Attributes->GetMaxHealth() : 0.f;
}

float ADwShip::GetCannonDamage() const
{
	return Attributes ? Attributes->GetCannonDamage() : 0.f;
}

void ADwShip::OnSunk_Implementation()
{
}
