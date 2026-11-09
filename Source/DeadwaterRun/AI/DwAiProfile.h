// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Core/DwDefinitionBase.h"
#include "DwAiProfile.generated.h"

/**
 * A crew personality for the utility AI. Owned by ai-engineer.
 * DW-004 only declares the asset type so encounter definitions can reference profiles; DW-106 adds FDwAiWeights,
 * Temperature, MaxMoveCandidates, bPreferCannons, bBoarders and FDwAiTuningRow (Docs/Architecture/07-Enemy-AI.md).
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwAiProfile : public UDwDefinitionBase
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetType GetDwAssetType() const override { return FTypes::AiProfile; }
};
