// Copyright Brandon Hillig 2026

#include "Sim/DwSimPolicy.h"

#include "AI/DwAiProfile.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "DwSimPolicy"

#if WITH_EDITOR
EDataValidationResult UDwSimPolicy::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!PlayerProfile)
	{
		Context.AddError(LOCTEXT("NoProfile", "PlayerProfile is empty: the simulator has nothing to play the crew with."));
		Result = EDataValidationResult::Invalid;
	}
	if (FightWeight + ShopWeight + EventWeight <= 0.f)
	{
		Context.AddError(LOCTEXT("NoWeights", "Fight, shop and event weights are all 0."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
