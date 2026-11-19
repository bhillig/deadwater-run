// Copyright Brandon Hillig 2026

#include "UI/DwCombatViewModels.h"

#include "Abilities/DwAbilitySystemComponent.h"
#include "Abilities/DwGameplayAbility.h"
#include "Combat/DwCombatInteraction.h"
#include "Combat/DwCombatRules.h"
#include "Combat/DwCombatSnapshot.h"
#include "Combat/DwCombatSubsystem.h"
#include "Combat/DwCombatUnit.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwKrakenDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "GameplayEffect.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#define LOCTEXT_NAMESPACE "DwCombatHud"

namespace DwCombatHud
{
	/** Placeholder wording carried over from ADwPlaceholderHUD (open creative question: final outcome copy). */
	FText OutcomeText(EDwCombatOutcome Outcome)
	{
		switch (Outcome)
		{
		case EDwCombatOutcome::VictorySunk:         return LOCTEXT("VictorySunk", "VICTORY - the enemy ship is sunk");
		case EDwCombatOutcome::VictoryCrewDefeated: return LOCTEXT("VictoryCrew", "VICTORY - the enemy crew is defeated (plunder!)");
		case EDwCombatOutcome::VictorySurrender:    return LOCTEXT("VictorySurrender", "VICTORY - the enemy surrenders");
		case EDwCombatOutcome::VictoryBoss:         return LOCTEXT("VictoryBoss", "VICTORY - the Kraken is slain");
		case EDwCombatOutcome::DefeatCrewDead:      return LOCTEXT("DefeatCrew", "DEFEAT - your crew is lost");
		case EDwCombatOutcome::DefeatCaptainDead:   return LOCTEXT("DefeatCaptain", "DEFEAT - your captain has fallen");
		case EDwCombatOutcome::DefeatShipSunk:      return LOCTEXT("DefeatShip", "DEFEAT - your ship is sunk");
		default:                                    return FText::GetEmpty();
		}
	}

	/** Status tags the HUD knows a label for, most important first. */
	struct FStatusLabel { FGameplayTag Tag; FText Label; };
	const TArray<FStatusLabel>& KnownStatuses()
	{
		static const TArray<FStatusLabel> Statuses = {
			{ DwTags::Status_Incapacitated_Frozen, LOCTEXT("Frozen", "Frozen") },
			{ DwTags::Status_Incapacitated_Cowering, LOCTEXT("Cowering", "Cowering") },
			{ DwTags::Status_Rooted, LOCTEXT("Rooted", "Rooted") },
			{ DwTags::Status_Afraid, LOCTEXT("Afraid", "Afraid") },
			{ DwTags::Status_Guarded, LOCTEXT("Guarded", "Guarded") },
			{ DwTags::State_Synergy_Adjacent, LOCTEXT("Synergy", "Synergy") } };
		return Statuses;
	}

	FText HitRange(int32 PerHit, int32 MinHits, int32 MaxHits, const FText& Unit, bool bNegative)
	{
		const FText Sign = bNegative ? LOCTEXT("Minus", "-") : LOCTEXT("Plus", "+");
		if (MaxHits > MinHits)
		{
			return FText::Format(LOCTEXT("Range", "{0}{1} to {0}{2} {3}"), Sign, PerHit * MinHits, PerHit * MaxHits, Unit);
		}
		return FText::Format(LOCTEXT("Single", "{0}{1} {2}"), Sign, PerHit * MinHits, Unit);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Unit

bool FDwUnitView::operator==(const FDwUnitView& Other) const
{
	return Handle == Other.Handle && Side == Other.Side && Name.EqualTo(Other.Name) && bIsCaptain == Other.bIsCaptain
		&& bAlive == Other.bAlive && Health == Other.Health && MaxHealth == Other.MaxHealth && Mana == Other.Mana
		&& MaxMana == Other.MaxMana && Fear == Other.Fear && MaxFear == Other.MaxFear && bHasMove == Other.bHasMove
		&& bHasAction == Other.bHasAction && bSelected == Other.bSelected && bHovered == Other.bHovered
		&& bActing == Other.bActing && Statuses == Other.Statuses;
}

void UDwUnitViewModel::Apply(const FDwUnitView& NewView)
{
	if (View == NewView && GetChangeCount() > 0)
	{
		return;
	}
	View = NewView;
	NotifyChanged();
}

void UDwUnitViewModel::SetInteraction(UDwCombatInteraction* InInteraction)
{
	Interaction = InInteraction;
}

void UDwUnitViewModel::Select()
{
	if (UDwCombatInteraction* Target = Interaction.Get())
	{
		Target->SelectUnit(View.Handle);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Action

bool FDwActionView::operator==(const FDwActionView& Other) const
{
	return SlotIndex == Other.SlotIndex && AbilityTag == Other.AbilityTag && ItemId == Other.ItemId && Name.EqualTo(Other.Name)
		&& Description.EqualTo(Other.Description) && Icon == Other.Icon && ManaCost == Other.ManaCost
		&& CostText.EqualTo(Other.CostText) && Count == Other.Count && bEnabled == Other.bEnabled
		&& DisabledReason.EqualTo(Other.DisabledReason) && Hotkey.EqualTo(Other.Hotkey) && bArmed == Other.bArmed;
}

void UDwActionViewModel::Apply(const FDwActionView& NewView)
{
	if (View == NewView && GetChangeCount() > 0)
	{
		return;
	}
	View = NewView;
	NotifyChanged();
}

void UDwActionViewModel::SetInteraction(UDwCombatInteraction* InInteraction)
{
	Interaction = InInteraction;
}

bool UDwActionViewModel::Select()
{
	UDwCombatInteraction* Target = Interaction.Get();
	return Target && Target->SelectSlot(View.SlotIndex);
}

FText UDwActionViewModel::FormatDescription(const UDwAbilityDefinition* Ability, FName ItemId, const FDwCombatSnapshot& Snapshot)
{
	FFormatNamedArguments Args;
	FText Source;
	if (const UDwItemDefinition* Item = ItemId.IsNone() ? nullptr : Snapshot.FindItem(ItemId))
	{
		Source = Item->Description;
		if (const FDwItemTuningRow* Row = Item->GetTuningRow<FDwItemTuningRow>())
		{
			Args.Add(TEXT("Amount"), Row->Amount);
			Args.Add(TEXT("Range"), Row->Range);
		}
	}
	else if (Ability)
	{
		Source = Ability->Description;
		if (const FDwAbilityTuningRow* Row = UDwGameplayAbility::GetTuning(Ability))
		{
			Args.Add(TEXT("Damage"), Row->Damage);
			Args.Add(TEXT("Heal"), Row->Heal);
			Args.Add(TEXT("Fear"), Row->Fear);
			Args.Add(TEXT("Range"), Row->Range);
			Args.Add(TEXT("ManaCost"), Row->ManaCost);
			Args.Add(TEXT("Turns"), Row->DurationTurns);
		}
	}
	return Args.IsEmpty() ? Source : FText::Format(Source, Args);
}

// ---------------------------------------------------------------------------------------------------------------------
// Keys

FText FDwHudKeys::KeyFor(const UInputMappingContext* Context, const UInputAction* Action)
{
	if (Context && Action)
	{
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (Mapping.Action == Action)
			{
				return Mapping.Key.GetDisplayName(/*bLongDisplayName*/ false);
			}
		}
	}
	return FText::GetEmpty();
}

FDwHudKeys FDwHudKeys::FromInput(const FDwInputConfig& Input)
{
	FDwHudKeys Keys;
	for (const UInputAction* Ability : Input.Abilities)
	{
		Keys.Abilities.Add(KeyFor(Input.CombatContext, Ability));
	}
	Keys.EndTurn = KeyFor(Input.CombatContext, Input.EndTurn);
	Keys.Select = KeyFor(Input.CombatContext, Input.Select);
	Keys.Cancel = KeyFor(Input.CombatContext, Input.Cancel);
	Keys.NextUnit = KeyFor(Input.CombatContext, Input.NextUnit);
	return Keys;
}

// ---------------------------------------------------------------------------------------------------------------------
// Combat HUD

void UDwCombatHudViewModel::Initialize(UDwCombatSubsystem& InCombat, UDwCombatInteraction* InInteraction, const FDwHudKeys& InKeys)
{
	Shutdown();
	Combat = &InCombat;
	Interaction = InInteraction;
	Keys = InKeys;
	InCombat.OnCombatEvent.AddDynamic(this, &ThisClass::HandleCombatEvent);
	InCombat.OnCommandResolved.AddDynamic(this, &ThisClass::HandleCommandResolved);
	InCombat.OnCombatEnded.AddDynamic(this, &ThisClass::HandleCombatEnded);
	InCombat.OnEndTurnSuggested.AddDynamic(this, &ThisClass::HandleEndTurnSuggested);
	if (InInteraction)
	{
		InInteraction->OnChanged.AddDynamic(this, &ThisClass::HandleInteractionChanged);
	}
	Refresh();
}

void UDwCombatHudViewModel::Shutdown()
{
	if (UDwCombatSubsystem* Fight = Combat.Get())
	{
		Fight->OnCombatEvent.RemoveAll(this);
		Fight->OnCommandResolved.RemoveAll(this);
		Fight->OnCombatEnded.RemoveAll(this);
		Fight->OnEndTurnSuggested.RemoveAll(this);
	}
	if (UDwCombatInteraction* Input = Interaction.Get())
	{
		Input->OnChanged.RemoveAll(this);
	}
	Combat = nullptr;
	Interaction = nullptr;
	Crew.Reset();
	Enemies.Reset();
	Actions.Reset();
	bEndTurnSuggested = false;
}

UDwUnitViewModel* UDwCombatHudViewModel::FindUnit(FDwUnitHandle Handle) const
{
	for (const TArray<TObjectPtr<UDwUnitViewModel>>* List : { &Crew, &Enemies })
	{
		for (UDwUnitViewModel* Unit : *List)
		{
			if (Unit->GetView().Handle == Handle)
			{
				return Unit;
			}
		}
	}
	return nullptr;
}

void UDwCombatHudViewModel::Refresh()
{
	const UDwCombatSubsystem* Fight = Combat.Get();
	if (!Fight || !Fight->IsActive())
	{
		return;
	}
	const FDwCombatSnapshot Snapshot = Fight->GetSnapshot();
	const UDwCombatInteraction* Input = Interaction.Get();

	Round = Snapshot.Round;
	ActiveSide = Snapshot.ActiveSide;
	Outcome = Snapshot.Outcome;
	OutcomeText = DwCombatHud::OutcomeText(Outcome);
	bCanAct = Input && Input->CanAct();
	TurnText = FText::Format(LOCTEXT("TurnText", "Round {0}  -  {1}"), Round,
		ActiveSide == EDwSide::Player ? LOCTEXT("YourTurn", "YOUR TURN") : LOCTEXT("EnemyTurn", "ENEMY TURN"));

	auto ShipView = [&Snapshot](EDwSide Side)
	{
		FDwShipView View;
		View.Side = Side;
		if (const FDwShipSnapshot* Ship = Snapshot.FindShip(Side))
		{
			View.bPresent = true;
			View.Health = Ship->Health;
			View.MaxHealth = Ship->MaxHealth;
		}
		return View;
	};
	PlayerShip = ShipView(EDwSide::Player);
	EnemyShip = ShipView(EDwSide::Enemy);

	RefreshUnits(Snapshot);
	RefreshBoss(Snapshot);
	RefreshActions(Snapshot);
	RefreshPrompt(Snapshot);
	NotifyChanged();
}

void UDwCombatHudViewModel::RefreshUnits(const FDwCombatSnapshot& Snapshot)
{
	const UDwCombatSubsystem* Fight = Combat.Get();
	const UDwCombatInteraction* Input = Interaction.Get();
	const FDwUnitHandle Selected = Input ? Input->GetSelectedUnit() : FDwUnitHandle();

	// Hovered: the unit under the cursor, plus every unit the previewed action would hit.
	TSet<FDwUnitHandle> Hovered;
	if (Input && Input->GetHoveredTile().IsSet())
	{
		const FDwOccupant Occupant = Snapshot.Grid.GetOccupant(Input->GetHoveredTile().GetValue());
		if (Occupant.IsSet())
		{
			Hovered.Add(Occupant.Unit);
		}
	}
	if (Input && Input->GetPreview().bValid)
	{
		for (const FDwTargetPreview& Hit : Input->GetPreview().Targets)
		{
			if (Hit.Target.IsUnit())
			{
				Hovered.Add(Hit.Target.Unit);
			}
		}
	}

	auto Sync = [&](EDwSide Side, TArray<TObjectPtr<UDwUnitViewModel>>& List)
	{
		int32 Index = 0;
		for (const FDwUnitSnapshot& Unit : Snapshot.Units)
		{
			if (Unit.Side != Side)
			{
				continue;
			}
			if (Unit.Handle == Snapshot.Kraken.Body && Snapshot.Kraken.IsSet())
			{
				continue; // the boss bar shows the body (DW-303); its tentacles stay in the enemy row
			}
			if (!List.IsValidIndex(Index))
			{
				UDwUnitViewModel* Created = NewObject<UDwUnitViewModel>(this);
				Created->SetInteraction(Interaction.Get());
				List.Add(Created);
			}
			const ADwCombatUnit* Actor = Fight ? Fight->FindUnit(Unit.Handle) : nullptr;
			FDwUnitView View;
			View.Handle = Unit.Handle;
			View.Side = Unit.Side;
			View.Name = Actor && Actor->GetDefinition() ? Actor->GetDefinition()->DisplayName : FText::AsNumber(Unit.Handle.Id);
			View.bIsCaptain = Actor && Actor->IsCaptain();
			View.bAlive = Unit.bAlive;
			View.Health = Unit.Health;
			View.MaxHealth = Unit.MaxHealth;
			View.Mana = Unit.Mana;
			View.MaxMana = Unit.MaxMana;
			View.Fear = Unit.Fear;
			View.MaxFear = Unit.MaxFear;
			View.bHasMove = Unit.bAlive && Unit.MovePoints > 0.f;
			View.bHasAction = Unit.bAlive && Unit.ActionPoints > 0.f;
			View.bSelected = Unit.Handle == Selected;
			View.bHovered = Hovered.Contains(Unit.Handle);
			View.bActing = Snapshot.ActingUnit == Unit.Handle;
			View.Statuses = Unit.bAlive ? GatherStatuses(Unit) : TArray<FDwStatusView>();
			List[Index]->Apply(View);
			++Index;
		}
		List.SetNum(Index);
	};
	Sync(EDwSide::Player, Crew);
	Sync(EDwSide::Enemy, Enemies);
}

// ---------------------------------------------------------------------------------------------------------------------
// The Kraken HUD (DW-303)

FDwBossView UDwCombatHudViewModel::BuildBossView(const FDwCombatSnapshot& Snapshot, const FText& BodyName)
{
	FDwBossView View;
	const FDwKrakenSnapshot& Kraken = Snapshot.Kraken;
	const FDwUnitSnapshot* Body = Kraken.IsSet() ? Snapshot.FindUnit(Kraken.Body) : nullptr;
	if (!Body)
	{
		return View;
	}
	View.bPresent = true;
	View.Body = Body->Handle;
	View.Name = BodyName.IsEmpty() ? LOCTEXT("Kraken", "The Kraken") : BodyName;
	View.Health = Body->bAlive ? Body->Health : 0.f;
	View.MaxHealth = Body->MaxHealth;
	View.PhaseIndex = Kraken.PhaseIndex;
	View.bEnraged = Kraken.PhaseIndex > 0;
	View.PhaseText = View.bEnraged ? FText::FromString(Kraken.PhaseId.ToString().ToUpper()) : FText::GetEmpty();
	View.TentaclesTotal = Kraken.TentaclesTotal;
	View.TentaclesAlive = FMath::Max(0, Kraken.TentaclesTotal - Kraken.TentaclesDead);
	View.DamageMultiplier = Kraken.DamageMultiplier;
	const int32 Weaker = FMath::RoundToInt((1.f - Kraken.DamageMultiplier) * 100.f);
	View.TentacleText = Weaker > 0
		? FText::Format(LOCTEXT("TentaclesWeaker", "Tentacles {0}/{1}   -{2}% damage"), View.TentaclesAlive, View.TentaclesTotal, Weaker)
		: FText::Format(LOCTEXT("Tentacles", "Tentacles {0}/{1}"), View.TentaclesAlive, View.TentaclesTotal);
	return View;
}

TArray<FDwIntentView> UDwCombatHudViewModel::BuildIntentViews(const FDwCombatSnapshot& Snapshot, TFunctionRef<FText(FDwUnitHandle)> NameOf)
{
	TArray<FDwIntentView> Views;
	TArray<const FDwUnitIntents*> Owners;
	for (const FDwUnitIntents& Entry : Snapshot.Intents)
	{
		Owners.Add(&Entry);
	}
	Owners.Sort([](const FDwUnitIntents& A, const FDwUnitIntents& B) { return A.Unit.Id < B.Unit.Id; });

	for (const FDwUnitIntents* Entry : Owners)
	{
		const FDwUnitSnapshot* Source = Snapshot.FindUnit(Entry->Unit);
		if (!Source || !Source->bAlive)
		{
			continue;
		}
		for (int32 Index = 0; Index < Entry->Intents.Num(); ++Index)
		{
			const FDwAiIntent& Intent = Entry->Intents[Index];
			const UDwAbilityDefinition* Ability = FDwCombatQueries::FindAbility(*Source, Intent.AbilityTag);
			const UDwGameplayAbility* CDO = FDwCombatQueries::GetAbilityCDO(Ability);
			FDwIntentView& View = Views.AddDefaulted_GetRef();
			View.Order = Index + 1;
			View.Source = Entry->Unit;
			View.AbilityTag = Intent.AbilityTag;
			View.MoveName = Ability ? Ability->DisplayName : FText::FromName(Intent.AbilityTag.GetTagName());
			View.DangerTiles = FDwCombatQueries::GetIntentDangerTiles(Snapshot, Entry->Unit, Intent);
			if (!CDO)
			{
				continue;
			}

			FDwTargetingContext Context;
			Context.Snapshot = &Snapshot;
			Context.Ability = Ability;
			Context.Source = Entry->Unit;
			const FDwTargetingRules& Rules = CDO->GetTargetingRules(Context);
			const FDwAbilityPreview Preview = CDO->PreviewOutcome(Context, Intent.Target);
			const bool bTileAim = !Intent.Target.IsUnit() && !Intent.Target.bShip;

			switch (Rules.Pattern)
			{
			case EDwTargetPattern::Self:          View.TargetText = LOCTEXT("TargetSelf", "Itself"); break;
			case EDwTargetPattern::AllEnemies:    View.TargetText = LOCTEXT("TargetAll", "Every pirate"); break;
			case EDwTargetPattern::RandomEnemies: View.TargetText = LOCTEXT("TargetRandom", "Random pirates"); break;
			default:
				if (Intent.Target.bShip)
				{
					View.TargetText = Intent.Target.ShipSide == EDwSide::Player ? LOCTEXT("TargetOurShip", "Your ship") : LOCTEXT("TargetTheirShip", "Its ship");
				}
				else if (bTileAim)
				{
					View.bDodgeable = Rules.bAimAtTile;
					const FDwOccupant Occupant = Snapshot.Grid.GetOccupant(Intent.Target.Tile);
					View.TargetText = Occupant.IsSet()
						? FText::Format(LOCTEXT("TargetTile", "{0}'s tile"), NameOf(Occupant.Unit))
						: LOCTEXT("TargetEmptyTile", "Empty tile");
				}
				else
				{
					View.TargetText = NameOf(Intent.Target.Unit);
				}
				break;
			}

			int32 Damage = 0;
			for (const FDwTargetPreview& Hit : Preview.Targets)
			{
				Damage = FMath::Max(Damage, Hit.Damage);
				if (Hit.bLethal)
				{
					View.bLethal = true;
					const FDwUnitSnapshot* Victim = Snapshot.FindUnit(Hit.Target.Unit);
					View.bThreatensCaptain |= Victim && Victim->Tags.HasTagExact(DwTags::Unit_Role_Captain);
				}
			}
			View.bWillMiss = View.bDodgeable && Preview.bValid && Preview.Targets.IsEmpty();
			if (View.bWillMiss)
			{
				View.EffectText = LOCTEXT("WillMiss", "Nobody there - it will miss");
			}
			else if (Damage > 0 && Rules.Pattern == EDwTargetPattern::RandomEnemies)
			{
				View.EffectText = FText::Format(LOCTEXT("RandomHits", "{0}-{1} hits of {2}"), Preview.MinHits, Preview.MaxHits, Damage);
			}
			else if (Damage > 0 && Rules.Pattern == EDwTargetPattern::AllEnemies)
			{
				View.EffectText = FText::Format(LOCTEXT("EachHit", "{0} to each pirate"), Damage);
			}
			else if (Damage > 0)
			{
				View.EffectText = FText::Format(LOCTEXT("SingleHit", "{0} damage"), Damage);
			}
			else
			{
				View.EffectText = Ability->Description;
			}
		}
	}
	return Views;
}

void UDwCombatHudViewModel::RefreshBoss(const FDwCombatSnapshot& Snapshot)
{
	const UDwCombatSubsystem* Fight = Combat.Get();
	const UDwKrakenDefinition* Kraken = Fight ? Fight->GetSetup().Kraken.Get() : nullptr;
	Boss = BuildBossView(Snapshot, Kraken ? Kraken->DisplayName : FText::GetEmpty()); // "The Kraken"
	Intents = BuildIntentViews(Snapshot, [this](FDwUnitHandle Handle)
	{
		if (const UDwUnitViewModel* Unit = FindUnit(Handle))
		{
			return Unit->GetView().Name;
		}
		return Handle == Boss.Body ? Boss.Name : LOCTEXT("SomeUnit", "Someone");
	});
}

TArray<FDwStatusView> UDwCombatHudViewModel::GatherStatuses(const FDwUnitSnapshot& Unit) const
{
	TArray<FDwStatusView> Statuses;
	const UDwCombatSubsystem* Fight = Combat.Get();
	const ADwCombatUnit* Actor = Fight ? Fight->FindUnit(Unit.Handle) : nullptr;
	const UDwAbilitySystemComponent* Asc = Actor ? Actor->GetDwAbilitySystem() : nullptr;

	auto Add = [&](const FGameplayTag& Tag, const FText& Label)
	{
		FDwStatusView& Status = Statuses.AddDefaulted_GetRef();
		Status.Tag = Tag;
		Status.Label = Label;
		if (Asc)
		{
			// Turns left = the longest turn duration among the effects granting this status (doc 02 "Turn-based durations").
			const TArray<FActiveGameplayEffectHandle> Effects = Asc->GetActiveEffects(
				FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(Tag)));
			for (const FActiveGameplayEffectHandle& Effect : Effects)
			{
				Status.TurnsRemaining = FMath::Max(Status.TurnsRemaining, Asc->GetTurnsRemaining(Effect));
			}
		}
	};

	TSet<FGameplayTag> Seen;
	for (const DwCombatHud::FStatusLabel& Known : DwCombatHud::KnownStatuses())
	{
		if (Unit.Tags.HasTagExact(Known.Tag))
		{
			Add(Known.Tag, Known.Label);
			Seen.Add(Known.Tag);
		}
	}
	// Statuses added after DW-107 still show up, labelled by their last tag segment, until they get a proper label.
	static const FGameplayTag StatusRoot = FGameplayTag::RequestGameplayTag(TEXT("Status"), /*bErrorIfNotFound*/ false);
	for (const FGameplayTag& Tag : Unit.Tags)
	{
		if (StatusRoot.IsValid() && Tag.MatchesTag(StatusRoot) && Tag != StatusRoot && !Seen.Contains(Tag)
			&& Tag != DwTags::Status_Incapacitated)
		{
			FString Leaf = Tag.ToString();
			Leaf.Split(TEXT("."), nullptr, &Leaf, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			Add(Tag, FText::FromString(Leaf));
		}
	}
	return Statuses;
}

void UDwCombatHudViewModel::RefreshActions(const FDwCombatSnapshot& Snapshot)
{
	const UDwCombatInteraction* Input = Interaction.Get();
	const TArray<FDwActionSlot> Bar = Input ? Input->GetActionBar() : TArray<FDwActionSlot>();
	for (int32 Index = 0; Index < Bar.Num(); ++Index)
	{
		const FDwActionSlot& Slot = Bar[Index];
		if (!Actions.IsValidIndex(Index))
		{
			UDwActionViewModel* Created = NewObject<UDwActionViewModel>(this);
			Created->SetInteraction(Interaction.Get());
			Actions.Add(Created);
		}
		FDwActionView View;
		View.SlotIndex = Index;
		View.AbilityTag = Slot.Ability ? Slot.Ability->AbilityTag : FGameplayTag();
		View.ItemId = Slot.ItemId;
		View.Name = Slot.Label;
		View.Count = Slot.Count;
		View.Description = UDwActionViewModel::FormatDescription(Slot.Ability, Slot.ItemId, Snapshot);
		const UDwItemDefinition* Item = Slot.ItemId.IsNone() ? nullptr : Snapshot.FindItem(Slot.ItemId);
		View.Icon = Item ? Item->Icon : (Slot.Ability ? Slot.Ability->Icon : TSoftObjectPtr<UTexture2D>());

		const FDwAbilityTuningRow* Tuning = Item ? nullptr : UDwGameplayAbility::GetTuning(Slot.Ability);
		const UDwGameplayAbility* CDO = FDwCombatQueries::GetAbilityCDO(Slot.Ability);
		View.ManaCost = Tuning ? Tuning->ManaCost : 0;
		if (View.ManaCost > 0)
		{
			View.CostText = FText::Format(LOCTEXT("ManaCost", "{0} MP"), View.ManaCost);
		}
		else if (CDO && CDO->ActionCost == EDwActionCost::Free)
		{
			View.CostText = LOCTEXT("Free", "Free");
		}

		// Rules (same checks as SubmitCommand) decide availability and the reason; "may the player act right now"
		// (their turn, nothing resolving) only locks the bar and never invents a reason.
		View.DisabledReason = Slot.bAvailable ? FText::GetEmpty() : Slot.Reason;
		View.bEnabled = Slot.bAvailable && Input->CanAct();
		View.Hotkey = Keys.Abilities.IsValidIndex(Index) ? Keys.Abilities[Index] : FText::GetEmpty();
		View.bArmed = Input->GetMode() == EDwInteractionMode::Targeting && Input->GetArmedAbility() == Slot.Ability
			&& Input->GetArmedItem() == Slot.ItemId;
		Actions[Index]->Apply(View);
	}
	Actions.SetNum(Bar.Num());
}

FText UDwCombatHudViewModel::NameOf(const FDwAbilityTarget& Target, const FDwCombatSnapshot& Snapshot) const
{
	if (Target.bShip)
	{
		return Target.ShipSide == EDwSide::Player ? LOCTEXT("YourShip", "Your ship") : LOCTEXT("EnemyShip", "Enemy ship");
	}
	if (const UDwUnitViewModel* Unit = FindUnit(Target.Unit))
	{
		return Unit->GetView().Name;
	}
	return LOCTEXT("Tile", "Tile");
}

void UDwCombatHudViewModel::RefreshPrompt(const FDwCombatSnapshot& Snapshot)
{
	const UDwCombatInteraction* Input = Interaction.Get();
	Hint = FText::GetEmpty();
	Message = Input ? Input->GetMessage() : FText::GetEmpty();
	Preview.Reset();
	if (!Input || Outcome != EDwCombatOutcome::None)
	{
		Message = FText::GetEmpty();
		return;
	}

	if (Input->GetMode() == EDwInteractionMode::Targeting && Input->GetArmedAbility())
	{
		const FText Name = Input->GetArmedItem().IsNone() || !Snapshot.FindItem(Input->GetArmedItem())
			? Input->GetArmedAbility()->DisplayName : Snapshot.FindItem(Input->GetArmedItem())->DisplayName;
		Hint = FText::Format(LOCTEXT("Aiming", "Aiming {0} - click a highlighted target ({1} to cancel)"), Name,
			Keys.Cancel.IsEmpty() ? LOCTEXT("CancelFallback", "right-click") : Keys.Cancel);
	}
	else if (ActiveSide == EDwSide::Player && !Input->GetSelectedUnit().IsValid())
	{
		Hint = LOCTEXT("PickPirate", "Click one of your pirates.");
	}

	const FDwAbilityPreview& Data = Input->GetPreview();
	if (Data.bValid)
	{
		for (const FDwTargetPreview& Hit : Data.Targets)
		{
			TArray<FText> Parts;
			if (Hit.Damage > 0) { Parts.Add(DwCombatHud::HitRange(Hit.Damage, Data.MinHits, Data.MaxHits, LOCTEXT("HP", "HP"), true)); }
			if (Hit.Heal > 0)   { Parts.Add(DwCombatHud::HitRange(Hit.Heal, 1, 1, LOCTEXT("HP", "HP"), false)); }
			if (Hit.Mana > 0)   { Parts.Add(DwCombatHud::HitRange(Hit.Mana, 1, 1, LOCTEXT("MP", "MP"), false)); }
			if (Hit.Fear > 0)   { Parts.Add(DwCombatHud::HitRange(Hit.Fear, 1, 1, LOCTEXT("Fear", "Fear"), false)); }
			FDwPreviewLine& Line = Preview.AddDefaulted_GetRef();
			Line.Target = NameOf(Hit.Target, Snapshot);
			Line.Effect = FText::Join(LOCTEXT("PartSeparator", ", "), Parts);
			Line.bLethal = Hit.bLethal;
		}
	}
	else if (!Data.Reason.IsEmpty() && Message.IsEmpty())
	{
		Message = Data.Reason;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Intents

void UDwCombatHudViewModel::SelectUnit(FDwUnitHandle Handle)
{
	if (UDwCombatInteraction* Input = Interaction.Get())
	{
		Input->SelectUnit(Handle);
	}
}

bool UDwCombatHudViewModel::SelectAction(int32 SlotIndex)
{
	UDwCombatInteraction* Input = Interaction.Get();
	return Input && Input->SelectSlot(SlotIndex);
}

void UDwCombatHudViewModel::EndTurn()
{
	if (UDwCombatInteraction* Input = Interaction.Get())
	{
		Input->RequestEndTurn();
	}
}

bool UDwCombatHudViewModel::Cancel()
{
	UDwCombatInteraction* Input = Interaction.Get();
	return Input && Input->Cancel();
}

// ---------------------------------------------------------------------------------------------------------------------
// Game events

void UDwCombatHudViewModel::HandleCombatEvent(const FDwCombatEvent& Event)
{
	if (Event.Type == EDwCombatEventType::TurnStarted)
	{
		bEndTurnSuggested = false; // doc 14: cleared on turn change
		Refresh();
		OnTurnBanner.Broadcast(Event.Side, FMath::RoundToInt(Event.Magnitude));
		return;
	}
	Refresh();
	if (Event.Type == EDwCombatEventType::PhaseChanged && Boss.bPresent)
	{
		// B7: the banner, e.g. "The Kraken is ENRAGED!" (doc 14).
		OnPhaseBanner.Broadcast(FText::Format(LOCTEXT("PhaseBanner", "{0} is {1}!"), Boss.Name, Boss.PhaseText), Boss.PhaseIndex);
	}
}

void UDwCombatHudViewModel::HandleCommandResolved(const FDwCombatCommand& /*Command*/)
{
	// Doc 14: cleared on the next command. The subsystem broadcasts OnEndTurnSuggested after OnCommandResolved, so a
	// command that spends the last action clears and then re-raises it in the same call.
	bEndTurnSuggested = false;
	Refresh();
}

void UDwCombatHudViewModel::HandleCombatEnded(const FDwCombatResult& /*Result*/)
{
	bEndTurnSuggested = false;
	Refresh();
}

void UDwCombatHudViewModel::HandleEndTurnSuggested()
{
	bEndTurnSuggested = true;
	Refresh();
}

void UDwCombatHudViewModel::HandleInteractionChanged()
{
	Refresh();
}

#undef LOCTEXT_NAMESPACE
