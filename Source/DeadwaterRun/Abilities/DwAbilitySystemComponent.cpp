// Copyright Brandon Hillig 2026

#include "Abilities/DwAbilitySystemComponent.h"

#include "Abilities/DwGameplayAbility.h"
#include "Abilities/DwTurnDurationComponent.h"
#include "Core/DwGameplayTags.h"
#include "Core/DwLog.h"
#include "Data/DwAbilityDefinition.h"

UDwAbilitySystemComponent::UDwAbilitySystemComponent()
{
	SetIsReplicatedByDefault(false); // single-player
}

void UDwAbilitySystemComponent::InitializeComponent()
{
	Super::InitializeComponent();
	// Bound here, not in BeginPlay: spawn setup applies effects before the fight begins play.
	BindEffectDelegate();
}

void UDwAbilitySystemComponent::BindEffectDelegate()
{
	if (!bEffectDelegateBound)
	{
		OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(this, &ThisClass::HandleEffectAdded);
		bEffectDelegateBound = true;
	}
}

FGameplayAbilitySpecHandle UDwAbilitySystemComponent::GrantAbility(const UDwAbilityDefinition* Definition, int32 Level)
{
	BindEffectDelegate(); // grants can happen before BeginPlay (spawn setup, tests)
	if (!Definition || !Definition->AbilityClass)
	{
		UE_LOG(LogDwGas, Error, TEXT("GrantAbility: %s has no AbilityClass."), *GetNameSafe(Definition));
		return FGameplayAbilitySpecHandle();
	}
	FGameplayAbilitySpec Spec(Definition->AbilityClass, Level, INDEX_NONE, const_cast<UDwAbilityDefinition*>(Definition));
	return GiveAbility(Spec);
}

FGameplayAbilitySpec* UDwAbilitySystemComponent::FindSpecByAbilityTag(const FGameplayTag& AbilityTag)
{
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		const UDwAbilityDefinition* Definition = Cast<UDwAbilityDefinition>(Spec.SourceObject.Get());
		if (Definition && Definition->AbilityTag == AbilityTag)
		{
			return &Spec;
		}
	}
	return nullptr;
}

void UDwAbilitySystemComponent::HandleEffectAdded(UAbilitySystemComponent* /*Target*/, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
{
	const UDwTurnDurationComponent* Duration = Spec.Def ? Spec.Def->FindComponent<UDwTurnDurationComponent>() : nullptr;
	if (!Duration)
	{
		return;
	}
	FTurnDuration Entry;
	const float Override = Spec.GetSetByCallerMagnitude(DwTags::Data_Turns, /*WarnIfNotFound*/ false, /*Default*/ -1.f);
	Entry.TurnsRemaining = Override > 0.f ? FMath::RoundToInt(Override) : Duration->Turns;
	Entry.TickPoint = Duration->TickPoint;
	Entry.AppliedDuringOwnerTurn = bOwnerSideTurnActive ? OwnerSideTurnIndex : INDEX_NONE;
	TurnDurations.Add(Handle, Entry);
}

void UDwAbilitySystemComponent::HandleTurnBoundary(EDwTurnTickPoint TickPoint)
{
	BindEffectDelegate();
	if (TickPoint == EDwTurnTickPoint::OwnerSideTurnStart)
	{
		++OwnerSideTurnIndex;
		bOwnerSideTurnActive = true;
	}

	TArray<FActiveGameplayEffectHandle> Expired;
	for (TPair<FActiveGameplayEffectHandle, FTurnDuration>& Pair : TurnDurations)
	{
		FTurnDuration& Entry = Pair.Value;
		if (Entry.TickPoint != TickPoint)
		{
			continue;
		}
		// An effect applied during this very owner turn must survive that turn's end (Eyes of the Sea, self buffs).
		if (TickPoint == EDwTurnTickPoint::OwnerSideTurnEnd && Entry.AppliedDuringOwnerTurn == OwnerSideTurnIndex)
		{
			continue;
		}
		if (--Entry.TurnsRemaining <= 0)
		{
			Expired.Add(Pair.Key);
		}
	}

	for (const FActiveGameplayEffectHandle& Handle : Expired)
	{
		TurnDurations.Remove(Handle);
		RemoveActiveGameplayEffect(Handle);
	}

	if (TickPoint == EDwTurnTickPoint::OwnerSideTurnEnd)
	{
		bOwnerSideTurnActive = false;
	}

	// Effects removed early (cleanse, death) leave stale entries; drop them.
	for (auto It = TurnDurations.CreateIterator(); It; ++It)
	{
		if (!GetActiveGameplayEffect(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
}

int32 UDwAbilitySystemComponent::GetTurnsRemaining(FActiveGameplayEffectHandle Handle) const
{
	const FTurnDuration* Entry = TurnDurations.Find(Handle);
	return Entry && GetActiveGameplayEffect(Handle) ? Entry->TurnsRemaining : INDEX_NONE;
}

void UDwAbilitySystemComponent::NotifyHealthDepleted(AActor* Instigator)
{
	if (bHealthDepletedNotified)
	{
		return;
	}
	bHealthDepletedNotified = true;
	AddLooseGameplayTag(DwTags::State_Dead);
	OnHealthDepleted.Broadcast(this, Instigator);
}

void UDwAbilitySystemComponent::NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled)
{
	Super::NotifyAbilityEnded(Handle, Ability, bWasCancelled);
	OnDwAbilityEnded.Broadcast(this, Handle);
}
