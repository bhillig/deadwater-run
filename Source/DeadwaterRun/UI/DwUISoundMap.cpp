// Copyright Brandon Hillig 2026

#include "UI/DwUISoundMap.h"

#include "Sound/SoundBase.h"

USoundBase* UDwUISoundMap::FindSound(const FGameplayTag& Tag) const
{
	const TSoftObjectPtr<USoundBase>* Sound = Sounds.Find(Tag);
	return Sound ? Sound->LoadSynchronous() : nullptr;
}
