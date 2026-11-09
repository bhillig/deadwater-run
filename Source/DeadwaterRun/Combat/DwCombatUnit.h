// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "Core/DwTypes.h"
#include "DwCombatUnit.generated.h"

class UDwAbilityDefinition;
class UDwAbilitySystemComponent;
class UDwUnitAttributeSet;
class UDwUnitDefinition;
class UStaticMeshComponent;
class UMaterialInterface;
class UTextRenderComponent;
struct FOnAttributeChangeData;

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
 * this actor mirrors it. BP_Unit_* subclasses add mesh, animation and montages.
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

	/** Placeholder body until BP_Unit_* meshes exist (DW-307). Tinted by side when its material has a "Color" parameter. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadwater")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Placeholder name and HP above the unit until the HUD's unit frames exist (DW-107). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadwater")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater|Placeholder")
	FLinearColor PlayerColor = FLinearColor(0.1f, 0.35f, 0.9f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater|Placeholder")
	FLinearColor EnemyColor = FLinearColor(0.85f, 0.15f, 0.1f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deadwater|Placeholder")
	FLinearColor CaptainColor = FLinearColor(0.95f, 0.75f, 0.1f);

private:
	void ApplyPlaceholderTint();
	UPROPERTY() TObjectPtr<UMaterialInterface> TintBase;
	void UpdateLabel();
	void HandleStatChanged(const FOnAttributeChangeData& Data);

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
