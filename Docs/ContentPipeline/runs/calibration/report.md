# Evaluator calibration - PASS

Generic pirate control (`Tools/ContentPipeline/calibration/generic-pirate.json`) vs 4 shipped events (`Tuning/Events.json`). Agent cost $0.23.

- PASS generic control can't be accepted: 52 < 85 and held under the stranger cap (69)
- PASS generic control fails the stranger test: specificity 18 < 40
- PASS shipped specificity beats the control by 30+ (70 vs 18)
- PASS shipped events outscore the control (67 vs 52)

| Set | Score | rules | text_data | design | voice | specificity |
|---|---|---|---|---|---|---|
| control | **52** | 82 | 72 | 45 | 58 | 18 |
| shipped | **67** | 90 | 84 | 73 | 85 | 70 |

## Control

52/100. Dimensions (weighted 20/15/20/20/25): rules 82 - Every outcome, requirement, selector and number is in the facts and inside the guardrails: Gold +100/+120/-60/-30, CrewHealth -20/-25, ShipHealth -50/-20, and MinGold 60 gates Gold -60. The fiction is where it breaks the setting. Mutiny says "The crew has gone too long without pay", but there is no wage system, and "a few hands linger near the weapons locker" invents a crowd of sailors that S05 rules out (a crew of 3-4 who are named units). MerchantGalleon's "merchant galleon" with a "tired" crew is a civilian trader, not the rival pirate crews S08 says fill the water. | text_data 72 - V07: BuriedTreasure's body says "The island is a day's sail off course", but "Follow the map" has no cost of any kind, so the detour the text sets up never happens. Mutiny "Make an example" applies CrewHealth -25 with the Random selector, which can land on the captain, yet "Order returns to the ship, for now." never says who was punished. A captain making an example of himself reads false. MerchantGalleon "silk and silver" pays out only Gold, which is acceptable. Storm's text matches its data ("sails are torn" for -50 ship, "lose cargo overboard" for -30 gold). | design 45 - S02 fails in all four events: no choice is quick-and-safe against slow-and-rich. BuriedTreasure gives +100 gold for nothing against Nothing, so "Follow the map" is strictly dominant. MerchantGalleon trades +120 gold for -20 crew HP on one random pirate, which is near-dominant at any point in a run. S03: Storm's two choices both spend the ship bar. -50 ShipHealth is worth the same as -20 ShipHealth plus -30 Gold at the repair rate (100 gold for 100 HP), so the choice is a coin flip of equal costs and never asks you to save the hull or the crew. Mutiny is the only real trade (60 gold vs 25 crew HP) and is the weakest copy of shipped Mutterings Below Deck. The S07 valence split of 2 Good and 2 Bad matches the brief. | voice 58 - V01 present tense is met. V04 is clean and there are no A02 caps or exclamation marks. V03 is missing: there is no dry gallows humour, and two results end on the flat stock line "Nothing happens." ("The bottle bobs away. Nothing happens." / "You watch her sails shrink. Nothing happens."). A03: "X Marks the Spot" is a stock idiom that gives away the payout, and "A Fat Merchant" leans the same way. Fit: Storm's "You outrun the worst of it but lose cargo overboard." echoes shipped Squall Line's "You outrun the worst of it" almost word for word, and Mutiny's "Whispers spread on the lower deck" / "the whispers stop" restages Mutterings Below Deck. | specificity 18 - None of the four events passes the stranger test, and code found zero signature terms or mechanics. Rename nothing and all four run unchanged in any pirate game: a treasure map in a bottle, a fat merchant prize, a mutiny over pay, a storm. Nothing in them touches sink vs plunder (S02), spending ship against crew (S03), permanent crew loss (S04), the rival crews racing for the Kraken (S08), the Kraken itself (S06), or any captain (C01-C03). Aquadia is never present (S01). The weakest line is "A torn map in a bottle washes against your hull. It shows a small island and a red X.". Base 52.3. Code checks: no deductions. Evaluator: This is a control set of generic pirate tropes, and none of the four events passes the stranger test. They have no sink-or-plunder tension and no ship-vs-crew trade, two are near-dominant free gold, and two repeat shipped events.

### X Marks the Spot  `DA_Event_BuriedTreasure` - 32/100

_Free +100 gold against Nothing is a strictly dominant choice with no S02 tension. The "day's sail off course" detour is never charged (V07). The title is a stock idiom that names the payout (A03)._

```
Title:    X Marks the Spot
Valence:  Good      Layers: 1-9        Weight: 1
Body:     A torn map in a bottle washes against your hull. It shows a small island and a red X. The island is a day's sail off course.
Choice 1: Follow the map  -> Outcomes: Gold +100  -> Result: You dig until the shovel hits wood. The chest is full of old coins.
Choice 2: Toss it back  -> Outcomes: Nothing  -> Result: The bottle bobs away. Nothing happens.
```

### A Fat Merchant  `DA_Event_MerchantGalleon` - 40/100

_+120 gold for -20 HP on one random pirate is near-dominant. A civilian merchant breaks S08 (the people on the water are rival pirate crews). The boarding is never framed as a sink-or-plunder call (S02)._

```
Title:    A Fat Merchant
Valence:  Good      Layers: 1-9        Weight: 1
Body:     A heavy merchant galleon wallows on the horizon, low in the water with cargo. Her guns are few and her crew looks tired.
Choice 1: Board her  -> Outcomes: Gold +120; CrewHealth -20 (Random)  -> Result: The fight is short. The hold is full of silk and silver.
Choice 2: Let her pass  -> Outcomes: Nothing  -> Result: You watch her sails shrink. Nothing happens.
```

### Grumbling Crew  `DA_Event_Mutiny` - 40/100

_It is the only real trade (gold vs crew HP), but it restages shipped Mutterings Below Deck. The pay mechanic and the crowd of "hands" don't exist (S05). The Random selector can make the captain the example._

```
Title:    Grumbling Crew
Valence:  Bad       Layers: 1-9        Weight: 1
Body:     The crew has gone too long without pay. Whispers spread on the lower deck and a few hands linger near the weapons locker.
Choice 1: Pay them off  -> Requires: MinGold 60 -> Outcomes: Gold -60  -> Result: Coins change hands and the whispers stop.
Choice 2: Make an example  -> Outcomes: CrewHealth -25 (Random)  -> Result: Order returns to the ship, for now.
```

### A Gathering Storm  `DA_Event_Storm` - 38/100

_Both choices hit the ship and cost the same at the repair rate, so it is a non-decision with no ship-vs-crew trade (S03). The result echoes Squall Line's "You outrun the worst of it"._

```
Title:    A Gathering Storm
Valence:  Bad       Layers: 1-9        Weight: 1
Body:     Black clouds pile up to the west and the wind turns cold. The sea starts to heave.
Choice 1: Ride it out  -> Outcomes: ShipHealth -50  -> Result: The storm batters the ship all night. By morning the sails are torn.
Choice 2: Run before the wind  -> Outcomes: ShipHealth -20; Gold -30  -> Result: You outrun the worst of it but lose cargo overboard.
```

## Shipped

67/100. Dimensions (weighted 20/15/20/20/25): rules 90 - Every outcome, requirement, item (HealthRestore), unit (Swordsman, Sniper), captain (Conqueror) and number exists in the facts and stays inside the guardrails: SquallLine -70 ship equals shipLossMax, Mutterings Captain -50 equals crewDeltaMax, TallyRock +120 gold is under 150 and 80 XP equals xpMax. Captain -50 floors at 1, so no event can kill. The ungated Recruit choices are already deducted by code. Weakest remaining rules point: LoseCrew Random in MutteringsBelowDeck choices[2] has no CrewCountAtLeast gate, so on a captain-only crew that branch silently does nothing (S04). | text_data 84 - Most text maps to data (V07). TallyRock 'pocket half the coin' matches Gold +60 against the strongbox's +120. SquallLine's single RandomOf result 'Whatever the sea took, it took fast' reads true for both -10 and -70. Weakest line: the DesertersLongboat label 'Trade rations for medicine' promises rations, but the data spends only Gold -20. The result half-admits this ('a sack of hardtack and a few coins'), so the label still misleads. Mutterings 'By morning, the crew has settled it their own way' is true in both branches but hides a permanent LoseCrew (see voice/S04). 'Whether there's a bunk to spare is another matter' admits the full-crew no-op but doesn't make it true. | design 73 - SquallLine and Mutterings are strong. SquallLine makes you choose between the hull and the crew (S03): ship -45, crew -20 All plus ship -15, or a 50/50 of ship -10/-70. Mutterings sets gold against the captain's health against a 1-in-3 permanent loss, and both Bad events offer a choice that limits the damage (S07). The two Good events have no sink-or-plunder tension (S02). TallyRock is three free gains: 'Open the strongbox' (+120 gold) or 'Read the notes aloud' (60 XP All), with no cost or risk anywhere. DesertersLongboat sets a free recruit (shop value 80) against 2 HealthRestore for 20 gold (shop value 60). With a crew of 3 the recruit nearly always wins, and with a crew of 4 it is a trap (already deducted). | voice 85 - The voice is dry and concrete and matches the reference lines (V02, V03): 'Old coin, still dry. Whoever left it isn't coming back for it.' and 'Coin goes around the deck and the muttering stops. For now.' Weakest line: Mutterings choices[1].resultText 'The captain takes the ringleader on in front of everyone' drops into third person and breaks V01's 'you'. The Mutterings LoseCrew branch never names who goes (S04's 'lets it land'). SquallLine's 'You have a minute to decide how to take it.' is flat filler, and 'until their hands bleed' repeats the shipped SomethingUnderTheKeel line. | specificity 70 - Uneven. TallyRock is built on this game: crews that sailed on to the Kraken (S06), the rival crews' run (S08), and a Conqueror gate that plays to C03 ('You'll be the one ending it'). DesertersLongboat leans on the Swordsman/Sniper roster and the 4-crew cap (S05) plus rival-ship deserters (S08). Weakest: SquallLine ('The horizon goes black from end to end.') and Mutterings ('Too many fights and too few shares have soured the crew.') are a storm and a mutiny from any pirate game. Only the ship/crew split (S03) and the unnamed LoseCrew (S04) tie them to Deadwater Run.. Base 79.7. Code checks -13 (2 major x6, 1 minor x1). Evaluator: The voice is strong, but SquallLine and MutteringsBelowDeck are a stock storm and a stock mutiny, and both Good events lack sink-or-plunder tension (S02). That holds specificity at 70 and design at 73.

### Deserters in a Longboat  `DA_Event_DesertersLongboat` - 64/100

_Clean scene and voice ('a long rifle wrapped in sailcloth'). The recruit dominates the medicine trade on a crew of 3 and is a trap at 4 (code). The rations label doesn't match the gold cost. No sink-or-plunder tension (S02)._

```
Title:    Deserters in a Longboat
Valence:  Good      Layers: 4-6        Weight: 1
Body:     A longboat bumps against your hull. The sailors in it walked off a rival ship two nights ago and haven't eaten since. One still has a sword and another has a long rifle wrapped in sailcloth.
Choice 1: Wave the swordsman aboard  -> Outcomes: Recruit Swordsman  -> Result: You wave the swordsman over the rail. Whether there's a bunk to spare is another matter.
Choice 2: Wave the marksman aboard  -> Outcomes: Recruit Sniper  -> Result: You wave the marksman over the rail. Whether there's a bunk to spare is another matter.
Choice 3: Trade rations for medicine  -> Requires: MinGold 20 -> Outcomes: Gold -20; GiveItem HealthRestore x2  -> Result: They give up two stolen medicine kits for a sack of hardtack and a few coins. Nobody asks whose they were.
```

### Squall Line  `DA_Event_SquallLine` - 73/100

_A real hull-vs-crew choice (S03), and the RandomOf text works for both branches. The scene itself is a stock storm with flat filler ('You have a minute to decide how to take it'), and the 'hands bleed' line repeats the shipped pool._

```
Title:    Squall Line
Valence:  Bad       Layers: 4-6        Weight: 1
Body:     The horizon goes black from end to end. The wind turns against you, and the first rain hits the deck sideways. You have a minute to decide how to take it.
Choice 1: Run before the wind  -> Outcomes: ShipHealth -45  -> Result: You outrun the worst of it, but the hull takes a beating from the waves on the way.
Choice 2: Reef sails and hold  -> Outcomes: CrewHealth -20 (All); ShipHealth -15  -> Result: Everyone works the rigging until their hands bleed. The ship mostly holds.
Choice 3: Drive straight through  -> Outcomes: RandomOf [ 1: ShipHealth -10 | 1: ShipHealth -70 ]  -> Result: You put the bow into the black and hold on. Whatever the sea took, it took fast.
```

### Mutterings Below Deck  `DA_Event_MutteringsBelowDeck` - 70/100

_Good three-way trade of gold, captain HP and a permadeath gamble. The 'The captain takes...' line breaks V01, and the LoseCrew branch never names or lands the loss (S04). The mutiny premise is generic._

```
Title:    Mutterings Below Deck
Valence:  Bad       Layers: 4-6        Weight: 1
Body:     The talk stops whenever you go below. Too many fights and too few shares have soured the crew. By morning, somebody will act on it.
Choice 1: Pay out shares  -> Requires: MinGold 50 -> Outcomes: Gold -50  -> Result: Coin goes around the deck and the muttering stops. For now.
Choice 2: Settle it with fists  -> Outcomes: CrewHealth -50 (Captain)  -> Result: The captain takes the ringleader on in front of everyone and wins, barely. No one mutters after that.
Choice 3: Let it fester  -> Outcomes: RandomOf [ 2: CrewHealth -20 (All) | 1: LoseCrew (Random) ]  -> Result: You leave them to it and turn in. By morning, the crew has settled it their own way.
```

### The Tally Rock  `DA_Event_TheTallyRock` - 81/100

_The most specific event in the batch: Kraken foreshadowing without a fight (S06) and a Conqueror line true to C03. The data is consistent (half of 120 is 60). No choice costs or risks anything (S02/S07), and the result text runs 3 sentences (already deducted by code)._

```
Title:    The Tally Rock
Valence:  Good      Layers: 6-9        Weight: 1
Body:     A black rock stands alone in the swell, cut all over with names. Every crew that sailed on to the Kraken stopped here first. Under a cairn at its foot sits a strongbox and a page of notes, weighted with a stone.
Choice 1: Open the strongbox  -> Outcomes: Gold +120  -> Result: Old coin, still dry. Whoever left it isn't coming back for it.
Choice 2: Read the notes aloud  -> Outcomes: GrantXp 60 (All)  -> Result: Scrawled warnings about tentacles, timing and where to stand. The crew make you read it twice.
Choice 3: Keep the notes to yourself  -> Requires: CaptainIs Conqueror -> Outcomes: GrantXp 80 (Captain); Gold +60  -> Result: You read every page alone, then pocket half the coin and leave the rest under the cairn. The crew don't need to know how it ends. You'll be the one ending it.
```
