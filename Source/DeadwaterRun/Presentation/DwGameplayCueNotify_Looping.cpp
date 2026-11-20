// Copyright Brandon Hillig 2026

#include "Presentation/DwGameplayCueNotify_Looping.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Presentation/DwGameplayCueNotify_Burst.h"
#include "Presentation/DwPresentationSubsystem.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "DwGameplayCueNotify"

bool ADwGameplayCueNotify_Looping::OnActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)
{
	if (!UDwPresentationSubsystem::ShouldPresent(GetWorld()))
	{
		return false;
	}
	if (UDwPresentationSubsystem* Presentation = GetWorld()->GetSubsystem<UDwPresentationSubsystem>())
	{
		Presentation->NoteCuePresented();
	}
	Super::OnActive_Implementation(Target, Parameters);
	PlaySection(Target, Parameters, EDwCueSoundSection::Start);
	return false;
}

bool ADwGameplayCueNotify_Looping::WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)
{
	if (!UDwPresentationSubsystem::ShouldPresent(GetWorld()))
	{
		return false;
	}
	Super::WhileActive_Implementation(Target, Parameters);
	if (!bLoopStarted)
	{
		bLoopStarted = true;
		PlaySection(Target, Parameters, EDwCueSoundSection::Loop);
	}
	return false;
}

bool ADwGameplayCueNotify_Looping::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)
{
	// Recurring effects only (a periodic status tick); no sound section of its own.
	return UDwPresentationSubsystem::ShouldPresent(GetWorld()) ? Super::OnExecute_Implementation(Target, Parameters) : false;
}

bool ADwGameplayCueNotify_Looping::OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)
{
	StopLoopSounds();
	if (!UDwPresentationSubsystem::ShouldPresent(GetWorld()))
	{
		// No target: the engine removes the looping effects and skips the removal ones.
		return Super::OnRemove_Implementation(nullptr, Parameters);
	}
	Super::OnRemove_Implementation(Target, Parameters);
	if (Target)
	{
		PlaySection(Target, Parameters, EDwCueSoundSection::Stop);
	}
	return false;
}

void ADwGameplayCueNotify_Looping::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLoopSounds();
	Super::EndPlay(EndPlayReason);
}

bool ADwGameplayCueNotify_Looping::Recycle()
{
	StopLoopSounds();
	return Super::Recycle();
}

void ADwGameplayCueNotify_Looping::PlaySection(AActor* Target, const FGameplayCueParameters& Parameters, EDwCueSoundSection Section)
{
	UWorld* World = GetWorld();
	UDwPresentationSubsystem* Presentation = World ? World->GetSubsystem<UDwPresentationSubsystem>() : nullptr;
	if (!Presentation || !Target)
	{
		return;
	}
	const FVector Location = Parameters.Location.IsZero() ? Target->GetActorLocation() : FVector(Parameters.Location);
	Presentation->PlayCueSounds(UDwGameplayCueNotify_Burst::GetSoundTag(GameplayCueTag, Parameters), Section, Location,
		Target->GetRootComponent(), Section == EDwCueSoundSection::Loop ? &LoopSounds : nullptr);
}

void ADwGameplayCueNotify_Looping::StopLoopSounds()
{
	for (UAudioComponent* Sound : LoopSounds)
	{
		if (IsValid(Sound))
		{
			Sound->FadeOut(LoopFadeOutSeconds, 0.f); // auto-destroys when the fade ends
		}
	}
	LoopSounds.Reset();
	bLoopStarted = false;
}

#if WITH_EDITOR
EDataValidationResult ADwGameplayCueNotify_Looping::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);
	const UScriptStruct& Burst = *FGameplayCueNotify_BurstEffects::StaticStruct();
	const int32 AssetSounds = UDwGameplayCueNotify_Burst::GetEffectsArrayNum(Burst, &ApplicationEffects, TEXT("BurstSounds"))
		+ UDwGameplayCueNotify_Burst::GetEffectsArrayNum(*FGameplayCueNotify_LoopingEffects::StaticStruct(), &LoopingEffects, TEXT("LoopingSounds"))
		+ UDwGameplayCueNotify_Burst::GetEffectsArrayNum(Burst, &RecurringEffects, TEXT("BurstSounds"))
		+ UDwGameplayCueNotify_Burst::GetEffectsArrayNum(Burst, &RemovalEffects, TEXT("BurstSounds"));
	if (AssetSounds > 0)
	{
		Context.AddError(LOCTEXT("LoopSounds", "The effect sound lists must be empty: cue sounds come from DA_AudioCueMap (15-Audio-VFX D1)."));
	}
	if (!GameplayCueTag.IsValid())
	{
		Context.AddError(LOCTEXT("NoTag", "GameplayCueTag is not set."));
	}
	return Context.GetNumErrors() > 0 ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
