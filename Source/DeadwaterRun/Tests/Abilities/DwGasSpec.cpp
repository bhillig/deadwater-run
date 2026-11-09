// Copyright Brandon Hillig 2026

// DW-101: GAS integration in a transient world (Docs/Architecture/02 "How it's tested").

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/DwAbilitySystemComponent.h"
#include "Abilities/DwAttributeSets.h"
#include "Abilities/DwGameplayEffects.h"
#include "Abilities/DwTargeting.h"
#include "Board/DwBoardBuilder.h"
#include "Combat/DwCombatSnapshot.h"
#include "Combat/DwCombatUnit.h"
#include "Combat/DwShip.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Tests/DwTestAbilities.h"

using FUnitSet = UDwUnitAttributeSet;

BEGIN_DEFINE_SPEC(FDwGasSpec, "DeadwaterRun.Gas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	UWorld* World = nullptr;
	ADwCombatUnit* Player = nullptr;  // handle 1, tile (0,0)
	ADwCombatUnit* Ally = nullptr;    // handle 2, tile (0,1)
	ADwCombatUnit* Enemy = nullptr;   // handle 3, tile (1,0)
	ADwShip* EnemyShip = nullptr;
	UDataTable* AbilityTable = nullptr;
	UDwAbilityDefinition* Strike = nullptr;

	UDwAbilitySystemComponent* Asc(ADwCombatUnit* Unit) const { return Unit->GetDwAbilitySystem(); }

	float Get(ADwCombatUnit* Unit, const FGameplayAttribute& Attribute) const { return Asc(Unit)->GetNumericAttribute(Attribute); }

	void InitUnit(ADwCombatUnit* Unit, float Attack = 10.f, float Bonus = 0.f, float Reduction = 0.f)
	{
		FDwUnitInitValues V;
		V.MaxHealth = 100; V.Health = 100; V.MaxMana = 20; V.Mana = 20;
		V.AttackDamage = Attack; V.DamageBonus = Bonus; V.DamageReduction = Reduction;
		V.MoveRange = 5; V.MovePoints = 5; V.MaxActionPoints = 1; V.ActionPoints = 1; V.MaxFear = 3; V.Fear = 0;
		FGameplayEffectSpecHandle Spec = Asc(Unit)->MakeOutgoingSpec(DwGas::GetSharedEffects().InitUnitStats, 1, Asc(Unit)->MakeEffectContext());
		UDwGE_InitUnitStats::SetValues(*Spec.Data, V);
		Asc(Unit)->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}

	/** Applies EffectClass from Source to Target with optional SetByCallers. */
	FActiveGameplayEffectHandle Apply(AActor* Source, UDwAbilitySystemComponent* Target, TSubclassOf<UGameplayEffect> EffectClass,
		TArray<TPair<FGameplayTag, float>> SetByCallers = {})
	{
		UDwAbilitySystemComponent* SourceAsc = Cast<IAbilitySystemInterface>(Source)
			? Cast<UDwAbilitySystemComponent>(Cast<IAbilitySystemInterface>(Source)->GetAbilitySystemComponent()) : Target;
		FGameplayEffectContextHandle Context = SourceAsc->MakeEffectContext();
		Context.AddInstigator(Source, Source);
		FGameplayEffectSpecHandle Spec = SourceAsc->MakeOutgoingSpec(EffectClass, 1, Context);
		for (const TPair<FGameplayTag, float>& Pair : SetByCallers)
		{
			Spec.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
		}
		return SourceAsc->ApplyGameplayEffectSpecToTarget(*Spec.Data, Target);
	}

	FDwUnitSnapshot Snap(ADwCombatUnit* Unit, int32 Id, EDwSide Side, FIntPoint Tile) const
	{
		FDwUnitSnapshot S;
		S.Handle = FDwUnitHandle(Id);
		S.Side = Side;
		S.Tile = Tile;
		S.Health = Get(Unit, FUnitSet::GetHealthAttribute());
		S.MaxHealth = Get(Unit, FUnitSet::GetMaxHealthAttribute());
		S.Mana = Get(Unit, FUnitSet::GetManaAttribute());
		S.AttackDamage = Get(Unit, FUnitSet::GetAttackDamageAttribute());
		S.DamageBonus = Get(Unit, FUnitSet::GetDamageBonusAttribute());
		S.DamageReduction = Get(Unit, FUnitSet::GetDamageReductionAttribute());
		S.ActionPoints = Get(Unit, FUnitSet::GetActionPointsAttribute());
		S.MovePoints = Get(Unit, FUnitSet::GetMovePointsAttribute());
		Asc(Unit)->GetOwnedGameplayTags(S.Tags);
		S.bAlive = !S.Tags.HasTagExact(DwTags::State_Dead);
		return S;
	}

	bool Activate(ADwCombatUnit* Source, ADwCombatUnit* Target, float DamageMultiplier = 0.f)
	{
		FGameplayAbilitySpec* Spec = Asc(Source)->FindSpecByAbilityTag(DwTags::Ability_Attack_Melee);
		if (!Spec)
		{
			return false;
		}
		FGameplayEventData Payload;
		Payload.EventTag = DwTags::Event_Ability_Activate;
		Payload.Instigator = Source;
		Payload.Target = Target;
		Payload.EventMagnitude = DamageMultiplier;
		Payload.TargetData = FDwGameplayAbilityTargetData_Grid::MakeHandle(FDwAbilityTarget::MakeUnit(FDwUnitHandle(3), FIntPoint(1, 0)));
		return Asc(Source)->TriggerAbilityFromGameplayEvent(Spec->Handle, Asc(Source)->AbilityActorInfo.Get(), DwTags::Event_Ability_Activate, &Payload, *Asc(Source));
	}

END_DEFINE_SPEC(FDwGasSpec)

void FDwGasSpec::Define()
{
	BeforeEach([this]()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		// Actors only run InitializeComponents (ASC registers its attribute sets) in a world initialized for play.
		World->InitializeActorsForPlay(FURL());
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Player = World->SpawnActor<ADwCombatUnit>(Params);
		Ally = World->SpawnActor<ADwCombatUnit>(Params);
		Enemy = World->SpawnActor<ADwCombatUnit>(Params);
		EnemyShip = World->SpawnActor<ADwShip>(Params);
		InitUnit(Player);
		InitUnit(Ally);
		InitUnit(Enemy);

		// A melee strike: 20 flat + 0.5 x AttackDamage, 10 MP, range 1.
		AbilityTable = NewObject<UDataTable>(GetTransientPackage());
		AbilityTable->RowStruct = FDwAbilityTuningRow::StaticStruct();
		FDwAbilityTuningRow Row;
		Row.ManaCost = 10; Row.Damage = 20; Row.DamageScale = 0.5f; Row.Range = 1;
		AbilityTable->AddRow(TEXT("Strike"), Row);

		Strike = NewObject<UDwAbilityDefinition>(GetTransientPackage());
		Strike->DisplayName = FText::FromString(TEXT("Strike"));
		Strike->AbilityClass = UDwTestAbility_Strike::StaticClass();
		Strike->AbilityTag = DwTags::Ability_Attack_Melee;
		Strike->Tuning.DataTable = AbilityTable;
		Strike->Tuning.RowName = TEXT("Strike");
		Asc(Player)->GrantAbility(Strike);
	});

	AfterEach([this]()
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		World = nullptr;
	});

	It("gets every shared effect from DA_GameData, not the native fallback", [this]()
	{
		const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
		if (!TestNotNull(TEXT("DA_GameData"), GameData)) { return; }
		const FDwSharedEffects& E = GameData->Effects;
		const TSubclassOf<UGameplayEffect> Slots[] = {
			E.InitUnitStats, E.InitShipStats, E.Damage, E.Heal, E.RestoreMana, E.AddFear, E.Cleanse, E.Cowering, E.Frozen,
			E.Rooted, E.Guarded, E.ConquerorExtraMove, E.DamageBonus, E.TurnBudgetRefresh, E.CostAction, E.CostMove,
			E.CostMana, E.SpendAll, E.Kill };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Slots); ++Index)
		{
			TestNotNull(FString::Printf(TEXT("Effects slot %d"), Index), Slots[Index].Get());
		}
	});

	Describe("Stats", [this]()
	{
		It("initialises every attribute, maximums before currents", [this]()
		{
			TestEqual(TEXT("Health"), Get(Player, FUnitSet::GetHealthAttribute()), 100.f);
			TestEqual(TEXT("MaxMana"), Get(Player, FUnitSet::GetMaxManaAttribute()), 20.f);
			TestEqual(TEXT("Mana"), Get(Player, FUnitSet::GetManaAttribute()), 20.f);
			TestEqual(TEXT("MoveRange"), Get(Player, FUnitSet::GetMoveRangeAttribute()), 5.f);
			TestEqual(TEXT("MaxFear"), Get(Player, FUnitSet::GetMaxFearAttribute()), 3.f);
		});

		It("refreshes the turn budget, and the Conqueror gets a second action", [this]()
		{
			const FDwSharedEffects& E = DwGas::GetSharedEffects();
			Apply(Player, Asc(Player), E.SpendAll);
			TestEqual(TEXT("Spent AP"), Get(Player, FUnitSet::GetActionPointsAttribute()), 0.f);
			TestEqual(TEXT("Spent MP"), Get(Player, FUnitSet::GetMovePointsAttribute()), 0.f);
			Apply(Player, Asc(Player), E.TurnBudgetRefresh);
			TestEqual(TEXT("AP refilled to MaxActionPoints"), Get(Player, FUnitSet::GetActionPointsAttribute()), 1.f);
			TestEqual(TEXT("MP refilled to MoveRange"), Get(Player, FUnitSet::GetMovePointsAttribute()), 5.f);
			Apply(Player, Asc(Player), E.ConquerorExtraMove);
			Apply(Player, Asc(Player), E.TurnBudgetRefresh);
			TestEqual(TEXT("Conqueror: 2 actions"), Get(Player, FUnitSet::GetActionPointsAttribute()), 2.f);
		});
	});

	Describe("Damage", [this]()
	{
		It("runs the damage rules through GE_Damage, with bonus and reduction", [this]()
		{
			InitUnit(Player, /*Attack*/ 10.f, /*Bonus*/ 0.2f);
			InitUnit(Enemy, 10.f, 0.f, /*Reduction*/ 0.5f);
			Apply(Player, Asc(Enemy), DwGas::GetSharedEffects().Damage, { { DwTags::Data_Damage, 20.f }, { DwTags::Data_DamageScale, 0.5f } });
			TestEqual(TEXT("(20 + 5) x 1.2 x 0.5 = 15"), Get(Enemy, FUnitSet::GetHealthAttribute()), 85.f);
			TestEqual(TEXT("Meta attribute cleared"), Get(Enemy, FUnitSet::GetIncomingDamageAttribute()), 0.f);
		});

		It("damages ships through the same effect, without unit-only terms", [this]()
		{
			FGameplayEffectSpecHandle Init = EnemyShip->GetDwAbilitySystem()->MakeOutgoingSpec(DwGas::GetSharedEffects().InitShipStats, 1, EnemyShip->GetDwAbilitySystem()->MakeEffectContext());
			UDwGE_InitShipStats::SetValues(*Init.Data, 250.f, 250.f, 70.f);
			EnemyShip->GetDwAbilitySystem()->ApplyGameplayEffectSpecToSelf(*Init.Data);

			Apply(Player, EnemyShip->GetDwAbilitySystem(), DwGas::GetSharedEffects().Damage, { { DwTags::Data_Damage, 70.f } });
			TestEqual(TEXT("Cannon: 250 - 70"), EnemyShip->GetDwAbilitySystem()->GetNumericAttribute(UDwShipAttributeSet::GetHealthAttribute()), 180.f);
		});

		It("marks the dead with State.Dead and reports the killer once", [this]()
		{
			int32 Notifications = 0;
			AActor* Killer = nullptr;
			Asc(Enemy)->OnHealthDepleted.AddLambda([&](UDwAbilitySystemComponent*, AActor* Instigator) { ++Notifications; Killer = Instigator; });

			Apply(Player, Asc(Enemy), DwGas::GetSharedEffects().Damage, { { DwTags::Data_Damage, 150.f } });
			Apply(Player, Asc(Enemy), DwGas::GetSharedEffects().Damage, { { DwTags::Data_Damage, 50.f } });
			TestEqual(TEXT("Health floors at 0"), Get(Enemy, FUnitSet::GetHealthAttribute()), 0.f);
			TestTrue(TEXT("State.Dead"), Asc(Enemy)->HasMatchingGameplayTag(DwTags::State_Dead));
			TestEqual(TEXT("Notified once"), Notifications, 1);
			TestTrue(TEXT("Kill credit to the instigator"), Killer == Player);

			Apply(Player, Asc(Enemy), DwGas::GetSharedEffects().Heal, { { DwTags::Data_Heal, 50.f } });
			TestEqual(TEXT("The dead are never healed"), Get(Enemy, FUnitSet::GetHealthAttribute()), 0.f);
		});
	});

	Describe("TurnDurations", [this]()
	{
		It("a 1-turn freeze applied on the player turn lasts through the enemy turn, then ends", [this]()
		{
			Apply(Player, Asc(Enemy), DwGas::GetSharedEffects().Frozen);
			TestTrue(TEXT("Frozen now"), Asc(Enemy)->HasMatchingGameplayTag(DwTags::Status_Incapacitated_Frozen));
			Asc(Enemy)->HandleTurnBoundary(EDwTurnTickPoint::OwnerSideTurnStart);
			TestTrue(TEXT("Still frozen at the enemy turn start"), Asc(Enemy)->HasMatchingGameplayTag(DwTags::Status_Incapacitated_Frozen));
			Asc(Enemy)->HandleTurnBoundary(EDwTurnTickPoint::OwnerSideTurnEnd);
			TestFalse(TEXT("Gone after the enemy turn ends"), Asc(Enemy)->HasMatchingGameplayTag(DwTags::Status_Incapacitated_Frozen));
		});

		It("an effect applied during its owner's own turn survives that turn (Eyes of the Sea)", [this]()
		{
			Asc(Enemy)->HandleTurnBoundary(EDwTurnTickPoint::OwnerSideTurnStart);
			const FActiveGameplayEffectHandle Buff = Apply(Enemy, Asc(Enemy), DwGas::GetSharedEffects().Frozen);
			Asc(Enemy)->HandleTurnBoundary(EDwTurnTickPoint::OwnerSideTurnEnd);
			TestEqual(TEXT("Survives the turn it was applied in"), Asc(Enemy)->GetTurnsRemaining(Buff), 1);
			Asc(Enemy)->HandleTurnBoundary(EDwTurnTickPoint::OwnerSideTurnStart);
			TestTrue(TEXT("Active for the next own turn"), Asc(Enemy)->GetActiveGameplayEffect(Buff) != nullptr);
			Asc(Enemy)->HandleTurnBoundary(EDwTurnTickPoint::OwnerSideTurnEnd);
			TestNull(TEXT("Expires at the end of the next own turn"), Asc(Enemy)->GetActiveGameplayEffect(Buff));
		});

		It("Guarded lasts through the enemy turn and ends at the next player turn start", [this]()
		{
			Asc(Ally)->HandleTurnBoundary(EDwTurnTickPoint::OwnerSideTurnStart);
			Apply(Player, Asc(Ally), DwGas::GetSharedEffects().Guarded);
			Asc(Ally)->HandleTurnBoundary(EDwTurnTickPoint::OwnerSideTurnEnd);
			TestTrue(TEXT("Guarded during the enemy turn"), Asc(Ally)->HasMatchingGameplayTag(DwTags::Status_Guarded));
			Asc(Ally)->HandleTurnBoundary(EDwTurnTickPoint::OwnerSideTurnStart);
			TestFalse(TEXT("Ends when the player side starts again"), Asc(Ally)->HasMatchingGameplayTag(DwTags::Status_Guarded));
		});

		It("SetByCaller Data.Turns overrides the duration", [this]()
		{
			Apply(Player, Asc(Enemy), DwGas::GetSharedEffects().Rooted, { { DwTags::Data_Turns, 2.f } });
			Asc(Enemy)->HandleTurnBoundary(EDwTurnTickPoint::OwnerSideTurnStart);
			Asc(Enemy)->HandleTurnBoundary(EDwTurnTickPoint::OwnerSideTurnEnd);
			TestTrue(TEXT("One turn left"), Asc(Enemy)->HasMatchingGameplayTag(DwTags::Status_Rooted));
			Asc(Enemy)->HandleTurnBoundary(EDwTurnTickPoint::OwnerSideTurnStart);
			Asc(Enemy)->HandleTurnBoundary(EDwTurnTickPoint::OwnerSideTurnEnd);
			TestFalse(TEXT("Gone after two"), Asc(Enemy)->HasMatchingGameplayTag(DwTags::Status_Rooted));
		});
	});

	Describe("Statuses", [this]()
	{
		It("toggles Status.Afraid with Fear and clamps at MaxFear", [this]()
		{
			const TSubclassOf<UGameplayEffect> AddFear = DwGas::GetSharedEffects().AddFear;
			TestFalse(TEXT("Not afraid at 0"), Asc(Enemy)->HasMatchingGameplayTag(DwTags::Status_Afraid));
			Apply(Player, Asc(Enemy), AddFear, { { DwTags::Data_Fear, 1.f } });
			TestTrue(TEXT("Afraid at 1"), Asc(Enemy)->HasMatchingGameplayTag(DwTags::Status_Afraid));
			Apply(Player, Asc(Enemy), AddFear, { { DwTags::Data_Fear, 5.f } });
			TestEqual(TEXT("Clamped at 3"), Get(Enemy, FUnitSet::GetFearAttribute()), 3.f);
			Apply(Player, Asc(Enemy), AddFear, { { DwTags::Data_Fear, -3.f } });
			TestFalse(TEXT("Calm again at 0"), Asc(Enemy)->HasMatchingGameplayTag(DwTags::Status_Afraid));
		});

		It("cleanses debuffs but keeps buffs (Rally Cry)", [this]()
		{
			const FDwSharedEffects& E = DwGas::GetSharedEffects();
			Apply(Enemy, Asc(Player), E.Frozen);
			Apply(Enemy, Asc(Player), E.Rooted);
			Apply(Ally, Asc(Player), E.Guarded);
			Apply(Player, Asc(Player), E.Cleanse);
			TestFalse(TEXT("Frozen removed"), Asc(Player)->HasMatchingGameplayTag(DwTags::Status_Incapacitated_Frozen));
			TestFalse(TEXT("Rooted removed"), Asc(Player)->HasMatchingGameplayTag(DwTags::Status_Rooted));
			TestTrue(TEXT("Guarded (a buff) kept"), Asc(Player)->HasMatchingGameplayTag(DwTags::Status_Guarded));
		});
	});

	Describe("Costs", [this]()
	{
		It("blocks activation without enough mana or an action, and pays both on success", [this]()
		{
			Asc(Player)->SetNumericAttributeBase(FUnitSet::GetManaAttribute(), 5.f);
			TestFalse(TEXT("5 MP < 10 MP cost"), Activate(Player, Enemy));
			TestEqual(TEXT("Nothing spent"), Get(Player, FUnitSet::GetActionPointsAttribute()), 1.f);

			Asc(Player)->SetNumericAttributeBase(FUnitSet::GetManaAttribute(), 20.f);
			TestTrue(TEXT("Activates with budget"), Activate(Player, Enemy));
			TestEqual(TEXT("Paid 10 MP"), Get(Player, FUnitSet::GetManaAttribute()), 10.f);
			TestEqual(TEXT("Paid the action"), Get(Player, FUnitSet::GetActionPointsAttribute()), 0.f);
			TestEqual(TEXT("Move untouched"), Get(Player, FUnitSet::GetMovePointsAttribute()), 5.f);
			TestFalse(TEXT("No action left"), Activate(Player, Enemy));
		});

		It("spends both budgets under the single-choice rule (Conqueror crew)", [this]()
		{
			Asc(Player)->AddLooseGameplayTag(DwTags::Rule_Turn_SingleChoice);
			TestTrue(TEXT("Activates"), Activate(Player, Enemy));
			TestEqual(TEXT("Move spent too"), Get(Player, FUnitSet::GetMovePointsAttribute()), 0.f);
		});

		It("is blocked while incapacitated or dead", [this]()
		{
			const FActiveGameplayEffectHandle Freeze = Apply(Enemy, Asc(Player), DwGas::GetSharedEffects().Frozen);
			TestFalse(TEXT("Frozen can't act"), Activate(Player, Enemy));
			Asc(Player)->RemoveActiveGameplayEffect(Freeze);
			Apply(Enemy, Asc(Player), DwGas::GetSharedEffects().Kill);
			TestFalse(TEXT("Dead can't act"), Activate(Player, Enemy));
		});
	});

	Describe("Preview", [this]()
	{
		It("matches the damage actually dealt", [this]()
		{
			InitUnit(Player, /*Attack*/ 14.f, /*Bonus*/ 0.1f);
			InitUnit(Enemy, 10.f, 0.f, /*Reduction*/ 0.25f);

			FDwCombatSnapshot Snapshot;
			Snapshot.Grid = FDwBoardBuilder::BuildSingleDeck(FDwDeckAscii::Parse({ TEXT("...."), TEXT("....") }), EDwSide::Player);
			Snapshot.Units = { Snap(Player, 1, EDwSide::Player, FIntPoint(0, 0)), Snap(Ally, 2, EDwSide::Player, FIntPoint(0, 1)),
				Snap(Enemy, 3, EDwSide::Enemy, FIntPoint(1, 0)) };

			FDwTargetingContext Context;
			Context.Snapshot = &Snapshot;
			Context.Ability = Strike;
			Context.Source = FDwUnitHandle(1);
			Context.DamageMultiplier = 1.25f; // e.g. synergy

			const FDwAbilityPreview Preview = GetDefault<UDwTestAbility_Strike>()->PreviewOutcome(Context, FDwAbilityTarget::MakeUnit(FDwUnitHandle(3), FIntPoint(1, 0)));
			if (!TestTrue(TEXT("Valid target"), Preview.bValid && Preview.Targets.Num() == 1))
			{
				return;
			}
			const float Before = Get(Enemy, FUnitSet::GetHealthAttribute());
			TestTrue(TEXT("Activates"), Activate(Player, Enemy, 1.25f));
			const int32 Dealt = FMath::RoundToInt(Before - Get(Enemy, FUnitSet::GetHealthAttribute()));
			TestEqual(TEXT("Preview == result"), Preview.Targets[0].Damage, Dealt);
			TestEqual(TEXT("(20 + 7) x 1.1 x 1.25 x 0.75 = 27.84 -> 28"), Dealt, 28);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
