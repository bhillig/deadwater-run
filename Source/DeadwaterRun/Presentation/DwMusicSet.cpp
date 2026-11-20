// Copyright Brandon Hillig 2026

#include "Presentation/DwMusicSet.h"

#include "Sound/SoundBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "DwMusicSet"

FGameplayTag UDwMusicSet::GetPhaseState(EDwRunPhase Phase) const
{
	const FGameplayTag* State = PhaseStates.Find(Phase);
	return State ? *State : FGameplayTag();
}

#if WITH_EDITOR
EDataValidationResult UDwMusicSet::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);
	for (const TPair<FGameplayTag, FDwMusicTrack>& Entry : Tracks)
	{
		const FText TagText = FText::FromName(Entry.Key.GetTagName());
		if (Entry.Value.Sound.IsNull())
		{
			Context.AddError(FText::Format(LOCTEXT("NoSound", "{0} has no Sound (leave the state out for silence)."), TagText));
		}
		if (!Entry.Value.bLoop && !Entry.Value.Intro.IsNull())
		{
			Context.AddWarning(FText::Format(LOCTEXT("StingerIntro", "{0} is a stinger; its Intro is never played."), TagText));
		}
	}
	if (PhaseStates.Contains(EDwRunPhase::Combat))
	{
		Context.AddWarning(LOCTEXT("CombatPhase", "PhaseStates.Combat is ignored: fights choose Music.Combat or Music.Boss themselves."));
	}
	return Context.GetNumErrors() > 0 ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
