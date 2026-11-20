// Copyright Brandon Hillig 2026

#include "Presentation/DwGameplayCueNotify_Burst.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "Presentation/DwPresentationSubsystem.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "DwGameplayCueNotify"

namespace DwCueNotify
{
	const FArrayProperty* FindArray(const UScriptStruct& Struct, FName Member)
	{
		return CastField<FArrayProperty>(Struct.FindPropertyByName(Member));
	}
}

int32 UDwGameplayCueNotify_Burst::GetEffectsArrayNum(const UScriptStruct& Struct, const void* Effects, FName Member)
{
	const FArrayProperty* Array = DwCueNotify::FindArray(Struct, Member);
	return Array && Effects ? FScriptArrayHelper(Array, Array->ContainerPtrToValuePtr<void>(Effects)).Num() : 0;
}

TArray<UNiagaraSystem*> UDwGameplayCueNotify_Burst::GetBurstNiagaraSystems() const
{
	TArray<UNiagaraSystem*> Systems;
	const FArrayProperty* Array = DwCueNotify::FindArray(*FGameplayCueNotify_BurstEffects::StaticStruct(), TEXT("BurstParticles"));
	if (!Array)
	{
		return Systems;
	}
	FScriptArrayHelper Helper(Array, Array->ContainerPtrToValuePtr<void>(&BurstEffects));
	for (int32 Index = 0; Index < Helper.Num(); ++Index)
	{
		Systems.Add(reinterpret_cast<const FGameplayCueNotify_ParticleInfo*>(Helper.GetRawPtr(Index))->NiagaraSystem);
	}
	return Systems;
}

int32 UDwGameplayCueNotify_Burst::GetNumAssetSounds() const
{
	return GetEffectsArrayNum(*FGameplayCueNotify_BurstEffects::StaticStruct(), &BurstEffects, TEXT("BurstSounds"));
}

#if WITH_EDITOR
void UDwGameplayCueNotify_Burst::SetBurstNiagaraSystems(const TArray<UNiagaraSystem*>& Systems)
{
	const FArrayProperty* Array = DwCueNotify::FindArray(*FGameplayCueNotify_BurstEffects::StaticStruct(), TEXT("BurstParticles"));
	if (!ensure(Array))
	{
		return;
	}
	FScriptArrayHelper Helper(Array, Array->ContainerPtrToValuePtr<void>(&BurstEffects));
	Helper.EmptyAndAddValues(Systems.Num());
	for (int32 Index = 0; Index < Systems.Num(); ++Index)
	{
		reinterpret_cast<FGameplayCueNotify_ParticleInfo*>(Helper.GetRawPtr(Index))->NiagaraSystem = Systems[Index];
	}
}
#endif

FGameplayTag UDwGameplayCueNotify_Burst::GetSoundTag(const FGameplayTag& NotifyTag, const FGameplayCueParameters& Parameters)
{
	return Parameters.OriginalTag.IsValid() ? Parameters.OriginalTag : NotifyTag;
}

FVector UDwGameplayCueNotify_Burst::GetSoundLocation(const FGameplayCueNotify_SpawnContext& SpawnContext,
	const FGameplayCueNotify_PlacementInfo& Placement, const AActor& Target)
{
	FTransform Transform;
	return Placement.FindSpawnTransform(SpawnContext, Transform) ? Transform.GetLocation() : Target.GetActorLocation();
}

bool UDwGameplayCueNotify_Burst::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	// The engine's Burst plus the cap and the cue-map sounds. Instant mode: no components, no sounds.
	UWorld* World = Target ? Target->GetWorld() : nullptr;
	if (!UDwPresentationSubsystem::ShouldPresent(World))
	{
		return false;
	}
	FGameplayCueNotify_SpawnContext SpawnContext(World, Target, Parameters);
	SpawnContext.SetDefaultSpawnCondition(&DefaultSpawnCondition);
	SpawnContext.SetDefaultPlacementInfo(&DefaultPlacementInfo);
	if (!DefaultSpawnCondition.ShouldSpawn(SpawnContext))
	{
		return false;
	}

	UDwPresentationSubsystem* Presentation = World->GetSubsystem<UDwPresentationSubsystem>();
	if (Presentation)
	{
		Presentation->NoteCuePresented();
	}
	FGameplayCueNotify_SpawnResult SpawnResult;
	BurstEffects.ExecuteEffects(SpawnContext, SpawnResult);
	if (Presentation && MaxEffectSeconds > 0.f)
	{
		for (UFXSystemComponent* Effect : SpawnResult.FxSystemComponents)
		{
			Presentation->CapEffectLifetime(Cast<UNiagaraComponent>(Effect), MaxEffectSeconds);
		}
	}
	OnBurst(Target, Parameters, SpawnResult);

	if (Presentation)
	{
		Presentation->PlayCueSounds(GetSoundTag(GameplayCueTag, Parameters), EDwCueSoundSection::Start,
			GetSoundLocation(SpawnContext, DefaultPlacementInfo, *Target));
	}
	return false;
}

#if WITH_EDITOR
EDataValidationResult UDwGameplayCueNotify_Burst::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);
	if (GetNumAssetSounds() > 0)
	{
		Context.AddError(LOCTEXT("BurstSounds", "BurstSounds must be empty: cue sounds come from DA_AudioCueMap (15-Audio-VFX D1)."));
	}
	if (!GameplayCueTag.IsValid())
	{
		Context.AddError(LOCTEXT("NoTag", "GameplayCueTag is not set."));
	}
	return Context.GetNumErrors() > 0 ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
