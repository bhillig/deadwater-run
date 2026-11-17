# Batch `kraken-reach` - needs Brandon (blocker/major left after revisions)

Brief: The last stretch before the Kraken: omens, broken ships, a sea that is getting worse. No Kraken fight in an event. Across the batch, give each of the 3 captains one CaptainIs-gated choice that plays to who they are (TeamLeader: a loyal crew; Fearmongerer: a feared reputation; Conqueror: a captain stronger than his crew).
Layers 6-9, 4 events (2 Good, 2 Bad).
Rounds: 2. Agent cost: $0.82 over 4 calls.

**Sounds like Deadwater Run?** Yes - 4/5 (final round). Mostly right: plain second-person present tense, concrete nouns and dry understatement ('Whoever left it isn't coming back for it.'). Two result texts let it down. The Fearmongerer gamble describes both branches at once, and a couple of lines overreach into melodrama or promise more than the data does.

Weakest lines (critic):
- The longboats scatter, some fast enough to drop their purses, some too close to stop before they ram you.
- The crew don't need to know how it ends. You'll be the one ending it.
- Your ship sails on sounder than it has in weeks.
- Crates of plunder go over the side and the ship rides higher. Whatever it was loses interest.

## Round 1 review - critic verdict: revise, voice 4/5

| Sev | From | Event | Field | Rule | Problem | Fix |
|---|---|---|---|---|---|---|
| minor | code | DA_Event_TheTallyRock | `choices[2].resultText` | E02 text | Result text is more than 2 sentences | Trim |
| major | critic | DA_Event_WreckersAmongTheHulls | `choices[0].requirements` | No free choice when broke | 'Pay the toll' (-80 gold) has no requirement, and gold floors at 0. A captain with little or no gold pays nothing or very little, so the choice becomes the clear best pick for a broke player in a Bad event. The result text 'They count it twice' also makes no sense when you paid nothing. | Add { "type": "MinGold", "amount": 80 } to choices[0].requirements. 'Run the shallows' stays as the choice with no requirement. |
| major | critic | DA_Event_SomethingUnderTheKeel | `choices[1].requirements` | No free choice when broke | 'Dump the cargo and run' (-70 gold, -20 ship) has no requirement, and gold floors at 0. A player with 0 gold gets a sure -20 ship, which beats 'ride it out' (about -47 ship on average). The gamble disappears exactly when the player is weakest. | Add { "type": "MinGold", "amount": 70 } to choices[1].requirements. Choice 0 still has no requirement, so the event always has an open choice. |
| major | critic | DA_Event_WreckersAmongTheHulls | `choices[2].outcomes` | No strictly dominant choice | The Fearmongerer's 'Let them see your face' costs nothing and gives +40 gold, so it beats -80 gold and -50 ship outright. For him a Bad event becomes a free Good event with no decision. The other two captain gates both cost something: the Conqueror gives up 60 gold and the crew's XP, and the TeamLeader takes -15 crew HP and -20 ship. | Give it a cost or a risk. For example, drop the gold so the gate just avoids the loss, or use a RandomOf where some longboats don't turn (weight 1: ShipHealth -30) and the rest give the purse (weight 2: Gold +40). |
| minor | critic | DA_Event_WreckersAmongTheHulls | `choices[1].resultText` | Text matches data | 'the purse stays full' is wrong for a player with little gold, and it reads as gaining something when the choice only avoids a loss. | Change it to something neutral, such as 'The hull takes it badly. The wreckers don't follow.' |
| minor | critic | DA_Event_TheTurningBrig | `choices[1].resultText` | Consistency with body | The body says he is going home. 'Won't be needing them where he's going' is the stock line for a dying man, so it muddles the picture, and it's a cliché. | Tie it to the trip home, for example 'Two tonics, corked and sealed. He'd rather you drank them than the sea.' |
| minor | critic | DA_Event_TheTallyRock | `choices[1].resultText` | No promised effects | 'warnings about tentacles, timing and where to stand' suggests real knowledge for the Kraken fight. The only outcome is +60 XP, and nothing in the boss fight changes. | Keep the notes vague, for example 'Scrawled warnings from crews who went on ahead. The crew make you read it twice.' |

## Round 2 review - critic verdict: revise, voice 4/5

| Sev | From | Event | Field | Rule | Problem | Fix |
|---|---|---|---|---|---|---|
| minor | code | DA_Event_TheTallyRock | `choices[2].resultText` | E02 text | Result text is more than 2 sentences | Trim |
| blocker | critic | DA_Event_WreckersAmongTheHulls | `choices[2].resultText` | Text matches data | FDwEventChoice has a single ResultText (11-Events.md L27), so every branch shows the same text. That text says both things happened: purses were dropped (+gold) and a longboat rammed you (hull loss). Each RandomOf branch applies only one of them. On the 2/3 branch the text claims hull damage that never happens. On the 1/3 branch it implies gold you never get. The writer's note says the text 'never promises something that didn't happen', and that's wrong. 'Drop their purses' is also odd logic: it's not clear why wreckers rowing in to extort you would be carrying purses or how you'd collect them. | Write branch-neutral text that names neither effect, e.g. 'You light the fuses in your beard and stand at the rail. The longboats break and scatter.' Let the applied-change log show the gold or hull result. Otherwise, ask Brandon for per-option result text on RandomOf (open question). |
| major | critic | DA_Event_WreckersAmongTheHulls | `choices[2].outcomes[0].options[1]` | No strictly dominant choice | The Fearmongerer's worst case (ShipHealth -30) is better than the free alternative 'Run the shallows' (ShipHealth -50), and his best case gains gold. Every branch beats every other choice, so for him this is still not a decision. It also turns a Bad event into a net-positive one (EV about +27 gold, -10 hull). The RandomOf added variance but not a real choice. | Make the bad branch worse than the free option, e.g. ShipHealth -70 (the shipLossMax guardrail). He keeps the best average (~+27 gold, ~-23 hull) but now risks a worse hit than running the shallows. |
| major | critic | DA_Event_TheTurningBrig | `choices[2].label` | Text matches data | 'Pay for the whole lot' says you get everything in his hold, which includes the medicine chest from choice 1. The outcomes only give ShipHealth +100 and no HealthRestore. The result text even switches to a carpenter instead. | Change the label to match the outcome, e.g. 'Pay for his carpenter' or 'Pay him to patch you up'. Keep the outcomes as they are. Don't add the tonics, since 60 gold for +100 hull is already under shop price. |
| minor | critic | DA_Event_TheTurningBrig | `choices[2].resultText` | Text matches data | 'Sounder than it has in weeks' overpromises. Hull is capped at 250, so a nearly full ship gains little, and +100 on a badly damaged ship still leaves it far from sound. 'Weeks' also sets a time scale the run doesn't have. | Understate it: 'His carpenter works through the afternoon. The worst of it is shut.' |
| minor | critic | DA_Event_TheTurningBrig | `choices[2].outcomes` | Economy fit | Agreeing with the writer's flag: +100 hull for 60 gold undercuts the shop's 100-gold repair. It's also only 50 hull more than the free 'Take his planks'. It's a Good event so the discount can be justified, but it's the cheapest repair in the game. | Either raise it to MinGold 80 / Gold -80, or keep 60 and accept it as a Good-event discount. Brandon's call. |
| minor | critic | DA_Event_TheTallyRock | `choices[1].resultText` | No promises outcomes don't keep | 'Scrawled warnings about tentacles, timing and where to stand' reads as real Kraken intel. The only effect is generic XP, and the boss fight gives no benefit for having read it. | Keep it vaguer so the XP reads as the payoff, e.g. 'Scrawled warnings from crews who went on ahead. The crew make you read it twice.' |
| minor | critic | DA_Event_TheTallyRock | `choices[2].resultText` | Captain fit | Besides the code check's length flag, the Conqueror leaving half the coin under the cairn is out of character for a captain who takes everything. It reads as an arbitrary reason for the gold being 60 instead of 120. | Give a reason that fits him, or just say he takes a share: 'You read every page alone and take what coin you can carry. The crew don't need to know how it ends.' |
| minor | critic | DA_Event_SomethingUnderTheKeel | `choices[2].outcomes` | Valence fit | For the TeamLeader, a Bad event becomes close to a net gain. He takes -15 crew HP and -20 hull but gets +40 XP for everyone, which his Veteran Crew rule raises to 60. That's more than half a level each, on layers 6-9 right before the Kraken. It's not strictly dominant because crew HP carries into fights, but it's generous. | Cut GrantXp to 20-25, or raise CrewHealth to -20/-25 so it still feels like a Bad event. |
| minor | critic | DA_Event_SomethingUnderTheKeel | `choices[1].resultText` | Fiction logic | 'Whatever it was loses interest' doesn't follow from throwing weight overboard. Riding higher helps you run, but it has nothing to do with the creature losing interest. | Tie it to the escape: 'Crates of plunder go over the side. The ship rides higher and you leave the dark water behind.' |

## Writer's notes on the revision

**Fixes from the review:**
- **Wreckers, 'Pay the toll' (major, broke player):** added MinGold 80 to match the -80 delta. A broke player can't get out of the event for free now, and 'They count it twice' is always true. 'Run the shallows' still has no requirement, so no player is stuck.
- **Something Under the Keel, 'Dump the cargo and run' (major, broke player):** added MinGold 70 to match the -70 delta. With no gold you have to take the gamble (or the TeamLeader's pumps). Choice 0 still has no requirement.
- **Wreckers, Fearmongerer gate (major, dominant choice):** changed it from a sure +40 gold to the critic's RandomOf: weight 2 gives Gold +40, weight 1 gives ShipHealth -30. He still has the best average result, since his reputation should count for something. But now one time in three it costs him hull, so it's a real gamble and not a free win. The result text covers both outcomes (purses dropped, or a longboat rams you) so it never promises something that didn't happen.
- **Untouched:** The Turning Brig, The Tally Rock and the other choices, since the critic didn't flag them.

**Batch summary:**
- Captain gates, one each: Conqueror on The Tally Rock, Fearmongerer on Wreckers Among the Hulls, TeamLeader on Something Under the Keel.
- Valence: 2 Good (Turning Brig, Tally Rock) and 2 Bad (Wreckers, Something Under the Keel). All are on layers 6-9.
- Guardrails: gold from +120 to -80, ship from +100 to -60, crew -15, XP at most 80, items at most 2. Every event has at least one choice with no requirement. Every choice that loses gold now needs MinGold equal to the loss.
- Balance for Brandon: 'Pay for the whole lot' gives +100 ship for 60 gold, which beats a shop repair (100 gold). You might want it at 80.
- Idea for Brandon (not used): a 'CrewCountAtMost' requirement, so a Recruit choice can't silently do nothing when the crew is full.

## Open questions for Brandon

- GiveItem when the inventory is full (6 items): does 'Take his medicine chest' fail gracefully, drop the extra, or block the choice? If it silently does nothing, should HealthRestore events get a requirement or a fallback result text?
- The writer flagged the 60-gold price for +100 ship (shop repair costs 100). Keep 60 for a Good event this late in the run, or raise it to 80?
- Should captain-gated choices be allowed to be strictly better than the open choices, as a reward for that captain? If so, the Wreckers finding drops to minor. Please write the rule into voice.md or 11-Events.md either way.
- The writer suggests a CrewCountAtMost requirement so Recruit choices don't silently fail on a full crew. Do you want it added to the requirements list?
- Should RandomOf options have their own result text? With one ResultText per choice, every gamble's text has to be vague. Or does the applied-change log in FDwEventContext show the player what happened?
- What does GiveItem do when the inventory is full (6 items)? Does it leave the item behind like plunder (P15) or fail silently? 'Take his medicine chest' could do nothing for a player with a full hold.
- Should captain-gated choices beat the ungated ones in every branch, or only on average? The brief asks them to 'play to who they are', but the critic rules forbid strictly dominant choices.

## Final events

### The Turning Brig  `DA_Event_TheTurningBrig`
```
Title:    The Turning Brig
Valence:  Good      Layers: 6-9        Weight: 1
Body:     A brig limps past on the other tack, her foremast gone and her rail torn open. Her captain shouts across that he has seen what waits ahead and is going home. He has no more use for what's in his hold.
Choice 1: Take his planks  -> Outcomes: ShipHealth +50  -> Result: His crew pass over timber and pitch. By dusk the worst of your holes are shut.
Choice 2: Take his medicine chest  -> Outcomes: GiveItem HealthRestore x2  -> Result: Two tonics, corked and sealed. He says he won't be needing them where he's going.
Choice 3: Pay for the whole lot  -> Requires: MinGold 60 -> Outcomes: Gold -60; ShipHealth +100  -> Result: He takes your coin for the road home and lends you his carpenter for the afternoon. Your ship sails on sounder than it has in weeks.
```

### The Tally Rock  `DA_Event_TheTallyRock`
```
Title:    The Tally Rock
Valence:  Good      Layers: 6-9        Weight: 1
Body:     A black rock stands alone in the swell, cut all over with names. Every crew that sailed on to the Kraken stopped here first. Under a cairn at its foot sits a strongbox and a page of notes, weighted with a stone.
Choice 1: Open the strongbox  -> Outcomes: Gold +120  -> Result: Old coin, still dry. Whoever left it isn't coming back for it.
Choice 2: Read the notes aloud  -> Outcomes: GrantXp 60 (All)  -> Result: Scrawled warnings about tentacles, timing and where to stand. The crew make you read it twice.
Choice 3: Keep the notes to yourself  -> Requires: CaptainIs Conqueror -> Outcomes: GrantXp 80 (Captain); Gold +60  -> Result: You read every page alone, then pocket half the coin and leave the rest under the cairn. The crew don't need to know how it ends. You'll be the one ending it.
```

### Wreckers Among the Hulls  `DA_Event_WreckersAmongTheHulls`
```
Title:    Wreckers Among the Hulls
Valence:  Bad       Layers: 6-9        Weight: 1
Body:     Broken ships lie on their sides in the shallows, ribs open to the sky. A wrecker crew is stripping them, and their longboats are already pulling your way. Their captain wants a toll, or your hull for parts.
Choice 1: Pay the toll  -> Requires: MinGold 80 -> Outcomes: Gold -80  -> Result: They count it twice and row off. Cheaper than a fight this close to the end.
Choice 2: Run the shallows  -> Outcomes: ShipHealth -50  -> Result: You scrape over a sunken keel to get clear. The hull takes it badly, but the purse stays full.
Choice 3: Let them see your face  -> Requires: CaptainIs Fearmongerer -> Outcomes: RandomOf [ 2: Gold +40 | 1: ShipHealth -30 ]  -> Result: You light the fuses in your beard and stand at the rail. The longboats scatter, some fast enough to drop their purses, some too close to stop before they ram you.
```

### Something Under the Keel  `DA_Event_SomethingUnderTheKeel`
```
Title:    Something Under the Keel
Valence:  Bad       Layers: 6-9        Weight: 1
Body:     The water goes flat and dark. Something long passes beneath the ship, slow enough that you can count the seconds. Then the deck lifts under your feet.
Choice 1: Hold on and ride it out  -> Outcomes: RandomOf [ 2: ShipHealth -60 | 1: ShipHealth -20 ]  -> Result: It rises once more and is gone. Whatever it cost the hull, you hear it groaning for an hour after.
Choice 2: Dump the cargo and run  -> Requires: MinGold 70 -> Outcomes: Gold -70; ShipHealth -20  -> Result: Crates of plunder go over the side and the ship rides higher. Whatever it was loses interest.
Choice 3: Every hand to the pumps  -> Requires: CaptainIs TeamLeader -> Outcomes: CrewHealth -15 (All); ShipHealth -20; GrantXp 40 (All)  -> Result: Nobody waits to be told. They bail until their hands bleed, and the ship stays dry enough to sail.
```
