// Copyright Brandon Hillig 2026

#include "Presentation/DwAudioCueMap.h"

#include "GameplayTagsManager.h"
#include "Math/RandomStream.h"
#include "Sound/SoundBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "DwAudioCueMap"

const TArray<FDwCueSoundLayer>& FDwCueSound::GetSection(EDwCueSoundSection Section) const
{
	switch (Section)
	{
	case EDwCueSoundSection::Loop: return Loop;
	case EDwCueSoundSection::Stop: return Stop;
	default:                       return Start;
	}
}

const FDwCueSound* UDwAudioCueMap::FindCue(const FGameplayTag& CueTag, FGameplayTag* OutMatchedTag) const
{
	for (FGameplayTag Tag = CueTag; Tag.IsValid(); Tag = Tag.RequestDirectParent())
	{
		if (const FDwCueSound* Found = Cues.Find(Tag))
		{
			if (OutMatchedTag)
			{
				*OutMatchedTag = Tag;
			}
			return Found;
		}
	}
	return nullptr;
}

int32 UDwAudioCueMap::PickVariation(int32 NumVariations, int32 Previous, FRandomStream& Random)
{
	if (NumVariations <= 0)
	{
		return INDEX_NONE;
	}
	if (NumVariations == 1)
	{
		return 0;
	}
	if (Previous < 0 || Previous >= NumVariations)
	{
		return Random.RandRange(0, NumVariations - 1);
	}
	// Pick among the others: draw from N-1 slots and skip over the previous one.
	const int32 Pick = Random.RandRange(0, NumVariations - 2);
	return Pick >= Previous ? Pick + 1 : Pick;
}

#if WITH_EDITOR
EDataValidationResult UDwAudioCueMap::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);
	const FGameplayTag CueRoot = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("GameplayCue"), /*ErrorIfNotFound*/ false);
	for (const TPair<FGameplayTag, FDwCueSound>& Entry : Cues)
	{
		const FText TagText = FText::FromName(Entry.Key.GetTagName());
		if (!Entry.Key.IsValid() || !Entry.Key.MatchesTag(CueRoot))
		{
			Context.AddError(FText::Format(LOCTEXT("NotCue", "{0} is not a GameplayCue.* tag."), TagText));
			continue;
		}
		const TPair<const TCHAR*, EDwCueSoundSection> Sections[] = {
			{ TEXT("Start"), EDwCueSoundSection::Start }, { TEXT("Loop"), EDwCueSoundSection::Loop }, { TEXT("Stop"), EDwCueSoundSection::Stop } };
		for (const TPair<const TCHAR*, EDwCueSoundSection>& Section : Sections)
		{
			const TArray<FDwCueSoundLayer>& Layers = Entry.Value.GetSection(Section.Value);
			for (int32 Index = 0; Index < Layers.Num(); ++Index)
			{
				const FDwCueSoundLayer& Layer = Layers[Index];
				const FText Where = FText::Format(LOCTEXT("Where", "{0} {1} layer {2}"), TagText, FText::FromString(Section.Key), Index);
				if (Layer.Sounds.IsEmpty() || Layer.Sounds.Contains(nullptr))
				{
					Context.AddError(FText::Format(LOCTEXT("NoSound", "{0}: every variation needs a sound."), Where));
				}
				if (Layer.PitchMin > Layer.PitchMax)
				{
					Context.AddError(FText::Format(LOCTEXT("Pitch", "{0}: PitchMin is above PitchMax."), Where));
				}
				if (Section.Value == EDwCueSoundSection::Loop)
				{
					for (const USoundBase* Sound : Layer.Sounds)
					{
						if (Sound && !Sound->IsLooping())
						{
							Context.AddWarning(FText::Format(LOCTEXT("NotLooping", "{0}: {1} does not loop; it will play once."), Where,
								FText::FromString(Sound->GetName())));
						}
					}
				}
			}
		}
		if (Entry.Value.bSpatialized && !Entry.Value.Attenuation)
		{
			Context.AddWarning(FText::Format(LOCTEXT("NoAttenuation", "{0} is spatialized without attenuation (stereo sources stay 2D)."), TagText));
		}
	}
	return Context.GetNumErrors() > 0 ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
