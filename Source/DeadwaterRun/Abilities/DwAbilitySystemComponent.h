// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Core/DwTypes.h"
#include "DwAbilitySystemComponent.generated.h"

class UDwAbilityDefinition;

DECLARE_MULTICAST_DELEGATE_TwoParams(FDwOnHealthDepleted, UDwAbilitySystemComponent* /*Victim*/, AActor* /*Instigator*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FDwOnDwAbilityEnded, UDwAbilitySystemComponent* /*Owner*/, FGameplayAbilitySpecHandle /*Ability*/);

/**
 * Deadwater ASC (Docs/Architecture/02-GAS-Foundation.md). Lives on ADwCombatUnit and ADwShip.
 * - Grants abilities from definitions (the definition is the spec's SourceObject).
 * - Counts turn-duration effects down at turn boundaries; never ticks.
 * - Reports Health depletion once and every ability end, so the combat subsystem can resolve commands.
 */
UCLASS(ClassGroup = Deadwater, meta = (BlueprintSpawnableComponent))
class DEADWATERRUN_API UDwAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UDwAbilitySystemComponent();

	/** Grants Definition->AbilityClass with the definition as SourceObject. Returns an invalid handle if it has no class. */
	FGameplayAbilitySpecHandle GrantAbility(const UDwAbilityDefinition* Definition, int32 Level = 1);

	/** The granted spec whose definition has this AbilityTag (commands identify abilities by tag). */
	FGameplayAbilitySpec* FindSpecByAbilityTag(const FGameplayTag& AbilityTag);

	/**
	 * Called by the turn manager on every ASC of the side whose turn starts or ends.
	 * Start: increments the owner side-turn index, then counts down OwnerSideTurnStart effects.
	 * End: counts down OwnerSideTurnEnd effects, except those applied during this same owner turn (Eyes of the Sea rule).
	 */
	void HandleTurnBoundary(EDwTurnTickPoint TickPoint);

	/** Turns left on a turn-duration effect, or INDEX_NONE if the handle has no turn duration. */
	int32 GetTurnsRemaining(FActiveGameplayEffectHandle Handle) const;

	bool IsOwnerSideTurnActive() const { return bOwnerSideTurnActive; }

	/** Called by the attribute sets when Health reaches 0. Adds State.Dead and broadcasts once per life. */
	void NotifyHealthDepleted(AActor* Instigator);

	/** Instigator of the most recent damage or heal (attribute-change listeners can't see it: Health is set from a meta attribute). */
	AActor* GetLastHealthInstigator() const { return LastHealthInstigator.Get(); }
	void SetLastHealthInstigator(AActor* Instigator) { LastHealthInstigator = Instigator; }

	FDwOnHealthDepleted OnHealthDepleted;
	FDwOnDwAbilityEnded OnDwAbilityEnded;

protected:
	virtual void InitializeComponent() override;
	virtual void NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled) override;

private:
	struct FTurnDuration
	{
		int32 TurnsRemaining = 0;
		EDwTurnTickPoint TickPoint = EDwTurnTickPoint::OwnerSideTurnEnd;
		/** Owner side-turn index at application if applied during the owner's own turn, else INDEX_NONE. */
		int32 AppliedDuringOwnerTurn = INDEX_NONE;
	};

	void HandleEffectAdded(UAbilitySystemComponent* Target, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);
	void BindEffectDelegate();

	TMap<FActiveGameplayEffectHandle, FTurnDuration> TurnDurations;
	int32 OwnerSideTurnIndex = 0;
	bool bOwnerSideTurnActive = false;
	bool bHealthDepletedNotified = false;
	bool bEffectDelegateBound = false;
	TWeakObjectPtr<AActor> LastHealthInstigator;
};
