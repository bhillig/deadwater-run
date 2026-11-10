// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "DwSeaEnvironment.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class UPostProcessComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * One look of the sea a fight happens on (DW-307): sky, light, water and weather. Pure presentation data; no rule reads
 * it. DA_GameData.SeaMoods lists the moods a fight can roll, each with a weight.
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwSeaMood : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mood")
	FText DisplayName;

	/** Sky dome material (Synty skybox). Empty hides the dome and shows the sky atmosphere. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sky")
	TObjectPtr<UMaterialInterface> SkyMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light")
	FRotator SunRotation = FRotator(-50.f, -60.f, 0.f);

	/** Lux. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light", meta = (ClampMin = "0"))
	float SunIntensity = 6.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light")
	FLinearColor SunColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light", meta = (ClampMin = "0"))
	float SkyLightIntensity = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light")
	FLinearColor SkyLightColor = FLinearColor::White;

	/** Exposure compensation in stops on top of auto exposure. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light")
	float ExposureBias = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fog", meta = (ClampMin = "0"))
	float FogDensity = 0.01f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fog")
	FLinearColor FogColor = FLinearColor(0.45f, 0.6f, 0.8f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water")
	TObjectPtr<UMaterialInterface> WaterMaterial;

	/** Rain and the like, spawned above the board. Empty = clear weather. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weather")
	TObjectPtr<UNiagaraSystem> Weather;
};

/** A mood and how often it comes up (relative to the other entries; 0 = never rolled, still selectable by name). */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwSeaMoodChance
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mood")
	TObjectPtr<UDwSeaMood> Mood;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mood", meta = (ClampMin = "0"))
	float Weight = 1.f;
};

/**
 * The sea of L_Sea (DW-307): water, sky dome, sun, sky light, fog, weather and exposure in one actor, restyled by a
 * UDwSeaMood. Placed in the level by the content library; ADwGameMode picks a mood per fight.
 */
UCLASS()
class DEADWATERRUN_API ADwSeaEnvironment : public AActor
{
	GENERATED_BODY()

public:
	ADwSeaEnvironment();

	UFUNCTION(BlueprintCallable, Category = "Deadwater|Sea")
	void ApplyMood(const UDwSeaMood* Mood);

	UFUNCTION(BlueprintPure, Category = "Deadwater|Sea")
	const UDwSeaMood* GetMood() const { return CurrentMood; }

	/**
	 * The mood of a fight: Forced by asset name or display name if it names one of Moods (dev: -DwMood=), else a weighted
	 * roll that depends only on Seed. Never touches the combat random stream, so the same seed plays the same fight in
	 * any weather.
	 */
	static const UDwSeaMood* PickMood(TConstArrayView<FDwSeaMoodChance> Moods, int32 Seed, const FString& Forced = FString());

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Shown in the editor and until a fight picks one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sea")
	TObjectPtr<UDwSeaMood> DefaultMood;

	/** World height of the water surface; the deck plane is Z = 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sea")
	float WaterHeight = -170.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sea") TObjectPtr<UStaticMeshComponent> Water;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sea") TObjectPtr<UStaticMeshComponent> SkyDome;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sea") TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sea") TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sea") TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sea") TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sea") TObjectPtr<UNiagaraComponent> Weather;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sea") TObjectPtr<UPostProcessComponent> PostProcess;

private:
	UPROPERTY(Transient) TObjectPtr<const UDwSeaMood> CurrentMood;
};
