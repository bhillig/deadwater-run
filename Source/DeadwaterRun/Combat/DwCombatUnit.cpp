// Copyright Brandon Hillig 2026

#include "Combat/DwCombatUnit.h"

#include "Abilities/DwAbilitySystemComponent.h"
#include "Abilities/DwAttributeSets.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
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

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(GetRootComponent());
	Label->SetRelativeLocation(FVector(0.f, 0.f, 210.f));
	Label->SetUsingAbsoluteRotation(true);
	Label->SetWorldRotation(FRotator(30.f, 90.f, 0.f)); // faces the camera rig (at +Y looking -Y, pitched down)
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextBottom);
	Label->SetWorldSize(18.f);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
	ApplyPlaceholderTint();
	AbilitySystem->GetGameplayAttributeValueChangeDelegate(UDwUnitAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleStatChanged);
	AbilitySystem->GetGameplayAttributeValueChangeDelegate(UDwUnitAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::HandleStatChanged);
	UpdateLabel();
}

bool ADwCombatUnit::IsAlive() const
{
	return AbilitySystem && !AbilitySystem->HasMatchingGameplayTag(DwTags::State_Dead);
}

void ADwCombatUnit::OnDied_Implementation()
{
	SetActorHiddenInGame(true);
}

void ADwCombatUnit::ApplyPlaceholderTint()
{
	// Only the placeholder cylinder is tinted; BP_Unit_* meshes keep their own materials.
	if (!Body || !TintBase || Body->GetStaticMesh() != GetDefault<ADwCombatUnit>()->Body->GetStaticMesh())
	{
		return;
	}
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(TintBase, this);
	Material->SetVectorParameterValue(TEXT("Color"), bIsCaptain ? CaptainColor : (Side == EDwSide::Player ? PlayerColor : EnemyColor));
	Body->SetMaterial(0, Material);
}

void ADwCombatUnit::HandleStatChanged(const FOnAttributeChangeData& /*Data*/)
{
	UpdateLabel();
}

void ADwCombatUnit::UpdateLabel()
{
	if (!Label || !Attributes)
	{
		return;
	}
	const FString Name = Definition ? Definition->DisplayName.ToString() : GetName();
	Label->SetText(FText::FromString(FString::Printf(TEXT("%s%s\n%d / %d"), bIsCaptain ? TEXT("Capt. ") : TEXT(""), *Name,
		FMath::RoundToInt(Attributes->GetHealth()), FMath::RoundToInt(Attributes->GetMaxHealth()))));
	Label->SetTextRenderColor((Side == EDwSide::Player ? PlayerColor : EnemyColor).ToFColor(true));
}
