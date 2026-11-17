# Batch `deep-water` - needs Brandon (blocker/major left after revisions)

Brief: Mid-run. What rival crews leave behind, bad weather, a crew under strain. Costs bite harder now. At least one event touches the crew roster (Recruit or LoseCrew) and at least one is a gamble (RandomOf).
Layers 4-6, 4 events (2 Good, 2 Bad).
Rounds: 2. Agent cost: $0.83 over 4 calls.

**Sounds like Deadwater Run?** Yes - 4/5 (final round). The writing is mostly plain and physical, in second person and present tense, with dry understatement ('For now.', 'wins, barely.'). Two lines are hedges, two lines repeat each other, and two RandomOf texts reuse the same 'the sea' wording.

Weakest lines (critic):
- Whether there's a bunk to spare is another matter.
- You wave the swordsman over the rail. / You wave the marksman over the rail.
- The sea decides the rest.
- Whatever the sea took, it took fast.
- The sailors in it walked off a rival ship two nights ago

## Round 1 review - critic verdict: revise, voice 4/5

| Sev | From | Event | Field | Rule | Problem | Fix |
|---|---|---|---|---|---|---|
| major | critic | DA_Event_RivalsCache | `choices[2]` | No strictly dominated choice | 'Haul it up fast' gives a sure +60 gold with no cost and no requirement. 'Leave their mark alone' gives nothing, and the game has no state where nothing beats free gold. So 'Leave' is a dead button, not a decision. | Give 'Haul it up fast' a small real cost so leaving it can be the right call. One option is a small RandomOf with a minor ShipHealth or CrewHealth hit. Another is to swap 'Leave' for a choice with its own trade-off. Keep the 'Bring up the whole chest' gamble as it is (EV is about +78 gold-equivalent against the sure +60, which is a fair gamble). |
| major | critic | DA_Event_DesertersLongboat | `choices[1].outcomes` | No trap choices / text matches data | With a full crew of 4, 'Pay the marksman's price' still takes the 40 gold and Recruit does nothing. It's a pure loss in a Good event. The result text admits this, but the button stays enabled and the label still promises a marksman. Requirements can't gate on 'crew not full'. CrewCountAtLeast is the only crew requirement, and the writer is right that there's no AtMost. | Remove the Gold -40 so the choice is a free pick between Swordsman and Sniper, and put the cost somewhere that can't go to waste. Or hold this event until Brandon adds a 'crew not full' requirement (see open questions). |
| major | critic | DA_Event_MutteringsBelowDeck | `choices[1].outcomes[0]` | No strictly dominant choice / costs bite harder | 'Settle it with fists' costs 30 captain HP with no requirement, and event damage floors at 1. A HealthRestore costs 30 gold at shops, so the fight costs about 30 gold or less. 'Pay out shares' costs 60 gold, so in practice it's never the right pick. If crew HP refills between nodes, the fight is almost free, which also undercuts the brief's 'costs bite harder now'. | Raise the fight cost toward the guardrail (CrewHealth -50 on Captain) or cut Pay to about 30-40 gold, so the two sure options cost roughly the same in different currencies. |
| major | critic | DA_Event_RivalsCache | `choices[1].resultText` | Voice: result text for RandomOf | FDwEventChoice has one ResultText per choice, so the writer has listed both branches with 'Either... or...'. After the roll, the player reads a hedge instead of what happened, and it breaks the present-tense, concrete voice. The same pattern is in SquallLine choices[2].resultText and MutteringsBelowDeck choices[2].resultText, and DesertersLongboat uses it for full-crew cases ('If your deck is already full...'). | Write one line that's true for every branch and leave the specifics to the change log. For example, for the chest: 'You get a hook on the chest and start hauling. The sea decides the rest.' For the squall, cut 'You find either a gap in the storm or the very worst of it.' and keep 'Whatever the sea took, it took fast.' Do the same for Mutterings and the Recruit texts. |
| minor | critic | DA_Event_DesertersLongboat | `choices[2].requirements` | Requirement matches cost | 'Trade rations for medicine' takes 20 gold but has no MinGold requirement. Gold floors at 0, so a broke player gets two HealthRestore for free while the result text says they paid 'a few coins'. | Add { "type": "MinGold", "amount": 20 }, matching how choices[1] gates its 40 gold. |
| minor | critic | DA_Event_MutteringsBelowDeck | `choices[1].resultText` | Voice: second person | 'The captain takes the ringleader on in front of everyone and wins, barely.' The player is the captain, and the voice sheet asks for second person. | 'You take the ringleader on in front of everyone and win, barely. No one mutters after that.' |
| minor | critic | DA_Event_RivalsCache | `choices[1].outcomes[0].options[1]` | Fit: rival crew without a fight | In the bad branch, the cache's owners 'show up' and fire a broadside, then the event ends with no fight. That's fine mechanically, but it's an odd beat in a game where meeting a rival crew means a fight. | Optional: make the damage the cache's own trap or the strain of hauling it (e.g. a fouled line that tears into the hull) rather than the rival crew arriving. |

## Round 2 review - critic verdict: revise, voice 4/5

| Sev | From | Event | Field | Rule | Problem | Fix |
|---|---|---|---|---|---|---|
| major | critic | DA_Event_MutteringsBelowDeck | `choices[2].outcomes[0].options` | No trap choice | 'Let it fester' is worse than 'Settle it with fists' in nearly every crew state. In 2/3 of rolls it costs -20 to every crewmate, which is 60-80 total HP for a crew of 3-4 and more than the fists' -50. In the other 1/3 it permanently kills a crewmate. Neither branch has an upside, so the gamble only pays off with a solo captain. HP carries between fights (C9, 03-Combat-Turns.md), so the damage is real. | Give the gamble a cheap branch so it competes with the sure costs. For example: weight 2 → CrewHealth -10 on Random (a scuffle), weight 1 → LoseCrew Random. Keep the crew-loss branch, which covers the brief's LoseCrew requirement. |
| major | critic | DA_Event_DesertersLongboat | `choices[0], choices[1]` | No dead buttons / text matches data | At layers 4-6 a full crew of 4 is a common state, because shop recruits are 80 gold. With a full crew, two of the three choices do nothing. The player can still click them, and 'Whether there's a bunk to spare is another matter' hides the no-op instead of saying it. Facts list no CrewCountAtMost requirement, so this can't be gated today. The writer flagged this as open but shipped it anyway. | Either hold the event until Brandon adds a crew-not-full requirement, or restructure within the current rules. Merge to one recruit choice and give it a second outcome that still lands when the crew is full, for example taking the deserters' kit via GiveItem HealthRestore 1. That frees a slot for a different choice that works at 4 crew. |
| minor | critic | DA_Event_RivalsCache | `choices[0].label` | Text matches body | 'Cut the purse free' brings in a purse the body never mentions. The body only sets up a barrel, a taut line and sunken plunder, so the player can't tell how a purse differs from 'the whole chest'. | Set the purse up in the body, for example 'A purse is lashed to the line above the chest', or rename the choice to something already on screen. |
| minor | critic | DA_Event_DesertersLongboat | `choices[0].resultText, choices[1].resultText` | Voice: no copy-paste | The two result texts are identical apart from one noun, and each just repeats its label ('Wave … aboard' → 'You wave … over the rail'). | Give each recruit its own concrete beat: the swordsman tests the rail with his blade, the marksman unwraps the rifle and checks the powder. |
| minor | critic | DA_Event_DesertersLongboat | `body` | Voice: concrete and plausible | In 'walked off a rival ship', the idiom 'walked off' sits oddly at sea. | 'slipped off a rival ship' or 'cut loose from a rival ship'. |
| minor | critic | DA_Event_RivalsCache / DA_Event_SquallLine | `choices[1].resultText / choices[2].resultText` | Fit: near-duplicates | 'The sea decides the rest.' and 'Whatever the sea took, it took fast.' use the same 'the sea' wording to cover a RandomOf. Seen in the same run, the repetition is obvious. | Give one of them a physical detail instead. For the chest: 'The line sings as it comes up.' |
| minor | critic | DA_Event_RivalsCache / DA_Event_DesertersLongboat | `choices[2].outcomes / choices[2].outcomes` | Fit: variety | Both Good events reward HealthRestore, described both times as someone else's 'medicine'. The set never uses ManaRestore, GrantXp or AbilityChoice. | Swap one of them. For example, the cache barrel could give ManaRestore or GrantXp, or the deserters could pass on what they know (GrantXp to Chosen). |

## Writer's notes on the revision

Fixes:
- RivalsCache dead button (major): I took out 'Leave their mark alone'. The sure-gold choice is now 'Cut the purse free': +50 gold and -15 HP to a Chosen crewmate. The new third choice, 'Strip the marker barrel', gives 1 HealthRestore. That's about 30 gold of value with no HP cost, but it uses an inventory slot. All three choices now trade something different: HP for gold, a gamble, or a sure item. The chest gamble is unchanged.
- RandomOf result text (major): every 'Either... or...' hedge is gone. Each text is now one line that's true for every branch. Chest: 'You get a hook on the chest... The sea decides the rest.' Squall: 'You put the bow into the black... Whatever the sea took, it took fast.' Mutterings: 'By morning, the crew has settled it their own way.' The Recruit texts no longer spell out the full-crew case. They read 'Whether there's a bunk to spare is another matter', which is true either way.
- DesertersLongboat trap (major): I removed the Gold -40 from the Sniper choice. Both recruits are now free and can't waste gold. The pick is which role you add. The medicine trade now needs MinGold 20, so the item isn't free at 0 gold. A broke player can still pick a recruit choice, so they're never stuck.
- Mutterings dominance (major): I did both of the critic's suggestions. The fight now costs -50 HP on the Captain (the guardrail max), and paying out costs 50 gold instead of 60. The two sure choices now cost about the same in different currencies. Losing 50 HP on the captain is a real risk going into the next fight.

Brief coverage: layers 4-6. 2 Good (RivalsCache, DesertersLongboat) and 2 Bad (SquallLine, MutteringsBelowDeck). Recruit is in Deserters and LoseCrew Random is in Mutterings (it never takes the captain). RandomOf is in Cache, Squall and Mutterings. Guardrails: gold between +150 and -50, ship loss at most 70, crew loss at most 50, items at most 2. No banned words.

Open for Brandon:
- Deserters is still weak with a full crew of 4. Both recruit buttons do nothing, so only the medicine trade has any effect. A 'crew not full' requirement (CrewCountAtMost) would let those buttons disable cleanly. Until that exists, you could hold this event.
- The Mutterings fists cost assumes crew HP carries between nodes. If HP refills, -50 captain HP is close to free and Pay should drop further.
- A MinShipHealth requirement would let Squall show its stakes more clearly. This is an idea only.

## Open questions for Brandon

- Should events get a 'crew not full' requirement (e.g. CrewCountAtMost)? Without it, any Recruit choice that costs gold is a trap with a full crew of 4.
- Should RandomOf options get their own result text? Without it, every gamble's result text has to stay vague or list every branch.
- Does crew HP carry over between nodes or refill after fights? This decides how much the CrewHealth costs in SquallLine and MutteringsBelowDeck actually hurt.
- What happens when GiveItem would go over the 6-item inventory cap? Does it fail gracefully, add only part, or block the choice? It matters for the Deserters medicine trade.
- Is two HealthRestore for 20 gold (half the shop price) an acceptable Good-event bargain? The writer flagged this too.
- The writer asked whether crew HP carries between nodes. It does: C9 (03-Combat-Turns.md, 09-Run-Map.md) says HP carries and only MP refills. The Mutterings and Squall crew costs are real as priced, so no further gold cut is needed for that reason.
- Should Brandon add a CrewCountAtMost (or 'crew not full') requirement? Without it, any Recruit event is a dead button with a full crew.
- What does GiveItem do when the inventory already holds 6 items? Both Good events can land on a full bag, and the facts don't define overflow.
- Does the event result screen list the outcomes that were applied, for example which crewmate LoseCrew removed? Each choice has only one resultText, so in Mutterings a permadeath could happen with no named victim on screen.

## Final events

### The Rival's Cache  `DA_Event_RivalsCache`
```
Title:    The Rival's Cache
Valence:  Good      Layers: 4-6        Weight: 1
Body:     A tarred barrel bobs on a short line, painted with a rival crew's mark. Below it, the rope runs taut into green water. Someone sank their plunder here and meant to come back for it.
Choice 1: Cut the purse free  -> Outcomes: Gold +50; CrewHealth -15 (Chosen)  -> Result: Your diver saws through the line and comes up with a fat purse and a gashed arm.
Choice 2: Bring up the whole chest  -> Outcomes: RandomOf [ 3: Gold +150 | 2: Gold +20, ShipHealth -50 ]  -> Result: You get a hook on the chest and start hauling. The sea decides the rest.
Choice 3: Strip the marker barrel  -> Outcomes: GiveItem HealthRestore x1  -> Result: Under the tar, the barrel is packed with someone's spare medicine. You leave the line to its owners.
```

### Deserters in a Longboat  `DA_Event_DesertersLongboat`
```
Title:    Deserters in a Longboat
Valence:  Good      Layers: 4-6        Weight: 1
Body:     A longboat bumps against your hull. The sailors in it walked off a rival ship two nights ago and haven't eaten since. One still has a sword and another has a long rifle wrapped in sailcloth.
Choice 1: Wave the swordsman aboard  -> Outcomes: Recruit Swordsman  -> Result: You wave the swordsman over the rail. Whether there's a bunk to spare is another matter.
Choice 2: Wave the marksman aboard  -> Outcomes: Recruit Sniper  -> Result: You wave the marksman over the rail. Whether there's a bunk to spare is another matter.
Choice 3: Trade rations for medicine  -> Requires: MinGold 20 -> Outcomes: Gold -20; GiveItem HealthRestore x2  -> Result: They give up two stolen medicine kits for a sack of hardtack and a few coins. Nobody asks whose they were.
```

### Squall Line  `DA_Event_SquallLine`
```
Title:    Squall Line
Valence:  Bad       Layers: 4-6        Weight: 1
Body:     The horizon goes black from end to end. The wind turns against you, and the first rain hits the deck sideways. You have a minute to decide how to take it.
Choice 1: Run before the wind  -> Outcomes: ShipHealth -45  -> Result: You outrun the worst of it, but the hull takes a beating from the waves on the way.
Choice 2: Reef sails and hold  -> Outcomes: CrewHealth -20 (All); ShipHealth -15  -> Result: Everyone works the rigging until their hands bleed. The ship mostly holds.
Choice 3: Drive straight through  -> Outcomes: RandomOf [ 1: ShipHealth -10 | 1: ShipHealth -70 ]  -> Result: You put the bow into the black and hold on. Whatever the sea took, it took fast.
```

### Mutterings Below Deck  `DA_Event_MutteringsBelowDeck`
```
Title:    Mutterings Below Deck
Valence:  Bad       Layers: 4-6        Weight: 1
Body:     The talk stops whenever you go below. Too many fights and too few shares have soured the crew. By morning, somebody will act on it.
Choice 1: Pay out shares  -> Requires: MinGold 50 -> Outcomes: Gold -50  -> Result: Coin goes around the deck and the muttering stops. For now.
Choice 2: Settle it with fists  -> Outcomes: CrewHealth -50 (Captain)  -> Result: The captain takes the ringleader on in front of everyone and wins, barely. No one mutters after that.
Choice 3: Let it fester  -> Outcomes: RandomOf [ 2: CrewHealth -20 (All) | 1: LoseCrew (Random) ]  -> Result: You leave them to it and turn in. By morning, the crew has settled it their own way.
```
