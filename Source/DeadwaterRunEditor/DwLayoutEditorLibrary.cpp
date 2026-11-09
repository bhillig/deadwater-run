// Copyright Brandon Hillig 2026

#include "DwLayoutEditorLibrary.h"

#include "Board/DwBoardBuilder.h"
#include "Board/DwDeckLayout.h"

bool UDwLayoutEditorLibrary::ImportAscii(UDwDeckLayout* Layout, const TArray<FString>& Rows, TArray<FString>& OutErrors)
{
	OutErrors.Reset();
	if (!Layout)
	{
		OutErrors.Add(TEXT("No layout."));
		return false;
	}
	const FDwDeckSource Deck = FDwDeckAscii::Parse(Rows, &OutErrors);
	Layout->Modify();
	Layout->Size = Deck.Size;
	Layout->Tiles = Deck.Tiles;
	Layout->MarkPackageDirty();
	return OutErrors.IsEmpty();
}

TArray<FString> UDwLayoutEditorLibrary::ExportAscii(const UDwDeckLayout* Layout)
{
	return Layout ? FDwDeckAscii::Export(FDwDeckSource::FromLayout(*Layout)) : TArray<FString>();
}

TArray<FString> UDwLayoutEditorLibrary::ValidateLayout(const UDwDeckLayout* Layout)
{
	return Layout ? Layout->Validate() : TArray<FString>{ TEXT("No layout.") };
}
