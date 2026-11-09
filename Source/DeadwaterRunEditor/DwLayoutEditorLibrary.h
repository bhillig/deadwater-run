// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DwLayoutEditorLibrary.generated.h"

class UDwDeckLayout;

/**
 * Editor scripting entry points for deck layouts (Python / Editor Utility Widgets). Format: FDwDeckAscii.
 * DW-110 builds the details-panel painter and toolbar buttons on top of these.
 */
UCLASS()
class DEADWATERRUNEDITOR_API UDwLayoutEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Replaces the layout's Size and Tiles with the parsed rows and marks it dirty. Returns false on unknown characters. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Layout")
	static bool ImportAscii(UDwDeckLayout* Layout, const TArray<FString>& Rows, TArray<FString>& OutErrors);

	UFUNCTION(BlueprintCallable, Category = "Deadwater|Layout")
	static TArray<FString> ExportAscii(const UDwDeckLayout* Layout);

	/** UDwDeckLayout::Validate() for scripts. */
	UFUNCTION(BlueprintCallable, Category = "Deadwater|Layout")
	static TArray<FString> ValidateLayout(const UDwDeckLayout* Layout);
};
