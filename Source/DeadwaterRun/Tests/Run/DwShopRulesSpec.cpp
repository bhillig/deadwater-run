// Copyright Brandon Hillig 2026

// DW-204: the shop (Docs/Architecture/10-Economy-Shop-Loot.md "Shop", "How it's tested"). Pure: FDwShopRules over a
// hand-built FDwRunState and transient content. The run flow is DeadwaterRun.Rules.Run "Shop (DW-204)".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Count.h"
#include "Board/DwBoardBuilder.h"
#include "Run/Economy/DwRewardRules.h"
#include "Run/Economy/DwShopRules.h"
#include "Tests/Run/DwRunTestContent.h"

BEGIN_DEFINE_SPEC(FDwShopRulesSpec, "DeadwaterRun.Rules.Shop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	DwRunTest::FContent C;
	FDwRunState State;
	/** Every result a test saw, for the "every EDwPurchaseResult is reachable" check. */
	TSet<EDwPurchaseResult> Seen;

	FDwShopContext Context()
	{
		FDwShopContext Out;
		Out.Economy = &C.EconomyRow();
		Out.MaxAbilitySlots = 2; // C.RunRow().MaxAbilitySlots
		return Out;
	}

	/** Opens the test shop on State (node 3) from Seed. */
	void Open(int32 Seed = 1)
	{
		FRandomStream Random(Seed);
		State.Shop = FDwShopRules::GenerateInventory(State, *C.Shop, Context(), Random);
		State.Shop.Node = 3;
		State.Shop.Offers.Reserve(64); // tests hold offer pointers while adding offers: no reallocation
	}

	const FDwShopOffer* First(EDwShopOfferType Type, TFunction<bool(const FDwShopOffer&)> Where = nullptr) const
	{
		return State.Shop.Offers.FindByPredicate([Type, &Where](const FDwShopOffer& Offer) { return Offer.Type == Type && (!Where || Where(Offer)); });
	}

	int32 Count(EDwShopOfferType Type) const
	{
		return static_cast<int32>(Algo::CountIf(State.Shop.Offers, [Type](const FDwShopOffer& Offer) { return Offer.Type == Type; }));
	}

	EDwPurchaseResult Can(const FDwShopOffer* Offer, const FGuid& Crew = FGuid(), int32 Replace = INDEX_NONE)
	{
		const EDwPurchaseResult Result = FDwShopRules::CanPurchase(State, Offer ? Offer->OfferId : 999, Crew, Replace, Context());
		Seen.Add(Result);
		return Result;
	}

	EDwPurchaseResult Buy(const FDwShopOffer* Offer, const FGuid& Crew = FGuid(), int32 Replace = INDEX_NONE)
	{
		const EDwPurchaseResult Result = FDwShopRules::ApplyPurchase(State, Offer ? Offer->OfferId : 999, Crew, Replace, Context());
		Seen.Add(Result);
		return Result;
	}

	const FDwShopOffer* AbilityOffer(const UDwAbilityDefinition* Ability) const
	{
		return First(EDwShopOfferType::Ability, [Ability](const FDwShopOffer& Offer) { return Offer.Ability == Ability; });
	}

	/** An ability offer for Ability whether or not this seed rolled it (the rules never trust the roll). */
	const FDwShopOffer* ForceAbilityOffer(const UDwAbilityDefinition* Ability)
	{
		if (const FDwShopOffer* Existing = AbilityOffer(Ability))
		{
			return Existing;
		}
		FDwShopOffer& Offer = State.Shop.Offers.AddDefaulted_GetRef();
		Offer.OfferId = State.Shop.Offers.Num() - 1;
		Offer.Type = EDwShopOfferType::Ability;
		Offer.Ability = Ability;
		Offer.Price = 50;
		return &Offer;
	}

	FDwCrewMemberState& Captain() { return State.Crew[0]; }
	FDwCrewMemberState& Hand(int32 Index = 0) { return State.Crew[1 + Index]; }

	/** Captain + two deckhands, a ship at 150 / 250, 500 gold. */
	void Reset()
	{
		State = FDwRunState();
		State.Seed = 42;
		State.Captain = C.Captain;
		State.Crew = { DwRunTest::Member(C.CaptainUnit, 1, /*bIsCaptain*/ true), DwRunTest::Member(C.Deckhand), DwRunTest::Member(C.Deckhand) };
		State.Ship.MaxHealth = 250.f;
		State.Ship.Health = 150.f;
		State.Ship.CannonDamage = 70.f;
		State.Gold = 500;
	}
END_DEFINE_SPEC(FDwShopRulesSpec)

void FDwShopRulesSpec::Define()
{
	using namespace DwRunTest;

	BeforeEach([this]()
	{
		C.Create();
		Reset();
	});
	AfterEach([this]() { C.Destroy(); });

	Describe("GenerateInventory", [this]()
	{
		It("offers repair, every item, abilities, recruits, upgrades, then a disband per crew member beside the captain", [this]()
		{
			Open();
			TArray<EDwShopOfferType> Order;
			for (int32 Index = 0; Index < State.Shop.Offers.Num(); ++Index)
			{
				TestEqual(TEXT("OfferId = index"), State.Shop.Offers[Index].OfferId, Index);
				if (Order.IsEmpty() || Order.Last() != State.Shop.Offers[Index].Type)
				{
					Order.Add(State.Shop.Offers[Index].Type);
				}
			}
			TestTrue(TEXT("Types in order"), Order == TArray<EDwShopOfferType>{ EDwShopOfferType::Repair, EDwShopOfferType::Item, EDwShopOfferType::Ability,
				EDwShopOfferType::Recruit, EDwShopOfferType::ShipUpgrade, EDwShopOfferType::Disband });
			TestEqual(TEXT("One repair"), Count(EDwShopOfferType::Repair), 1);
			TestEqual(TEXT("Every item"), Count(EDwShopOfferType::Item), 2);
			TestEqual(TEXT("ShopAbilityOffers abilities"), Count(EDwShopOfferType::Ability), 3);
			TestEqual(TEXT("One recruit: the pool has one archetype (no repeats)"), Count(EDwShopOfferType::Recruit), 1);
			TestEqual(TEXT("All three upgrades"), Count(EDwShopOfferType::ShipUpgrade), 3);
			TestEqual(TEXT("A disband per deckhand, none for the captain"), Count(EDwShopOfferType::Disband), 2);
			TestFalse(TEXT("Never the captain"), First(EDwShopOfferType::Disband, [this](const FDwShopOffer& O) { return O.CrewId == Captain().Id; }) != nullptr);
		});

		It("reads every price from the offered thing's own tuning row (repair from the economy row)", [this]()
		{
			Open();
			const FDwShopOffer* Repair = First(EDwShopOfferType::Repair);
			TestTrue(TEXT("Repair 100 gold for +100 hull"), Repair && Repair->Price == 100 && Repair->Amount == 100);
			TestEqual(TEXT("Grog 30"), First(EDwShopOfferType::Item, [this](const FDwShopOffer& O) { return O.Item == C.Grog; })->Price, 30);
			TestEqual(TEXT("Tonic 20"), First(EDwShopOfferType::Item, [this](const FDwShopOffer& O) { return O.Item == C.Tonic; })->Price, 20);
			TestEqual(TEXT("Ability ShopPrice 50"), First(EDwShopOfferType::Ability)->Price, 50);
			TestEqual(TEXT("Recruit RecruitCost 80"), First(EDwShopOfferType::Recruit)->Price, 80);
			TestEqual(TEXT("Upgrade price 150"), First(EDwShopOfferType::ShipUpgrade)->Price, 150);
			TestEqual(TEXT("Disband is free"), First(EDwShopOfferType::Disband)->Price, 0);

			C.Items->FindRow<FDwItemTuningRow>(TEXT("Grog"), TEXT("Test"))->Price = 45;
			Open();
			TestEqual(TEXT("Changing the row changes the shop"), First(EDwShopOfferType::Item)->Price, 45);
		});

		It("leaves out anything without a price, owned upgrades, and offer types the shop doesn't list", [this]()
		{
			C.Items->FindRow<FDwItemTuningRow>(TEXT("Tonic"), TEXT("Test"))->Price = 0;
			State.Ship.Upgrades.Add(C.Hull);
			Open();
			TestEqual(TEXT("Unpriced item left out"), Count(EDwShopOfferType::Item), 1);
			TestEqual(TEXT("Owned upgrade left out"), Count(EDwShopOfferType::ShipUpgrade), 2);
			TestFalse(TEXT("Not the hull"), First(EDwShopOfferType::ShipUpgrade, [this](const FDwShopOffer& O) { return O.Upgrade == C.Hull; }) != nullptr);

			C.Shop->OfferTypes = { EDwShopOfferType::Item };
			Open();
			TestEqual(TEXT("Only items"), State.Shop.Offers.Num(), Count(EDwShopOfferType::Item));
			TestFalse(TEXT("No disbands, so recruits get none either"), State.Shop.bDisbands);
		});

		It("offers abilities only to crew who can learn them, never firearms to a blocked captain (K2)", [this]()
		{
			C.Captain->BlockedAbilityTags.AddTag(DwTags::Ability_Weapon_Firearm);
			C.EconomyRow().ShopAbilityOffers = 10;
			Open();
			TestEqual(TEXT("All four learnable abilities"), Count(EDwShopOfferType::Ability), 4);
			const FDwShopOffer* Musket = AbilityOffer(C.Musket);
			const FDwShopOffer* Lantern = AbilityOffer(C.Lantern);
			const FDwShopOffer* Rigging = AbilityOffer(C.Rigging);
			if (!TestTrue(TEXT("Offered"), Musket && Lantern && Rigging)) { return; }
			TestFalse(TEXT("No musket for the blocked captain"), Musket->EligibleCrew.Contains(Captain().Id));
			TestTrue(TEXT("Musket for the deckhands"), Musket->EligibleCrew.Contains(Hand(0).Id) && Musket->EligibleCrew.Contains(Hand(1).Id));
			TestTrue(TEXT("Lantern: captain's pool only"), Lantern->EligibleCrew == TArray<FGuid>{ Captain().Id });
			TestFalse(TEXT("Rigging: not the captain's"), Rigging->EligibleCrew.Contains(Captain().Id));
		});

		It("is the same for the same seed and state, and different seeds roll different shops", [this]()
		{
			auto Signature = [this](int32 Seed)
			{
				Open(Seed);
				FString Out;
				for (const FDwShopOffer& Offer : State.Shop.Offers)
				{
					Out += FString::Printf(TEXT("%d:%s%s%s%s;"), static_cast<int32>(Offer.Type), *GetNameSafe(Offer.Item), *GetNameSafe(Offer.Ability),
						*GetNameSafe(Offer.Unit), *GetNameSafe(Offer.Upgrade));
				}
				return Out;
			};
			TSet<FString> Distinct;
			for (int32 Seed = 1; Seed <= 200; ++Seed)
			{
				const FString A = Signature(Seed);
				TestEqual(FString::Printf(TEXT("Seed %d stable"), Seed), Signature(Seed), A);
				Distinct.Add(A);
			}
			TestTrue(TEXT("Seeds vary the abilities and upgrade order"), Distinct.Num() > 5);
		});
	});

	Describe("Repair", [this]()
	{
		It("adds RepairAmount for RepairCost, can be bought again, and never goes past max hull", [this]()
		{
			Open();
			const FDwShopOffer* Repair = First(EDwShopOfferType::Repair);
			TestEqual(TEXT("Ok"), Buy(Repair), EDwPurchaseResult::Ok);
			TestEqual(TEXT("150 + 100"), State.Ship.Health, 250.f);
			TestEqual(TEXT("100 gold spent"), State.Gold, 400);
			TestEqual(TEXT("GoldSpent"), State.Stats.GoldSpent, 100);
			TestFalse(TEXT("Repair never sells out"), State.Shop.Find(Repair->OfferId)->bSoldOut);
			TestEqual(TEXT("Full hull: ShipFull"), Can(Repair), EDwPurchaseResult::ShipFull);

			State.Ship.Health = 200.f;
			TestEqual(TEXT("Again"), Buy(Repair), EDwPurchaseResult::Ok);
			TestEqual(TEXT("Clamped at max"), State.Ship.Health, 250.f);
			TestEqual(TEXT("Two repairs counted"), State.Stats.Purchases.FindRef(EDwShopOfferType::Repair), 2);
		});
	});

	Describe("Items", [this]()
	{
		It("go into the shared inventory, any kind, until MaxInventory items in total (P10)", [this]()
		{
			Open();
			const FDwShopOffer* Grog = First(EDwShopOfferType::Item, [this](const FDwShopOffer& O) { return O.Item == C.Grog; });
			const FDwShopOffer* Tonic = First(EDwShopOfferType::Item, [this](const FDwShopOffer& O) { return O.Item == C.Tonic; });
			State.AddItem(C.Grog, 3, 0);
			TestEqual(TEXT("Grog"), Buy(Grog), EDwPurchaseResult::Ok);
			TestEqual(TEXT("Tonic"), Buy(Tonic), EDwPurchaseResult::Ok);
			TestEqual(TEXT("Tonic again"), Buy(Tonic), EDwPurchaseResult::Ok);
			TestEqual(TEXT("Six carried"), State.CountItems(), 6);
			TestEqual(TEXT("Four grog in one stack"), State.CountItem(C.Grog->GetFName()), 4);
			TestEqual(TEXT("30 + 20 + 20 spent"), State.Gold, 430);
			TestEqual(TEXT("Full: InventoryFull"), Buy(Grog), EDwPurchaseResult::InventoryFull);
			TestEqual(TEXT("Nothing taken"), State.Gold, 430);
			TestEqual(TEXT("Still six"), State.CountItems(), 6);
		});

		It("respects the cap when items are added any other way", [this]()
		{
			TestEqual(TEXT("Five of ten fit"), State.AddItem(C.Grog, 10, 5), 5);
			TestEqual(TEXT("None more"), State.AddItem(C.Tonic, 1, 5), 0);
			TestEqual(TEXT("No cap"), State.AddItem(C.Tonic, 3, 0), 3);
			TestEqual(TEXT("Room left with cap 6"), State.GetItemRoom(6), 0);
		});
	});

	Describe("Abilities", [this]()
	{
		It("teach a crew member who can learn it; the offer then sells out", [this]()
		{
			Open();
			const FDwShopOffer* Parry = ForceAbilityOffer(C.Parry);
			TestEqual(TEXT("Ok"), Buy(Parry, Hand(0).Id), EDwPurchaseResult::Ok);
			TestTrue(TEXT("Learned"), Hand(0).Abilities.Contains(C.Parry));
			TestEqual(TEXT("50 gold"), State.Gold, 450);
			TestEqual(TEXT("Sold out"), Can(State.Shop.Find(Parry->OfferId), Hand(1).Id), EDwPurchaseResult::SoldOut);
		});

		It("refuse dead, unknown, unsuited and blocked learners, and those who know it already", [this]()
		{
			C.Captain->BlockedAbilityTags.AddTag(DwTags::Ability_Weapon_Firearm);
			Open();
			const FDwShopOffer* Musket = ForceAbilityOffer(C.Musket);
			const FDwShopOffer* Rigging = ForceAbilityOffer(C.Rigging);
			TestEqual(TEXT("Unknown crew"), Can(Musket, FGuid::NewGuid()), EDwPurchaseResult::InvalidTarget);
			TestEqual(TEXT("Blocked captain (K2)"), Can(Musket, Captain().Id), EDwPurchaseResult::InvalidTarget);
			TestEqual(TEXT("Not in the captain's pool"), Can(Rigging, Captain().Id), EDwPurchaseResult::InvalidTarget);
			Hand(1).bAlive = false;
			TestEqual(TEXT("Dead"), Can(Musket, Hand(1).Id), EDwPurchaseResult::InvalidTarget);
			Hand(0).Abilities.Add(C.Musket);
			TestEqual(TEXT("Knows it"), Can(Musket, Hand(0).Id), EDwPurchaseResult::AlreadyKnown);
			TestEqual(TEXT("The basic attack counts as known"), Can(ForceAbilityOffer(C.Cutlass), Hand(0).Id), EDwPurchaseResult::AlreadyKnown);
			TestEqual(TEXT("Nothing spent"), State.Gold, 500);
		});

		It("with every slot full, need a slot to replace (P4)", [this]()
		{
			Hand(0).Abilities = { C.Rigging, C.Musket }; // MaxAbilitySlots 2
			Open();
			const FDwShopOffer* Parry = ForceAbilityOffer(C.Parry);
			TestEqual(TEXT("Full, no slot named"), Can(Parry, Hand(0).Id), EDwPurchaseResult::NoSlot);
			TestEqual(TEXT("A slot that doesn't exist"), Can(Parry, Hand(0).Id, 5), EDwPurchaseResult::NoSlot);
			TestEqual(TEXT("Replace the musket"), Buy(Parry, Hand(0).Id, 1), EDwPurchaseResult::Ok);
			TestTrue(TEXT("Parry in slot 1"), Hand(0).Abilities == TArray<TObjectPtr<const UDwAbilityDefinition>>{ C.Rigging, C.Parry });
		});
	});

	Describe("Recruits", [this]()
	{
		It("join at floor(average living level) + RecruitLevelOffset, at least level 1 (P8)", [this]()
		{
			TestEqual(TEXT("All level 1: max(1, 1 - 1)"), FDwShopRules::GetRecruitLevel(State, C.EconomyRow()), 1);
			Captain().Level = 3; Hand(0).Level = 4; Hand(1).Level = 4;
			TestEqual(TEXT("floor(11 / 3) - 1"), FDwShopRules::GetRecruitLevel(State, C.EconomyRow()), 2);
			Hand(1).bAlive = false;
			TestEqual(TEXT("The dead don't count: floor(7 / 2) - 1"), FDwShopRules::GetRecruitLevel(State, C.EconomyRow()), 2);
			C.EconomyRow().RecruitLevelOffset = 0;
			TestEqual(TEXT("Offset from tuning"), FDwShopRules::GetRecruitLevel(State, C.EconomyRow()), 3);
		});

		It("adds a crew member at full Health for their level with a seeded id, and a disband offer for them", [this]()
		{
			Captain().Level = 4; Hand(0).Level = 4; Hand(1).Level = 4;
			Open();
			const FDwShopOffer* Recruit = First(EDwShopOfferType::Recruit);
			TestEqual(TEXT("Offer shows level 3"), Recruit->Level, 3);
			const int32 Disbands = Count(EDwShopOfferType::Disband);
			TestEqual(TEXT("Ok"), Buy(Recruit), EDwPurchaseResult::Ok);
			if (!TestEqual(TEXT("Four crew"), State.Crew.Num(), 4)) { return; }
			const FDwCrewMemberState& New = State.Crew.Last();
			TestTrue(TEXT("Deckhand"), New.Unit.Get() == C.Deckhand);
			TestEqual(TEXT("Level 3"), New.Level, 3);
			TestEqual(TEXT("80 + 2 x 10 HP"), New.Health, 100.f);
			TestTrue(TEXT("Not the captain, alive, nothing learned"), !New.bIsCaptain && New.bAlive && New.Abilities.IsEmpty() && New.Xp == 0);
			TestEqual(TEXT("Seeded serial"), State.LastCrewSerial, 1);
			TestEqual(TEXT("80 gold"), State.Gold, 420);
			TestEqual(TEXT("Sold out"), Can(State.Shop.Find(Recruit->OfferId)), EDwPurchaseResult::SoldOut);
			TestEqual(TEXT("A disband offer for the recruit"), Count(EDwShopOfferType::Disband), Disbands + 1);
			TestTrue(TEXT("For them"), First(EDwShopOfferType::Disband, [&New](const FDwShopOffer& O) { return O.CrewId == New.Id; }) != nullptr);
		});

		It("stop at MaxCrew living crew, the captain included", [this]()
		{
			C.Shop->RecruitPool = { C.Deckhand, C.CaptainUnit }; // two recruit offers
			C.Units->FindRow<FDwUnitTuningRow>(TEXT("Cap"), TEXT("Test"))->RecruitCost = 80;
			Open();
			TestEqual(TEXT("Two recruits"), Count(EDwShopOfferType::Recruit), 2);
			TestEqual(TEXT("Fourth"), Buy(First(EDwShopOfferType::Recruit)), EDwPurchaseResult::Ok);
			const FDwShopOffer* Second = First(EDwShopOfferType::Recruit, [](const FDwShopOffer& O) { return !O.bSoldOut; });
			TestEqual(TEXT("Fifth: CrewFull"), Can(Second), EDwPurchaseResult::CrewFull);
			Hand(0).bAlive = false;
			TestEqual(TEXT("The dead free a place"), Can(Second), EDwPurchaseResult::Ok);
		});
	});

	Describe("Ship upgrades", [this]()
	{
		It("add their tuning bonuses once each: +50 max and current hull, +20 cannon damage, the extra cannon tag", [this]()
		{
			Open();
			auto Offer = [this](const UDwShipUpgradeDefinition* Upgrade)
			{
				return First(EDwShopOfferType::ShipUpgrade, [Upgrade](const FDwShopOffer& O) { return O.Upgrade == Upgrade; });
			};
			TestEqual(TEXT("Hull"), Buy(Offer(C.Hull)), EDwPurchaseResult::Ok);
			TestEqual(TEXT("Max 300"), State.Ship.MaxHealth, 300.f);
			TestEqual(TEXT("Current 200"), State.Ship.Health, 200.f);
			TestEqual(TEXT("Shot"), Buy(Offer(C.Shot)), EDwPurchaseResult::Ok);
			TestEqual(TEXT("Cannons 90"), State.Ship.CannonDamage, 90.f);
			TestEqual(TEXT("Enough left for the third (500 - 300)"), Can(Offer(C.Cannon)), EDwPurchaseResult::Ok);
			TestEqual(TEXT("Cannon"), Buy(Offer(C.Cannon)), EDwPurchaseResult::Ok);
			TestTrue(TEXT("Extra cannon tag"), State.Ship.GetUpgradeTags().HasTagExact(DwTags::Ship_Upgrade_ExtraCannon));
			TestEqual(TEXT("Three upgrades"), State.Ship.Upgrades.Num(), 3);
			TestEqual(TEXT("50 gold left"), State.Gold, 50);
			TestEqual(TEXT("Sold out"), Can(Offer(C.Hull)), EDwPurchaseResult::SoldOut);

			Open(7);
			TestEqual(TEXT("A new shop offers none of them again"), Count(EDwShopOfferType::ShipUpgrade), 0);
		});

		It("sell out when the ship has the upgrade already, however it got it", [this]()
		{
			Open();
			const FDwShopOffer* Hull = First(EDwShopOfferType::ShipUpgrade, [this](const FDwShopOffer& O) { return O.Upgrade == C.Hull; });
			State.Ship.Upgrades.Add(C.Hull);
			TestEqual(TEXT("Owned"), Can(Hull), EDwPurchaseResult::SoldOut);
		});

		It("unlock the deck tiles that need their tag: the third cannon (ASCII 'U')", [this]()
		{
			const FDwDeckSource Deck = FDwDeckAscii::Parse({ TEXT("SU.E"), TEXT("SC.E") });
			FDwGridModel Grid = FDwBoardBuilder::BuildSingleDeck(Deck, EDwSide::Player);
			auto Cannons = [&Grid]() { return static_cast<int32>(Algo::CountIf(Grid.Tiles, [](const FDwTile& T) { return T.Tags.HasTagExact(DwTags::Tile_Cannon); })); };
			TestTrue(TEXT("Round trip"), FDwDeckAscii::Export(Deck) == TArray<FString>{ TEXT("SU.E"), TEXT("SC.E") });
			TestEqual(TEXT("Without the upgrade: one cannon"), Cannons(), 1);
			TestEqual(TEXT("Other side's tags unlock nothing"), FDwBoardBuilder::ApplyShipUpgrades(Grid, EDwSide::Enemy, FGameplayTagContainer(DwTags::Ship_Upgrade_ExtraCannon)), 0);
			TestEqual(TEXT("No tags, nothing"), FDwBoardBuilder::ApplyShipUpgrades(Grid, EDwSide::Player, FGameplayTagContainer()), 0);
			TestEqual(TEXT("With it: one unlocked"), FDwBoardBuilder::ApplyShipUpgrades(Grid, EDwSide::Player, FGameplayTagContainer(DwTags::Ship_Upgrade_ExtraCannon)), 1);
			TestEqual(TEXT("Two cannons"), Cannons(), 2);
		});
	});

	Describe("Disband", [this]()
	{
		It("removes the crew member for nothing back (P8), but keeps the last one beside the captain", [this]()
		{
			Open();
			const FGuid First0 = Hand(0).Id;
			const FDwShopOffer* Leave0 = First(EDwShopOfferType::Disband, [First0](const FDwShopOffer& O) { return O.CrewId == First0; });
			const FDwShopOffer* Leave1 = First(EDwShopOfferType::Disband, [First0](const FDwShopOffer& O) { return O.CrewId != First0; });
			TestEqual(TEXT("Ok"), Buy(Leave0), EDwPurchaseResult::Ok);
			TestNull(TEXT("Gone from the roster"), State.FindCrew(First0));
			TestEqual(TEXT("Captain and one hand"), State.Crew.Num(), 2);
			TestEqual(TEXT("No refund"), State.Gold, 500);
			TestEqual(TEXT("Sold out"), Can(State.Shop.Find(Leave0->OfferId)), EDwPurchaseResult::SoldOut);
			TestEqual(TEXT("The last crew member stays"), Can(State.Shop.Find(Leave1->OfferId)), EDwPurchaseResult::CantDisband);
		});

		It("never disbands the captain, pays DisbandRefund when tuned, and sells out for the dead", [this]()
		{
			C.EconomyRow().DisbandRefund = 15;
			Open();
			FDwShopOffer& Captain0 = State.Shop.Offers.AddDefaulted_GetRef();
			Captain0.OfferId = State.Shop.Offers.Num() - 1;
			Captain0.Type = EDwShopOfferType::Disband;
			Captain0.CrewId = Captain().Id;
			TestEqual(TEXT("Captain"), Can(&State.Shop.Offers.Last()), EDwPurchaseResult::CantDisband);
			const FDwShopOffer* Leave = First(EDwShopOfferType::Disband);
			TestEqual(TEXT("Refund shown"), Leave->Amount, 15);
			TestEqual(TEXT("Ok"), Buy(Leave), EDwPurchaseResult::Ok);
			TestEqual(TEXT("Refunded"), State.Gold, 515);
			const FDwShopOffer* Other = First(EDwShopOfferType::Disband, [this](const FDwShopOffer& O) { return O.CrewId == Hand(0).Id; });
			Hand(0).bAlive = false;
			TestEqual(TEXT("Dead: gone already"), Can(Other), EDwPurchaseResult::SoldOut);
		});
	});

	Describe("Gold and offers", [this]()
	{
		It("never overspends: every paid offer is NotEnoughGold below its price, and nothing changes", [this]()
		{
			Open();
			State.Gold = 10;
			const FDwRunState Before = State;
			TestEqual(TEXT("Repair"), Buy(First(EDwShopOfferType::Repair)), EDwPurchaseResult::NotEnoughGold);
			TestEqual(TEXT("Item"), Buy(First(EDwShopOfferType::Item)), EDwPurchaseResult::NotEnoughGold);
			TestEqual(TEXT("Ability"), Buy(ForceAbilityOffer(C.Parry), Hand(0).Id), EDwPurchaseResult::NotEnoughGold);
			TestEqual(TEXT("Recruit"), Buy(First(EDwShopOfferType::Recruit)), EDwPurchaseResult::NotEnoughGold);
			TestEqual(TEXT("Upgrade"), Buy(First(EDwShopOfferType::ShipUpgrade)), EDwPurchaseResult::NotEnoughGold);
			TestEqual(TEXT("Gold untouched"), State.Gold, 10);
			TestEqual(TEXT("Hull untouched"), State.Ship.Health, Before.Ship.Health);
			TestEqual(TEXT("Crew untouched"), State.Crew.Num(), Before.Crew.Num());
			TestEqual(TEXT("Nothing carried"), State.CountItems(), 0);
			TestEqual(TEXT("Disband is free"), Can(First(EDwShopOfferType::Disband)), EDwPurchaseResult::Ok);

			State.Gold = 100;
			TestEqual(TEXT("Exactly the price"), Buy(First(EDwShopOfferType::Repair)), EDwPurchaseResult::Ok);
			TestEqual(TEXT("Down to 0, never below"), State.Gold, 0);
		});

		It("reports the room problem before the gold one", [this]()
		{
			Open();
			State.Gold = 0;
			State.Ship.Health = State.Ship.MaxHealth;
			TestEqual(TEXT("Full hull, no gold"), Can(First(EDwShopOfferType::Repair)), EDwPurchaseResult::ShipFull);
		});

		It("refuses unknown offers and a closed shop", [this]()
		{
			Open();
			TestEqual(TEXT("No such offer"), Can(nullptr), EDwPurchaseResult::NoOffer);
			const FDwShopOffer Repair = *First(EDwShopOfferType::Repair);
			State.Shop.Node = INDEX_NONE;
			TestEqual(TEXT("Closed"), Can(&Repair), EDwPurchaseResult::NoOffer);
			FDwShopContext NoEconomy;
			State.Shop.Node = 3;
			TestEqual(TEXT("No economy row"), FDwShopRules::CanPurchase(State, Repair.OfferId, FGuid(), INDEX_NONE, NoEconomy), EDwPurchaseResult::NoOffer);
		});

		It("reaches every EDwPurchaseResult value", [this]()
		{
			// The cases above, replayed in one test so the coverage is checked in one place.
			Open();
			Seen.Reset();
			Can(nullptr);                                                                    // NoOffer
			const FDwShopOffer* Parry = ForceAbilityOffer(C.Parry);
			Buy(Parry, Hand(0).Id);                                                          // Ok
			Can(State.Shop.Find(Parry->OfferId), Hand(1).Id);                                // SoldOut
			Can(ForceAbilityOffer(C.Lantern), Hand(0).Id);                                   // InvalidTarget (not in the deckhand pool)
			Can(ForceAbilityOffer(C.Cutlass), Hand(0).Id);                                   // AlreadyKnown
			Hand(1).Abilities = { C.Rigging, C.Musket };
			Can(State.Shop.Find(ForceAbilityOffer(C.Lantern)->OfferId), Captain().Id);       // Ok (captain's pool)
			Can(ForceAbilityOffer(C.Rigging), Hand(1).Id);                                  // AlreadyKnown (Rigging)
			{
				FDwShopOffer& Extra = State.Shop.Offers.AddDefaulted_GetRef();
				Extra.OfferId = State.Shop.Offers.Num() - 1;
				Extra.Type = EDwShopOfferType::Ability;
				Extra.Ability = C.Parry;
				Extra.Price = 50;
				Can(&State.Shop.Offers.Last(), Hand(1).Id);                                 // NoSlot
			}
			Buy(First(EDwShopOfferType::Disband, [this](const FDwShopOffer& O) { return O.CrewId == Hand(0).Id; })); // Ok
			Can(First(EDwShopOfferType::Disband, [](const FDwShopOffer& O) { return !O.bSoldOut; }));            // CantDisband
			Buy(First(EDwShopOfferType::Recruit));                                           // Ok (three crew again)
			State.Crew.Add(Member(C.Deckhand));
			FDwShopRules::Refresh(State, Context());
			{
				FDwShopOffer& Extra = State.Shop.Offers.AddDefaulted_GetRef();
				Extra.OfferId = State.Shop.Offers.Num() - 1;
				Extra.Type = EDwShopOfferType::Recruit;
				Extra.Unit = C.Deckhand;
				Extra.Price = 80;
				Can(&State.Shop.Offers.Last());                                              // CrewFull
			}
			State.Ship.Health = State.Ship.MaxHealth;
			Can(First(EDwShopOfferType::Repair));                                            // ShipFull
			State.AddItem(C.Grog, 6, 6);
			Can(First(EDwShopOfferType::Item));                                              // InventoryFull
			State.Gold = 0;
			Can(First(EDwShopOfferType::ShipUpgrade));                                       // NotEnoughGold

			const UEnum* Enum = StaticEnum<EDwPurchaseResult>();
			for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index) // the last entry is the generated _MAX
			{
				const EDwPurchaseResult Value = static_cast<EDwPurchaseResult>(Enum->GetValueByIndex(Index));
				TestTrue(FString::Printf(TEXT("Reached %s"), *Enum->GetNameStringByIndex(Index)), Seen.Contains(Value));
			}
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
