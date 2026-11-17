# Batch `early-waters` - needs Brandon (blocker/major left after revisions)

Brief: The first days out of port. Wrecks, traders, small strokes of luck. Low stakes: the player is learning that events are trade-offs (gold vs. HP, a sure thing vs. a gamble), not free loot.
Layers 1-3, 4 events (3 Good, 1 Bad).
Rounds: 2. Agent cost: $0.71 over 4 calls.

**Sounds like Deadwater Run?** Yes - 4/5 (final round). Mostly plain, physical and dry, in second person. 'The hull barely notices. The crew's shoulders do.' and the sandbar line land well. The weak spots are one result text that reads like a rules summary and a few lines that promise more than the outcomes do.

Weakest lines (critic):
- Your diver comes up with a full chest, or comes up empty-handed and bleeding from the timbers.
- He names a fair price, which is how you know he wants to be gone.
- your crew stops dropping their guard.
- Over go stores and a chest of coin.

## Round 1 review - critic verdict: revise, voice 4/5

| Sev | From | Event | Field | Rule | Problem | Fix |
|---|---|---|---|---|---|---|
| minor | code | DA_Event_HiddenShoal | `choices[1].resultText` | E02 text | Result text is more than 2 sentences | Trim |
| major | critic | DA_Event_DriftingWreck | `choices[1].outcomes[0].options` | Design: no dominant choice | 'Dive the hold' is close to dominant over 'Strip the deck'. On average it pays 84 gold against a sure 40, and its worst case (+30 gold, -20 HP to one random crew member, with HP floored at 1) is barely worse than the safe pick. The brief asks for 'a sure thing vs. a gamble', but this gamble has almost no real downside. | Make the bad branch hurt. For example, 50/50 weights with the bad branch at 0 gold and -25 to -30 CrewHealth (Random). Or raise 'Strip the deck' to about 60 gold so the safe pick is worth taking. |
| major | critic | DA_Event_CastawayOnASpar | `choices[0]` | Design: no trap choice | With a full crew (4), 'Sign him on' is still enabled but does nothing (Recruit fails gracefully), while the other choice gives 60 gold. No requirement exists to disable it. A player with 3 crew plus a recruit from a shop or earlier event walks into a free loss. The result text admits this ('If there's no bunk for him...'), but the button still invites the click. | Block on Brandon's call: either add a 'CrewCountAtMost' requirement (not in the facts, see open questions) or accept the trap and rewrite the label or body so a full crew is warned up front. Don't ship as-is. |
| major | critic | DA_Event_HiddenShoal | `choices[2].requirements` | Design: Bad event must cost something | 'Lighten the hold' has no requirement, and Gold floors at 0. A player with little or no gold (common right after a shop) loses nothing, so the Bad event becomes free and the choice beats ship damage and crew damage outright. | Add { "type": "MinGold", "amount": 60 } to choices[2].requirements. Choices 0 and 1 remain as fallbacks with no requirements. |
| minor | critic | DA_Event_HiddenShoal | `choices[0].resultText` | Text matches data | "leaves half the keel on the sand" overstates a -50 ShipHealth hit, which is 20% of 250 max HP. | Tone it down, e.g. "...and leaves a good length of keel on the sand." |
| minor | critic | DA_Event_HiddenShoal | `choices[2].resultText` | Text matches data | "Over go stores and a chest of coin" suggests the player loses more than gold. The only outcome is Gold -60. | Make it only about coin, e.g. "Over goes a chest of coin. She floats off clean, and you're poorer for it." |
| minor | critic | DA_Event_DriftingWreck | `choices[1].resultText` | Text matches data | One result text covers both RandomOf branches ('a full chest - or ...'), so the player never reads what actually happened. The data format has no per-option result text. | Write a neutral line that doesn't describe either outcome (e.g. "Your diver goes over the side with a knife in his teeth.") and let the outcome readout show the result. See the open question about per-option text. |
| minor | critic | DA_Event_DriftingWreck | `choices[2]` | Design: real decisions | 'Leave her to the sea' is strictly worse than 'Strip the deck', which is free and has no requirements. Nobody would ever pick it. The line 'Nothing happens' also reads like a menu item. | Cut it to 2 choices, or give it a small upside (e.g. a little ShipHealth from salvaged timber). If kept, replace 'Nothing happens' with something drier. |
| minor | critic | DA_Event_PeddlersSloop | `choices[0].resultText` | Text consistency | The body says he names 'a fair price'. The result says 'Cheaper than any port will sell them.' Those are slightly at odds. The joke works better if the price is suspiciously good. | Change the body to 'He names a low price, which is how you know he wants to be gone.' |
| minor | critic | DA_Event_PeddlersSloop | `choices[0].outcomes[1]` | Inventory cap | With 5 or 6 items in inventory, the player pays 40 gold and may get 1 or 0 tonics. GiveItem's behaviour when inventory is full isn't in the facts, and no requirement can check free slots. | Wait for Brandon's answer on GiveItem overflow. If it drops items silently, refund the gold or add a free-slot requirement. |
| minor | critic | DA_Event_PeddlersSloop | `choices[1].resultText` | Voice | "He sails off fast." is flat and repeats the body's 'wants to be gone' beat. | Make it drier, e.g. "He's under full sail before you can say thank you." |

## Round 2 review - critic verdict: revise, voice 4/5

| Sev | From | Event | Field | Rule | Problem | Fix |
|---|---|---|---|---|---|---|
| minor | code | DA_Event_HiddenShoal | `choices[1].resultText` | E02 text | Result text is more than 2 sentences | Trim |
| major | critic | DA_Event_DriftingWreck | `choices[1].resultText` | Voice / result text fits every branch | FDwEventChoice has one ResultText per choice (DwEventDefinition.h:24), so writing for both branches is the right idea. But 'X, or Y' reads like a tooltip listing the possible results, not like something that happened. The player who gets +120 gold still reads 'or comes up empty-handed and bleeding'. 'Bleeding from the timbers' is also garbled: the timbers aren't bleeding. | Write one line that is true for both branches and let the change log show what happened. For example: 'Your diver goes down into the black hold, and the sea decides what comes back up.' |
| minor | critic | DA_Event_DriftingWreck | `choices[2]` | Design: no dead choice | 'Leave her to the sea' gives nothing. 'Strip the deck' gives +50 gold for free. So Leave is strictly worse and never worth picking. That works against the brief's point that events are trade-offs. | Either drop Leave (two choices is allowed) or give 'Strip the deck' a small cost so walking away is a real option. For example, a 25/75 RandomOf with -10 CrewHealth on a Random crew member. |
| minor | critic | DA_Event_PeddlersSloop | `body / choices[0].resultText` | Text matches data | The body says 'He names a fair price', but the result text says 'Cheaper than any port will sell them'. The data agrees with the result text: 40 gold for two, against 60 at a shop. The body and result text contradict each other. | Change the body to 'He names a low price, which is how you know he wants to be gone.' That also makes the hurry line make sense. |
| minor | critic | DA_Event_CastawayOnASpar | `choices[0].resultText` | No promised effects | 'your crew stops dropping their guard' sounds like a defence buff. The only outcome is +50 XP to All. | Point it at growth instead. For example: '...He keeps his purse, and your crew learns a few of his tricks.' |
| minor | critic | DA_Event_CastawayOnASpar | `body` | Text matches data | 'the heavy purse around his neck' sets up a big payout. It pays 60 gold, less than half of a fight's 150. | Change it to 'a thin purse' or just 'his purse'. Or raise the gold to around 80-100, still inside goldGainMax, and rebalance against the XP choice. |
| minor | critic | DA_Event_HiddenShoal | `choices[2].resultText` | No promised effects | 'Over go stores' suggests supplies or items are lost, but the only outcome is -60 gold. | Change it to 'Over goes a chest of coin. She floats off clean, and you're poorer for it.' |
| minor | critic | DA_Event_HiddenShoal | `choices[0].resultText` | Text matches data | 'leaves half the keel on the sand' overstates -50 ship HP, which is 20% of 250. The pain sounds much bigger than the hit. | Tone it down. For example: '...and leaves a long strip of her keel on the sand.' |

## Writer's notes on the revision

Revision of early-waters: 3 Good, 1 Bad, layers 1-3, weight 1.

Fixes:
- DriftingWreck (major, dominant gamble): 'Strip the deck' goes from +40 to +50. 'Dive the hold' is now 50/50: +120 gold, or 0 gold and -30 HP to one random crew member. That's an average of 60 gold with a real HP cost half the time, against a sure 50. The result text now covers the empty-handed branch.
- CastawayOnASpar (major, trap with a full crew): I didn't wait on CrewCountAtMost. I took Recruit out. The castaway now trades either his purse (+60 gold) or crew drill (+50 XP to All, half a level each). Both choices work at any crew size, so there's no trap and still a real gold-vs-growth choice. The body explains both offers. When Brandon adds a CrewCountAtMost requirement, a 'Sign him on' (Recruit Swordsman, CrewCountAtMost 3) choice can come back.
- HiddenShoal (major, free with no gold): 'Lighten the hold' now needs MinGold 60. 'Force her off' and 'Over the side and push' have no requirements and stay available.

Unchanged: PeddlersSloop wasn't flagged.

Open questions for Brandon: (1) Sloop sells 2 HealthRestore for 40 total, below the shop's 30 each. Is that discount OK? (2) Does GiveItem fail gracefully when the inventory is full (6)? (3) Idea: a CrewCountAtMost requirement, so Recruit events can disable their button cleanly.

## Open questions for Brandon

- GiveItem when the inventory is full (6 items): does it partly fail, fully fail, or overflow? It affects whether PeddlersSloop 'Buy two tonics' can take gold and give nothing.
- Should there be a 'CrewCountAtMost' requirement (or should Recruit choices auto-disable when the crew is full)? Without one, every Recruit event is a trap for a full crew.
- Should RandomOf options get their own resultText? Without it, gamble choices can't tell the player which way the roll went in-voice.
- Does crew HP carry over between fights? If HP resets, every event CrewHealth loss (Wreck dive, Shoal push) costs nothing and these trade-offs fall apart.
- Is a captain-only choice that is strictly better for that captain (Fearmongerer gets free items in PeddlersSloop) the intended way to do captain perks?
- PeddlersSloop sells 2 HealthRestore at 20 each, against 30 in the shop. Is that discount OK for a Good event in layers 1-3? (The writer raised this.)
- GiveItem when the inventory is full (max 6): does the item get dropped, the choice get blocked, or the gold get refunded? 'Buy two tonics' takes 40 gold before GiveItem runs, so if items are dropped silently, a player with 5-6 items pays for nothing. If that's how it works, we may need a 'has inventory space' requirement.
- Is it OK for the sloop to sell 2 HealthRestore for 40 gold, below the shop's 30 each? This is the writer's question; I think it's fine for a Good event.
- The Fearmongerer-only choice gives a free HealthRestore and ManaRestore, and the other captains get no matching option. Are captain-gated freebies in events the intended pattern?
- Should a CrewCountAtMost requirement be added so Recruit events like 'Sign him on' can come back without being a trap?

## Final events

### The Drifting Wreck  `DA_Event_DriftingWreck`
```
Title:    The Drifting Wreck
Valence:  Good      Layers: 1-3        Weight: 1
Body:     A merchant hull lies on her side in the swell, masts gone, hold half under. Something glints below the waterline. The tide is turning, and she won't float much longer.
Choice 1: Strip the deck  -> Outcomes: Gold +50  -> Result: Brass fittings, a purse in a dead man's coat. Small, but it's yours.
Choice 2: Dive the hold  -> Outcomes: RandomOf [ 50: Gold +120 | 50: CrewHealth -30 (Random) ]  -> Result: Your diver comes up with a full chest, or comes up empty-handed and bleeding from the timbers.
Choice 3: Leave her to the sea  -> Outcomes: none  -> Result: You hold your course. Nothing happens, and behind you the wreck slips under.
```

### A Peddler's Sloop  `DA_Event_PeddlersSloop`
```
Title:    A Peddler's Sloop
Valence:  Good      Layers: 1-3        Weight: 1
Body:     A little sloop runs up her colours and comes alongside. Her captain has crates of tonics lashed to the rail and a nervous smile. He names a fair price, which is how you know he wants to be gone.
Choice 1: Buy two tonics  -> Requires: MinGold 40 -> Outcomes: Gold -40; GiveItem HealthRestore x2  -> Result: Two bottles for forty gold. Cheaper than any port will sell them.
Choice 2: Let him see your captain  -> Requires: CaptainIs Fearmongerer -> Outcomes: GiveItem HealthRestore x1; GiveItem ManaRestore x1  -> Result: He takes one look at the beard and calls it a gift. He sails off fast.
Choice 3: Wave him off  -> Outcomes: none  -> Result: Nothing changes hands. The sloop goes off looking for an easier crew.
```

### Castaway on a Spar  `DA_Event_CastawayOnASpar`
```
Title:    Castaway on a Spar
Valence:  Good      Layers: 1-3        Weight: 1
Body:     A man clings to a broken spar, cutlass still in his belt. He says he was a master-at-arms until his ship went down two nights ago. He'll pay for a ride to shore with the heavy purse around his neck, or work off his passage drilling your crew.
Choice 1: Put him to work  -> Outcomes: GrantXp 50 (All)  -> Result: Three days of blade drill on a rolling deck. He keeps his purse, and your crew stops dropping their guard.
Choice 2: Take the purse  -> Outcomes: Gold +60  -> Result: You row him to a sandbar and take your fare. A fair trade, more or less.
```

### The Hidden Shoal  `DA_Event_HiddenShoal`
```
Title:    The Hidden Shoal
Valence:  Bad       Layers: 1-3        Weight: 1
Body:     The keel grinds and the whole ship shudders to a stop. You've run onto a sandbank the chart never showed. The tide is going out, and the hull groans with every wave.
Choice 1: Force her off  -> Outcomes: ShipHealth -50  -> Result: Full sail drags her free, and leaves half the keel on the sand.
Choice 2: Over the side and push  -> Outcomes: CrewHealth -15 (All); ShipHealth -10  -> Result: Waist-deep in cold water, the crew heaves her clear. The hull barely notices. The crew's shoulders do.
Choice 3: Lighten the hold  -> Requires: MinGold 60 -> Outcomes: Gold -60  -> Result: Over go stores and a chest of coin. She floats off clean, and you're poorer for it.
```
