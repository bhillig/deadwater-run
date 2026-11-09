// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DwEventOutcome.generated.h"

/**
 * One thing an event choice does (gold, ship health, crew health, item, ...). Edited inline on UDwEventDefinition.
 * DW-004 declares the abstract bases so event definitions are typed; DW-205 adds FDwEventContext, Apply() and the
 * concrete subclasses (Docs/Architecture/11-Events.md "UDwEventOutcome").
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class DEADWATERRUN_API UDwEventOutcome : public UObject
{
	GENERATED_BODY()

public:
	/** "You will ..." line for the event screen and data validation. */
	virtual FText Describe() const { return FText::GetEmpty(); }
};

/** A condition that must hold for an event choice to be enabled (_MinGold, _CrewCountAtLeast, _HasItem, _CaptainIs in DW-205). */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class DEADWATERRUN_API UDwEventRequirement : public UObject
{
	GENERATED_BODY()

public:
	/** Shown on a disabled choice ("Requires 50 gold"). */
	virtual FText Describe() const { return FText::GetEmpty(); }
};
