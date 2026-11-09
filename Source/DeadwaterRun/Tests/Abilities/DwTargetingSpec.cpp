// Copyright Brandon Hillig 2026

// DW-101: ability targeting, resolve and redirect rules on plain snapshots (no world).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/DwTargeting.h"
#include "Board/DwBoardBuilder.h"
#include "Combat/DwCombatSnapshot.h"
#include "Core/DwGameplayTags.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/Tuning/DwTuningRows.h"
#include "Engine/DataTable.h"
#include "Tests/DwTestAbilities.h"

BEGIN_DEFINE_SPEC(FDwTargetingSpec, "DeadwaterRun.Rules.Targeting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

	FDwCombatSnapshot Snapshot;
	UDataTable* Table = nullptr;
	UDwAbilityDefinition* Definition = nullptr;
	UDwTestAbility_Strike* Ability = nullptr; // a configurable instance; the queries are const and world-free

	void SetTuning(int32 Range, int32 MinHits = 0, int32 MaxHits = 0, int32 Damage = 10)
	{
		Table->EmptyTable();
		FDwAbilityTuningRow Row;
		Row.Range = Range; Row.MinHits = MinHits; Row.MaxHits = MaxHits; Row.Damage = Damage;
		Table->AddRow(TEXT("Row"), Row);
	}

	FDwUnitSnapshot& AddUnit(int32 Id, EDwSide Side, FIntPoint Tile, float Health = 100.f)
	{
		FDwUnitSnapshot& U = Snapshot.Units.AddDefaulted_GetRef();
		U.Handle = FDwUnitHandle(Id); U.Side = Side; U.Tile = Tile; U.Health = Health; U.MaxHealth = 100.f;
		Snapshot.Grid.SetOccupant(Tile, U.Handle, Side);
		return U;
	}

	FDwTargetingContext Context(int32 SourceId) const
	{
		FDwTargetingContext C;
		C.Snapshot = &Snapshot;
		C.Ability = Definition;
		C.Source = FDwUnitHandle(SourceId);
		return C;
	}

	TArray<FDwUnitHandle> TargetUnits(int32 SourceId) const
	{
		TArray<FDwAbilityTarget> Targets;
		Ability->GatherValidTargets(Context(SourceId), Targets);
		TArray<FDwUnitHandle> Units;
		for (const FDwAbilityTarget& T : Targets) { if (T.IsUnit()) { Units.Add(T.Unit); } }
		return Units;
	}

END_DEFINE_SPEC(FDwTargetingSpec)

void FDwTargetingSpec::Define()
{
	BeforeEach([this]()
	{
		//   x: 0123456
		//  y0: S...LN.     ladder (4,0), nest (5,0)
		//  y1: .c.....     crate (1,1)
		//  y2: .......
		Snapshot = FDwCombatSnapshot();
		Snapshot.Grid = FDwBoardBuilder::BuildSingleDeck(FDwDeckAscii::Parse({ TEXT("S...LN."), TEXT(".c....."), TEXT(".......") }), EDwSide::Player);
		Snapshot.Ships.AddDefaulted_GetRef().Side = EDwSide::Enemy;

		Table = NewObject<UDataTable>(GetTransientPackage());
		Table->RowStruct = FDwAbilityTuningRow::StaticStruct();
		Definition = NewObject<UDwAbilityDefinition>(GetTransientPackage());
		Definition->Tuning.DataTable = Table;
		Definition->Tuning.RowName = TEXT("Row");
		Ability = NewObject<UDwTestAbility_Strike>(GetTransientPackage());
		SetTuning(1);
	});

	Describe("Melee", [this]()
	{
		It("hits adjacent enemies only, never allies", [this]()
		{
			Ability->Targeting.bCanTargetElevated = false;
			AddUnit(1, EDwSide::Player, FIntPoint(2, 2));
			AddUnit(2, EDwSide::Player, FIntPoint(1, 2));
			AddUnit(3, EDwSide::Enemy, FIntPoint(3, 2));
			AddUnit(4, EDwSide::Enemy, FIntPoint(5, 2));
			TestTrue(TEXT("Only the adjacent enemy"), TargetUnits(1) == TArray<FDwUnitHandle>{ FDwUnitHandle(3) });
		});

		It("can't reach a unit in the nest, nor strike down from it (C5, C5a placeholder)", [this]()
		{
			Ability->Targeting.bCanTargetElevated = false;
			AddUnit(1, EDwSide::Player, FIntPoint(4, 0)); // on the ladder, next to the nest
			AddUnit(2, EDwSide::Enemy, FIntPoint(5, 0));  // in the nest
			TestTrue(TEXT("Ladder can't melee the nest"), TargetUnits(1).IsEmpty());

			FText Reason;
			TestFalse(TEXT("IsValidTarget agrees"), Ability->IsValidTarget(Context(1), FDwAbilityTarget::MakeUnit(FDwUnitHandle(2), FIntPoint(5, 0)), &Reason));
			TestTrue(TEXT("Reason names the nest"), Reason.ToString().Contains(TEXT("nest")));

			TestTrue(TEXT("Nest can't melee down"), TargetUnits(2).IsEmpty());
		});
	});

	Describe("Ranged", [this]()
	{
		It("needs range and line of sight, and the nest adds range", [this]()
		{
			Ability->Targeting.bRequiresLineOfSight = true;
			SetTuning(4);
			AddUnit(1, EDwSide::Player, FIntPoint(0, 1));
			AddUnit(2, EDwSide::Enemy, FIntPoint(2, 1));  // behind the crate at (1,1)
			AddUnit(3, EDwSide::Enemy, FIntPoint(0, 2));  // in range, clear
			AddUnit(4, EDwSide::Enemy, FIntPoint(6, 2));  // distance 7: out of range
			TestTrue(TEXT("Only the clear target in range"), TargetUnits(1) == TArray<FDwUnitHandle>{ FDwUnitHandle(3) });

			// From the nest (5,0): +2 range -> 6. Unit 4 at distance 3, unit 3 at distance 7.
			AddUnit(5, EDwSide::Player, FIntPoint(5, 0));
			const TArray<FDwUnitHandle> FromNest = TargetUnits(5);
			TestTrue(TEXT("Nest reaches (6,2)"), FromNest.Contains(FDwUnitHandle(4)));
			TestFalse(TEXT("Nest doesn't reach distance 7"), FromNest.Contains(FDwUnitHandle(3)));
		});

		It("offers the enemy ship for SingleUnitOrShip", [this]()
		{
			Ability->Targeting.Pattern = EDwTargetPattern::SingleUnitOrShip;
			AddUnit(1, EDwSide::Player, FIntPoint(0, 0));
			TArray<FDwAbilityTarget> Targets;
			Ability->GatherValidTargets(Context(1), Targets);
			TestTrue(TEXT("Enemy ship targetable"), Targets.Contains(FDwAbilityTarget::MakeShip(EDwSide::Enemy)));
		});
	});

	Describe("Groups", [this]()
	{
		It("AllEnemies hits every valid enemy whatever the aim", [this]()
		{
			Ability->Targeting.Pattern = EDwTargetPattern::AllEnemies;
			Ability->Targeting.bIgnoreRange = true;
			AddUnit(1, EDwSide::Player, FIntPoint(0, 0));
			AddUnit(2, EDwSide::Enemy, FIntPoint(6, 2));
			AddUnit(3, EDwSide::Enemy, FIntPoint(3, 2));
			AddUnit(4, EDwSide::Enemy, FIntPoint(2, 2)).bAlive = false;
			TArray<FDwAbilityTarget> Hit;
			Ability->ResolveTargets(Context(1), FDwAbilityTarget(), Hit);
			TestEqual(TEXT("Two living enemies"), Hit.Num(), 2);
		});

		It("RandomEnemies draws MinHits..MaxHits from the combat stream, deterministically", [this]()
		{
			Ability->Targeting.Pattern = EDwTargetPattern::RandomEnemies;
			Ability->Targeting.bIgnoreRange = true;
			SetTuning(0, 3, 5);
			AddUnit(1, EDwSide::Player, FIntPoint(0, 0));
			AddUnit(2, EDwSide::Enemy, FIntPoint(6, 2));
			AddUnit(3, EDwSide::Enemy, FIntPoint(3, 2));

			for (int32 Seed = 0; Seed < 50; ++Seed)
			{
				FRandomStream A(Seed), B(Seed);
				FDwTargetingContext CA = Context(1), CB = Context(1);
				CA.Rng = &A; CB.Rng = &B;
				TArray<FDwAbilityTarget> HitsA, HitsB;
				Ability->ResolveTargets(CA, FDwAbilityTarget(), HitsA);
				Ability->ResolveTargets(CB, FDwAbilityTarget(), HitsB);
				TestTrue(TEXT("3-5 hits"), HitsA.Num() >= 3 && HitsA.Num() <= 5);
				TestTrue(TEXT("Same seed, same hits"), HitsA == HitsB);
			}

			const FDwAbilityPreview Preview = Ability->PreviewOutcome(Context(1), FDwAbilityTarget());
			TestEqual(TEXT("Preview lists each possible target once"), Preview.Targets.Num(), 2);
			TestEqual(TEXT("Preview hit range"), FIntPoint(Preview.MinHits, Preview.MaxHits), FIntPoint(3, 5));
		});
	});

	Describe("Defend", [this]()
	{
		It("redirects a single-target enemy hit to an adjacent living guardian", [this]()
		{
			AddUnit(1, EDwSide::Enemy, FIntPoint(2, 2));
			FDwUnitSnapshot& Guarded = AddUnit(2, EDwSide::Player, FIntPoint(3, 2));
			AddUnit(3, EDwSide::Player, FIntPoint(4, 2)); // the guardian, next to unit 2
			Guarded.Tags.AddTag(DwTags::Status_Guarded);
			Guarded.GuardedBy = FDwUnitHandle(3);

			TArray<FDwAbilityTarget> Hit;
			Ability->ResolveTargets(Context(1), FDwAbilityTarget::MakeUnit(FDwUnitHandle(2), FIntPoint(3, 2)), Hit);
			TestTrue(TEXT("Guardian takes the hit"), Hit.Num() == 1 && Hit[0].Unit == FDwUnitHandle(3));

			const FDwAbilityPreview Preview = Ability->PreviewOutcome(Context(1), FDwAbilityTarget::MakeUnit(FDwUnitHandle(2), FIntPoint(3, 2)));
			TestTrue(TEXT("Preview shows the redirect"), Preview.Targets.Num() == 1 && Preview.Targets[0].Target.Unit == FDwUnitHandle(3));
		});

		It("doesn't redirect when the guardian moved away or died", [this]()
		{
			AddUnit(1, EDwSide::Enemy, FIntPoint(2, 2));
			FDwUnitSnapshot& Guarded = AddUnit(2, EDwSide::Player, FIntPoint(3, 2));
			FDwUnitSnapshot& Guardian = AddUnit(3, EDwSide::Player, FIntPoint(6, 0));
			Guarded.Tags.AddTag(DwTags::Status_Guarded);
			Guarded.GuardedBy = FDwUnitHandle(3);

			TArray<FDwAbilityTarget> Hit;
			Ability->ResolveTargets(Context(1), FDwAbilityTarget::MakeUnit(FDwUnitHandle(2), FIntPoint(3, 2)), Hit);
			TestTrue(TEXT("Far guardian: original target"), Hit.Num() == 1 && Hit[0].Unit == FDwUnitHandle(2));

			Guardian.Tile = FIntPoint(4, 2);
			Guardian.bAlive = false;
			Ability->ResolveTargets(Context(1), FDwAbilityTarget::MakeUnit(FDwUnitHandle(2), FIntPoint(3, 2)), Hit);
			TestTrue(TEXT("Dead guardian: original target"), Hit.Num() == 1 && Hit[0].Unit == FDwUnitHandle(2));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
