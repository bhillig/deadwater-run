// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DwUISoundMap.generated.h"

class USoundBase;

/**
 * UI.Sound.* tag -> sound (Docs/Architecture/14-UI.md "Juice hooks"). The class is ui-engineer's; the contents of
 * DA_UISoundMap are ui-juice-engineer's, with sounds supplied by sound-wrangler. Empty until DW-402.
 * UI sounds are never GameplayCues (Docs/Architecture/15-Audio-VFX.md).
 */
UCLASS(BlueprintType)
class DEADWATERRUN_API UDwUISoundMap : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound", meta = (Categories = "UI.Sound"))
	TMap<FGameplayTag, TSoftObjectPtr<USoundBase>> Sounds;

	/** The sound for Tag (loaded on first use), or nullptr if none is mapped. */
	USoundBase* FindSound(const FGameplayTag& Tag) const;
};
