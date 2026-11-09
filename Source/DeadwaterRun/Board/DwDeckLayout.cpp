// Copyright Brandon Hillig 2026

#include "Board/DwDeckLayout.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "DwDeckLayout"

#if WITH_EDITOR
EDataValidationResult UDwDeckLayout::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Size.X <= 0 || Size.Y <= 0)
	{
		Context.AddError(FText::Format(LOCTEXT("BadSize", "Size must be positive, got {0}x{1}."), Size.X, Size.Y));
		Result = EDataValidationResult::Invalid;
	}
	else if (Tiles.Num() != Size.X * Size.Y)
	{
		Context.AddError(FText::Format(LOCTEXT("TileCount", "Tiles has {0} entries, Size {1}x{2} needs {3}."),
			Tiles.Num(), Size.X, Size.Y, Size.X * Size.Y));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
