// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Core/DwDefinitionBase.h"
#include "Data/DwDataTypes.h"
#include "DwEventDefinition.generated.h"

class UDwEventOutcome;
class UDwEventRequirement;
class UTexture2D;

/** One button on an event screen. */
USTRUCT(BlueprintType)
struct DEADWATERRUN_API FDwEventChoice
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Choice")
	FText Label;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Choice", meta = (MultiLine = true))
	FText ResultText;

	/** All must pass to enable the choice. */
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Choice")
	TArray<TObjectPtr<UDwEventRequirement>> Requirements;

	/** Applied in order. */
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Choice")
	TArray<TObjectPtr<UDwEventOutcome>> Outcomes;
};

/** A map event, authored by Brandon in the editor (Docs/Architecture/11-Events.md). */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwEventDefinition : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	static constexpr int32 MinChoices = 1;
	static constexpr int32 MaxChoices = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event")
	FText Title;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event", meta = (MultiLine = true))
	FText Body;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event")
	TSoftObjectPtr<UTexture2D> Illustration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event")
	EDwEventValence Valence = EDwEventValence::Good;

	/** Selection weight within its valence. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event", meta = (ClampMin = "0"))
	float Weight = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event", meta = (ClampMin = "0"))
	int32 MinLayer = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event", meta = (ClampMin = "0"))
	int32 MaxLayer = 99;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event")
	TArray<FDwEventChoice> Choices;

	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::Event; }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
