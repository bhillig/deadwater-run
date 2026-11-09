// Copyright Brandon Hillig 2026

#include "Abilities/DwTurnDurationComponent.h"

#include "GameplayEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "DwTurnDurationComponent"

#if WITH_EDITOR
EDataValidationResult UDwTurnDurationComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const UGameplayEffect* Owner = GetOwner();
	if (Owner && Owner->DurationPolicy != EGameplayEffectDurationType::Infinite)
	{
		Context.AddError(FText::Format(LOCTEXT("NotInfinite", "{0}: a turn duration needs an Infinite effect (time-based durations would also expire it)."),
			FText::FromString(Owner->GetName())));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
