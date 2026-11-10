// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "Core/DwTypes.h"
#include "Presentation/DwMeshPiece.h"
#include "DwCombatUnit.generated.h"

class UAnimationAsset;
class USkeletalMeshComponent;
class UDwAbilityDefinition;
class UDwAbilitySystemComponent;
class UDwUnitAttributeSet;
class UDwUnitDefinition;
class UStaticMeshComponent;
class UMaterialInterface;

/** What the combat subsystem tells a unit when it spawns it (Docs/Architecture/03 "ADwCombatUnit"). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwUnitSpawnParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Unit") FDwUnitHandle Handle;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") EDwSide Side = EDwSide::Player;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") FIntPoint Tile = FIntPoint::ZeroValue;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") FGuid RosterId;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") TObjectPtr<const UDwUnitDefinition> Definition;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") bool bIsCaptain = false;
};

/**
 * A combatant on the board (crew, enemy pirate, Kraken body, tentacle). Owns its ASC and unit attribute set
 * (Docs/Architecture/02 D1: the ASC lives on the per-fight actor; the run is pure data).
 * Identity (handle, side, tile, roster id) is set by UDwCombatSubsystem; the subsystem's grid is the truth for tiles,
 * this actor mirrors it. BP_Unit_* set the pirate mesh, attachments and looping animations as class defaults.
 */
UCLASS(Blueprintable)
class DEADWATERRUN_API ADwCombatUnit : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ADwCombatUnit();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UDwAbilitySystemComponent* GetDwAbilitySystem() const { return AbilitySystem; }
	const UDwUnitAttributeSet* GetAttributes() const { return Attributes; }

	virtual void PostInitializeComponents() override;

	void InitializeFromSpawnParams(const FDwUnitSpawnParams& Params);

	UFUNCTION(BlueprintPure, Category = "Deadwater|Unit") FDwUnitHandle GetHandle() const { return Handle; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|Unit") EDwSide GetSide() const { return Side; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|Unit") FIntPoint GetTile() const { return Tile; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|Unit") FGuid GetRosterId() const { return RosterId; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|Unit") bool IsCaptain() const { return bIsCaptain; }
	UFUNCTION(BlueprintPure, Category = "Deadwater|Unit") bool IsAlive() const;
	const UDwUnitDefinition* GetDefinition() const { return Definition; }

	/** Logical tile (combat subsystem only; the presentation moves the actor). */
	void SetTile(FIntPoint InTile) { Tile = InTile; }

	/** Records a granted ability's definition, in grant order (snapshots and the action bar read it). */
	void AddGrantedAbility(const UDwAbilityDefinition* Ability) { GrantedAbilities.Add(Ability); }
	const TArray<TObjectPtr<const UDwAbilityDefinition>>& GetGrantedAbilities() const { return GrantedAbilities; }

	/** Called once when the unit dies. Default hides the actor; BP_Unit_* override to play a death animation. */
	UFUNCTION(BlueprintNativeEvent, Category = "Deadwater|Unit")
	void OnDied();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Walking along a path (UDwAT_MoveAlongPath): swaps the looping idle for the looping move animation. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Unit")
	void SetMoving(bool bInMoving);

	UFUNCTION(BlueprintPure, Category = "Deadwater|Unit") bool IsMoving() const { return bMoving; }

	/** Grey-box body: shown only while Character has no mesh. Tinted by side when its material has a "Color" parameter. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadwater")
	TObjectPtr<UStaticMeshComponent> Body;

	/** The pirate (DW-307). BP_Unit_* set the mesh; +X of the actor is forward. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadwater")
	TObjectPtr<USkeletalMeshComponent> Character;

	/** Flat ring under the unit in its side's colour: both crews use the same pirate models. Hidden without RingMaterial. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadwater")
	TObjectPtr<UStaticMeshComponent> SideRing;

	/** Hats, hair, weapons: static meshes on Character's bones or sockets (FDwMeshPiece::Socket). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater|Visuals")
	TArray<FDwMeshPiece> Attachments;

	/** Looping animations played directly on Character (no anim Blueprint until attack animations exist, open question O2). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater|Visuals")
	TObjectPtr<UAnimationAsset> IdleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater|Visuals")
	TObjectPtr<UAnimationAsset> MoveAnimation;

	/** Needs a "Color" vector parameter (M_UnitRing). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater|Visuals")
	TObjectPtr<UMaterialInterface> RingMaterial;

	/** How solid the disc inside the ring is (the outline itself uses the material's EdgeOpacity). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater|Visuals", meta = (ClampMin = "0", ClampMax = "1"))
	float RingFillOpacity = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater|Side Colours")
	FLinearColor PlayerColor = FLinearColor(0.1f, 0.35f, 0.9f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater|Side Colours")
	FLinearColor EnemyColor = FLinearColor(0.85f, 0.15f, 0.1f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater|Side Colours")
	FLinearColor CaptainColor = FLinearColor(0.95f, 0.75f, 0.1f);

protected:
	virtual void BeginPlay() override;

private:
	void ApplySideColour();
	void PlayLoop(UAnimationAsset* Animation, bool bRandomStart);
	UPROPERTY() TObjectPtr<UMaterialInterface> TintBase;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> AttachmentComponents;
	bool bMoving = false;

	UPROPERTY(VisibleAnywhere, Category = "Deadwater")
	TObjectPtr<UDwAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UDwUnitAttributeSet> Attributes;

	UPROPERTY(VisibleInstanceOnly, Category = "Deadwater|Unit") FDwUnitHandle Handle;
	UPROPERTY(VisibleInstanceOnly, Category = "Deadwater|Unit") EDwSide Side = EDwSide::Player;
	UPROPERTY(VisibleInstanceOnly, Category = "Deadwater|Unit") FIntPoint Tile = FIntPoint::ZeroValue;
	UPROPERTY(VisibleInstanceOnly, Category = "Deadwater|Unit") FGuid RosterId;
	UPROPERTY(VisibleInstanceOnly, Category = "Deadwater|Unit") bool bIsCaptain = false;
	UPROPERTY(VisibleInstanceOnly, Category = "Deadwater|Unit") TObjectPtr<const UDwUnitDefinition> Definition;
	UPROPERTY(VisibleInstanceOnly, Category = "Deadwater|Unit") TArray<TObjectPtr<const UDwAbilityDefinition>> GrantedAbilities;
};
