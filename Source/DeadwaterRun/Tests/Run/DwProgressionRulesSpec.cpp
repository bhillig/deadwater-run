// Copyright Brandon Hillig 2026

// DW-203: XP, levels, perks and the Veteran Crew hooks (Docs/Architecture/12-Progression.md "How it's tested"). Pure.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Run/DwCaptainRules.h"
#include "Run/DwProgressionRules.h"
#include "Tests/Run/DwRunTestContent.h"

BEGIN_DEFINE_SPEC(FDwProgressionRulesSpec, "DeadwaterRun.Rules.Progression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	DwRunTest::FContent C;
	FDwCrewMemberState Hand;

	/** Grants Amount to Hand with the fixture table (100, 100, 200 -> max level 4). */
	int32 Grant(int32 Amount, float XpMultiplier = 1.f, float ThresholdMultiplier = 1.f, int32 MaxLevel = 0)
	{
		return FDwProgressionRules::GrantXp(Hand, Amount, XpMultiplier, ThresholdMultiplier, C.Progression, MaxLevel);
	}

	TArray<const UDwPerkDefinition*> RollPerks(const FDwCrewMemberState& Crew, int32 Count, int32 Seed = 1)
	{
		FRandomStream Random(Seed);
		return FDwProgressionRules::RollPerkChoices(Crew, C.Perks, Count, Random);
	}
END_DEFINE_SPEC(FDwProgressionRulesSpec)

void FDwProgressionRulesSpec::Define()
{
	using namespace DwRunTest;

	BeforeEach([this]()
	{
		C.Create();
		Hand = Member(C.Deckhand);
	});
	AfterEach([this]() { C.Destroy(); });

	Describe("GrantXp", [this]()
	{
		It("levels up exactly at the threshold, not one short", [this]()
		{
			TestEqual(TEXT("99: no level"), Grant(99), 0);
			TestTrue(TEXT("Level 1, 99 XP"), Hand.Level == 1 && Hand.Xp == 99);
			TestEqual(TEXT("+1: level 2"), Grant(1), 1);
			TestTrue(TEXT("Level 2, 0 XP"), Hand.Level == 2 && Hand.Xp == 0);
		});

		It("gains several levels from one big grant and keeps the remainder", [this]()
		{
			TestEqual(TEXT("250 = 100 + 100 + 50"), Grant(250), 2);
			TestTrue(TEXT("Level 3, 50 XP"), Hand.Level == 3 && Hand.Xp == 50);
		});

		It("stops at MaxLevel, or at the table's last row, with no XP left over", [this]()
		{
			TestEqual(TEXT("MaxLevel 2"), Grant(1000, 1.f, 1.f, 2), 1);
			TestTrue(TEXT("Level 2, 0 XP"), Hand.Level == 2 && Hand.Xp == 0);
			TestEqual(TEXT("Capped: nothing more"), Grant(1000, 1.f, 1.f, 2), 0);
			TestEqual(TEXT("No cap: the table ends at level 4"), Grant(10000), 2);
			TestTrue(TEXT("Level 4, 0 XP"), Hand.Level == 4 && Hand.Xp == 0);
			TestEqual(TEXT("XpToNext at the end"), FDwProgressionRules::GetXpToNext(C.Progression, 4), 0);
		});

		It("grows current Health with each level's MaxHealth gain", [this]()
		{
			Hand.Health = 40.f;
			Grant(200);
			TestEqual(TEXT("Level 3"), Hand.Level, 3);
			TestEqual(TEXT("Max 100"), Hand.GetMaxHealth(), 100.f);
			TestEqual(TEXT("40 + 2 x 10"), Hand.Health, 60.f);
		});

		It("gives the dead nothing", [this]()
		{
			Hand.bAlive = false;
			TestEqual(TEXT("No levels"), Grant(500), 0);
			TestTrue(TEXT("Untouched"), Hand.Level == 1 && Hand.Xp == 0);
		});

		It("applies the XP and threshold multipliers (both readings of K7, and both together)", [this]()
		{
			struct FCase { float Xp; float Threshold; int32 Amount; int32 Level; int32 Left; };
			const FCase Cases[] = {
				{ 1.5f, 1.f, 140, 3, 10 },  // a) +50% XP: 210 = 100 + 100 + 10
				{ 1.f, 0.5f, 140, 3, 40 },  // b) half the XP per level: 140 = 50 + 50 + 40
				{ 1.5f, 0.5f, 140, 4, 0 },  // c) both: 210 = 50 + 50 + 100, the last level: no XP kept
				{ 1.f, 1.f, 140, 2, 40 },   // neither
			};
			for (const FCase& Case : Cases)
			{
				Hand = Member(C.Deckhand);
				Grant(Case.Amount, Case.Xp, Case.Threshold);
				TestEqual(*FString::Printf(TEXT("x%.1f / x%.1f: level"), Case.Xp, Case.Threshold), Hand.Level, Case.Level);
				TestEqual(*FString::Printf(TEXT("x%.1f / x%.1f: XP left"), Case.Xp, Case.Threshold), Hand.Xp, Case.Left);
			}
			TestEqual(TEXT("Threshold rounds, never 0"), FDwProgressionRules::GetXpToNext(C.Progression, 1, 0.001f), 1);
		});
	});

	Describe("GetLevelStats", [this]()
	{
		It("is base + per-level gains x (Level - 1), matching what combat spawns", [this]()
		{
			const FDwUnitTuningRow& Row = *C.Deckhand->GetTuningRow<FDwUnitTuningRow>();
			const FDwLevelStats L1 = FDwProgressionRules::GetLevelStats(Row, 1);
			const FDwLevelStats L3 = FDwProgressionRules::GetLevelStats(Row, 3);
			TestTrue(TEXT("Level 1"), L1.MaxHealth == 80 && L1.MaxMana == 20);
			TestTrue(TEXT("Level 3"), L3.MaxHealth == 100 && L3.MaxMana == 24 && L3.MaxHealth == Row.GetMaxHealthAtLevel(3));
			const FDwLevelStats Gain = L3 - L1;
			TestTrue(TEXT("Difference"), Gain.MaxHealth == 20 && Gain.MaxMana == 4 && Gain.AttackDamage == 0);
		});
	});

	Describe("Perks", [this]()
	{
		It("rolls the requested count with no duplicates, only perks this unit can take", [this]()
		{
			for (int32 Seed = 0; Seed < 30; ++Seed)
			{
				const TArray<const UDwPerkDefinition*> Three = RollPerks(Hand, 3, Seed);
				TestEqual(TEXT("3 choices"), Three.Num(), 3);
				TestEqual(TEXT("No duplicates"), TSet<const UDwPerkDefinition*>(Three).Num(), Three.Num());
				TestFalse(TEXT("Marksman needs Unit.Archetype.Sniper"), Three.Contains(C.Marksman));
			}
			TestEqual(TEXT("Count 2"), RollPerks(Hand, 2).Num(), 2);
			TestEqual(TEXT("More than the pool has"), RollPerks(Hand, 9).Num(), 3);
			FRandomStream Random(1);
			TestTrue(TEXT("No pool, no choices"), FDwProgressionRules::RollPerkChoices(Hand, nullptr, 3, Random).IsEmpty());
		});

		It("offers tag-restricted perks to units with the tags", [this]()
		{
			C.Deckhand->UnitTags.AddTag(DwTags::Unit_Archetype_Sniper);
			TestTrue(TEXT("Sniper gets Marksman"), RollPerks(Hand, 9).Contains(C.Marksman));
		});

		It("respects MaxStacks", [this]()
		{
			TestTrue(TEXT("Swift"), FDwProgressionRules::ApplyPerk(Hand, *C.Swift));
			TestFalse(TEXT("Swift twice refused"), FDwProgressionRules::ApplyPerk(Hand, *C.Swift));
			TestFalse(TEXT("Swift no longer offered"), RollPerks(Hand, 9).Contains(C.Swift));
			TestTrue(TEXT("Tough 1"), FDwProgressionRules::ApplyPerk(Hand, *C.Tough));
			TestTrue(TEXT("Tough still offered at 1 of 2"), RollPerks(Hand, 9).Contains(C.Tough));
			TestTrue(TEXT("Tough 2"), FDwProgressionRules::ApplyPerk(Hand, *C.Tough));
			TestFalse(TEXT("Tough maxed"), RollPerks(Hand, 9).Contains(C.Tough));
			TestEqual(TEXT("Count"), FDwProgressionRules::CountPerk(Hand, *C.Tough), 2);
			TestEqual(TEXT("Three perks owned"), Hand.Perks.Num(), 3);
		});

		It("adds a perk's granted ability once", [this]()
		{
			TestTrue(TEXT("Lamplighter"), FDwProgressionRules::ApplyPerk(Hand, *C.Lamplighter));
			TestTrue(TEXT("Lantern learned"), Hand.Abilities.Num() == 1 && Hand.Abilities[0] == C.Lantern);
			C.Lamplighter->MaxStacks = 2;
			FDwProgressionRules::ApplyPerk(Hand, *C.Lamplighter);
			TestEqual(TEXT("Not twice"), Hand.Abilities.Num(), 1);
		});

		It("follows the weights and is stable per seed", [this]()
		{
			C.Tough->Weight = 8.f;
			C.Swift->Weight = 1.f;
			C.Lamplighter->Weight = 1.f;
			int32 Tough = 0;
			for (int32 Seed = 0; Seed < 1000; ++Seed)
			{
				const TArray<const UDwPerkDefinition*> One = RollPerks(Hand, 1, Seed);
				Tough += One.Num() == 1 && One[0] == C.Tough ? 1 : 0;
				TestTrue(TEXT("Stable"), RollPerks(Hand, 3, Seed) == RollPerks(Hand, 3, Seed));
			}
			TestTrue(*FString::Printf(TEXT("~80%% Tough (%d of 1000)"), Tough), Tough > 740 && Tough < 860);
			C.Swift->Weight = 0.f;
			TestFalse(TEXT("Weight 0 never offered"), RollPerks(Hand, 9).Contains(C.Swift));
		});
	});

	Describe("FDwCaptainRules", [this]()
	{
		It("returns the Veteran Crew multipliers only for a captain with Rule.Captain.VeteranCrew", [this]()
		{
			C.CaptainRow().XpMultiplier = 1.5f;
			C.CaptainRow().LevelThresholdMultiplier = 0.5f;
			TestEqual(TEXT("No rule: XP x1"), FDwCaptainRules::GetXpMultiplier(C.Captain), 1.f);
			TestEqual(TEXT("No rule: threshold x1"), FDwCaptainRules::GetLevelThresholdMultiplier(C.Captain), 1.f);
			C.Captain->RuleTags.AddTag(DwTags::Rule_Captain_VeteranCrew);
			TestEqual(TEXT("XP x1.5"), FDwCaptainRules::GetXpMultiplier(C.Captain), 1.5f);
			TestEqual(TEXT("Threshold x0.5"), FDwCaptainRules::GetLevelThresholdMultiplier(C.Captain), 0.5f);
			C.CaptainRow().LevelThresholdMultiplier = 0.f;
			TestEqual(TEXT("A 0 threshold is ignored"), FDwCaptainRules::GetLevelThresholdMultiplier(C.Captain), 1.f);
			TestEqual(TEXT("No captain"), FDwCaptainRules::GetXpMultiplier(nullptr), 1.f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
