# Deadwater Run - Style Guide (generated)

> Built by `node Tools/ContentPipeline/content-pipeline.js style` from the files below. Don't edit this file: edit the source and rebuild. Every rule quotes its source line, and the build fails if a quote moves or a rule stops naming something only Deadwater Run has (the stranger test).

Sources: `DeadwaterRun.md`, `Docs/ContentPipeline/voice.md`, `Docs/Release/StorePage.md`, `Docs/Architecture/14-UI.md`, `Docs/Architecture/11-Events.md`, `Tuning/Events.json`.

## Setting and story

- **S01** Every event happens on the seas of Aquadia, somewhere on the chart between port and the Kraken. Places belong to Aquadia (reefs, shoals, wrecks, rival crews); no real-world ports, nations, navies or history.
  - `DeadwaterRun.md:5`: "legendary seas of Aquadia"
  - `Docs/ContentPipeline/voice.md:8`: "The legendary seas of Aquadia."
- **S02** Each event restages the game's hook in miniature: a quick, safe option against a slow, rich one, the same call as sinking the ship (1x loot) or killing the crew to plunder (1.5x). If no choice carries that sink-or-plunder tension, the event is not Deadwater Run.
  - `DeadwaterRun.md:5`: "sink your enemy’s ship allowing you to progress through the fight quicker or slay all enemies for the opportunity to plunder?"
  - `DeadwaterRun.md:88`: "grants you 1.5x the loot"
  - `Docs/Release/StorePage.md:102`: "Quick and safe, or slow and rich - your call, every fight."
- **S03** Costs land on Deadwater Run's two health bars: the ship (250 HP, two 70-damage cannons, ShipHealth) and the crew (100 HP each, CrewHealth). The strongest events make you spend one bar to save the other: the hull or the crew.
  - `DeadwaterRun.md:61`: "Ship starts with 250 HP and two cannons"
  - `DeadwaterRun.md:66`: "Start with a pirate crew of three (100 HP/20 MP)"
  - `DeadwaterRun.md:5`: "Each pirate ship has a health bar too."
- **S04** Crew lives are permanent. A pirate lost to LoseCrew is gone for good, from a crew of 4 at most. That loss is never a throwaway line: the result text names who goes (the Swordsman, the Sniper) and lets it land.
  - `DeadwaterRun.md:70`: "Once a crew member dies… they are gone for good!"
  - `Docs/Release/StorePage.md:108`: "Permadeath crew: a fallen pirate is gone for good"
  - `DeadwaterRun.md:72`: "Maximum crew of 4 members (including the captain)"
- **S05** You are a small, tough crew: a captain, a Swordsman (close range) and a Sniper (long range), up to four. Not a fleet, not a navy, no first mate, quartermaster or cabin boy the game doesn't have. Crowds of your own sailors don't exist.
  - `Docs/Release/StorePage.md:123`: "You have a ship, a captain and two crewmates. That will have to do."
  - `DeadwaterRun.md:68`: "Swordsman (Close range)"
  - `DeadwaterRun.md:69`: "Sniper (Long range)"
- **S06** The Kraken waits at the end of the chart and has never been beaten. Events can foreshadow it (tentacle scars, hulls it broke, crews that sailed on before you) but never fight it, wound it or hand you its weakness. It is the only monster in Aquadia.
  - `DeadwaterRun.md:121`: "Players must fight a Kraken to beat the game"
  - `Docs/Release/StorePage.md:123`: "the Kraken has never been beaten"
  - `DeadwaterRun.md:125`: "Tentacle slam"
- **S07** Aquadia's sea is mostly kind but never safe: 60% of events are Good, the rest Bad. Good events can still cost something; Bad events always offer a choice that limits the damage.
  - `DeadwaterRun.md:118`: "Random event occurs with 60% good / 40% bad odds"
  - `Docs/ContentPipeline/voice.md:11`: "The sea is mostly kind (60% good events) but never safe"
- **S08** The other people on the water are rival pirate crews making their own run at the Kraken: what they left, what they want, what they fear. No magic, gods, ghost ships or curses with rules, because nothing in the game can show them.
  - `DeadwaterRun.md:86`: "Obtain gold and new abilities upon successfully defeating a rival crew"
  - `Docs/ContentPipeline/voice.md:24`: "Magic systems, gods, curses with rules, anything the game can't show or doesn't have"

## The three captains

- **C01** A CaptainIs TeamLeader choice plays to the Team Leader: "Leads a loyal crew"; "Crew members next to each other can perform synergy moves (dealing bonus damage/effects)"; "Can defend other crew members from attacks (move onto them to take any hits that would be addressed to them)". Draw on what the GDD gives them: Rally Cry (Heals all crew by 15 and removes all negative status effects); Grapple Hook (Can pull a target to the spot right next to the captain or pull the captain to the spot right next to the target (target can be player crew or enemy) (range: 10 spots and within viewing distance)); run rule Veteran Crew (Their crew gets +50% XP and levels up twice as fast). Text gated to another captain never borrows this one's traits.
  - `DeadwaterRun.md:20`: "Leads a loyal crew"
- **C02** A CaptainIs Fearmongerer choice plays to the Fearmongerer: "Known across the seas for his ruthlesness"; "Enemies collect Fear stacks. At 3 stacks, an enemy loses its next turn as it cowers"; "If every enemy is at 3 stacks, the crew surrenders: you win the fight, get the plunder, and nobody dies"; "Adds a third choice: sink the ship, kill the crew, or scare them into giving up". Draw on what the GDD gives them: Smoke and Fire (lights the fuses in his beard. Every enemy gets 1 Fear, and enemies already afraid take damage from smoke); Six Pistols (Six shots at random targets for 15 damage each); run rule Dread Reputation (Each fight you win by surrender adds +1 starting Fear to every later right. His legend grows over the run). Text gated to another captain never borrows this one's traits.
  - `DeadwaterRun.md:34`: "Known across the seas for his ruthlesness"
- **C03** A CaptainIs Conqueror choice plays to the Conqueror: "Captain has one extra move and is significantly stronger than his crew members"; "Unlike other captains, the conqueror can only use a sword"; "Crew members only have one choice per turn (can choose to attack or move, can't do both)". Draw on what the GDD gives them: Charge (Dash up to six spaces and strike the first enemy hit for heavy damage); Eye of the Tiger (Target one enemy from anywhere on the map and freeze them from moving and acting for one turn); run rule "Every enemy the captain kills himself gives him +1% permanent damage (max of 20%)". Text gated to another captain never borrows this one's traits.
  - `DeadwaterRun.md:49`: "Captain has one extra move and is significantly stronger than his crew members"

## Voice

- **V01** Write as "you", the captain, in present tense ("A longboat bumps against your hull."). Untagged text never assumes which captain you are: it must read true for the Team Leader, Fearmongerer, Conqueror alike.
  - `Docs/ContentPipeline/voice.md:15`: "**Second person, present tense.** "A longboat bumps against your hull.""
- **V02** Short sentences built from what is on a Deadwater Run deck: hull, powder, rope, rum, gold, rigging, fog, the cannons, the sniper's nest. No abstractions where a thing would do.
  - `Docs/ContentPipeline/voice.md:14`: "**Plain and punchy.** Short sentences. Concrete nouns: hull, powder, rope, rum, gold, deck, rigging, fog"
  - `DeadwaterRun.md:82`: "The sniper's nest of a ship is available"
- **V03** Dry gallows humour, the store page's "That will have to do." Understatement over punchlines. Aquadia is dangerous and tense: not a cartoon, not grimdark.
  - `Docs/ContentPipeline/voice.md:10`: "The tone is a small, tough crew against a cruel sea. Dangerous and tense, with dry gallows humour. It isn't a cartoon and it isn't grimdark"
  - `Docs/Release/StorePage.md:123`: "That will have to do."
- **V04** One salty word per line at most. Deadwater Run never says "arr", "matey", "ye", "shiver me timbers": those are stage pirates, and the code checks ban them.
  - `Docs/ContentPipeline/voice.md:17`: "**Light pirate flavour.** One salty word per line at most. No "arr", "matey", "ye" or "shiver me timbers""
- **V05** Labels are short imperatives ("Cut the line"). The event screen already prints each outcome's Describe() "you will" line under its button, so a label never restates the numbers and never reads like "Option A: lose 20 gold".
  - `Docs/ContentPipeline/voice.md:19`: "**Choices read as decisions**, not as menu items. "Cut the line" rather than "Option A: lose 20 gold""
  - `Docs/Architecture/11-Events.md:56`: "shows title, body and one button per choice with its outcomes' "you will" lines"
- **V06** A body is 2-4 sentences, a result 1-2. Aquadia is shown through one scene, never explained: no history of the seas, no backstory of the Kraken.
  - `Docs/ContentPipeline/voice.md:23`: "Long lore dumps. A body is 2-4 sentences"
- **V07** Every promise in a body, label or result maps to an outcome in that choice: "two medicine kits" only with GiveItem HealthRestore x2. A RandomOf choice shows one ResultText for every branch, so it must read true whichever branch lands; never describe both.
  - `Docs/ContentPipeline/voice.md:26`: "Promising effects the outcomes don't apply"
  - `Docs/Architecture/11-Events.md:27`: "`TArray<TObjectPtr<UDwEventOutcome>> Outcomes` (applied in order)"

## Aesthetic (Ship's Log art direction)

- **A01** Every screen is the Ship's Log: an event is ink on a parchment page, its title set in Grenze and its body in Texturina. Write what a captain would ink into the log of the run. No UI talk ("click", "button", "menu", "screen"), no emoji, no markdown.
  - `Docs/Architecture/14-UI.md:116`: "**"Ship's Log"** everywhere - parchment pages and cards"
  - `Docs/Architecture/14-UI.md:116`: "fonts Grenze (headings, `F_DwHeading`) and Texturina (body"
- **A02** ALL CAPS belongs to the Ship's Log ink stamps ("HULL IS SOUND", "SOLD OUT", "WE ARE HERE"). Event text is sentence case and titles are short headings ("The Tally Rock"). No caps-lock shouting, no exclamation marks.
  - `Docs/Architecture/14-UI.md:115`: ""HULL IS SOUND", "SOLD OUT""
  - `Docs/Architecture/14-UI.md:120`: "WE ARE HERE over the current one"
- **A03** On the parchment chart an event is only a "?" stamp, so the title is the reveal: it names the thing you sailed into ("The Drifting Wreck", "Squall Line"), never the outcome ("Free Gold").
  - `Docs/Architecture/14-UI.md:120`: "sword = fight, coin = shop, ? = event"

## Reference lines

- "The seas of Aquadia have never been kind, and the Kraken has never been beaten." (`Docs/Release/StorePage.md:123`)
- "You have a ship, a captain and two crewmates. That will have to do." (`Docs/Release/StorePage.md:123`)
- "Quick and safe, or slow and rich - your call, every fight." (`Docs/Release/StorePage.md:102`)
- "Lose a pirate and they're gone for good." (`Docs/Release/StorePage.md:90`)

## The shipped event pool (`Tuning/Events.json`, in the game)

How Deadwater Run events already sound. New content matches this voice and never repeats these ideas.

- **The Drifting Wreck** (Good, layers 1-3): A merchant hull lies on her side in the swell, masts gone, hold half under. Something glints below the waterline. The tide is turning, and she won't float much longer.
- **A Peddler's Sloop** (Good, layers 1-3): A little sloop runs up her colours and comes alongside. Her captain has crates of tonics lashed to the rail and a nervous smile. He names a fair price, which is how you know he wants to be gone.
- **Castaway on a Spar** (Good, layers 1-3): A man clings to a broken spar, cutlass still in his belt. He says he was a master-at-arms until his ship went down two nights ago. He'll pay for a ride to shore with the heavy purse around his neck, or work off his passage drilling your crew.
- **The Hidden Shoal** (Bad, layers 1-3): The keel grinds and the whole ship shudders to a stop. You've run onto a sandbank the chart never showed. The tide is going out, and the hull groans with every wave.
- **The Rival's Cache** (Good, layers 4-6): A tarred barrel bobs on a short line, painted with a rival crew's mark. Below it, the rope runs taut into green water. Someone sank their plunder here and meant to come back for it.
- **Deserters in a Longboat** (Good, layers 4-6): A longboat bumps against your hull. The sailors in it walked off a rival ship two nights ago and haven't eaten since. One still has a sword and another has a long rifle wrapped in sailcloth.
- **Squall Line** (Bad, layers 4-6): The horizon goes black from end to end. The wind turns against you, and the first rain hits the deck sideways. You have a minute to decide how to take it.
- **Mutterings Below Deck** (Bad, layers 4-6): The talk stops whenever you go below. Too many fights and too few shares have soured the crew. By morning, somebody will act on it.
- **The Turning Brig** (Good, layers 6-9): A brig limps past on the other tack, her foremast gone and her rail torn open. Her captain shouts across that he has seen what waits ahead and is going home. He has no more use for what's in his hold.
- **The Tally Rock** (Good, layers 6-9): A black rock stands alone in the swell, cut all over with names. Every crew that sailed on to the Kraken stopped here first. Under a cairn at its foot sits a strongbox and a page of notes, weighted with a stone.
- **Wreckers Among the Hulls** (Bad, layers 6-9): Broken ships lie on their sides in the shallows, ribs open to the sky. A wrecker crew is stripping them, and their longboats are already pulling your way. Their captain wants a toll, or your hull for parts.
- **Something Under the Keel** (Bad, layers 6-9): The water goes flat and dark. Something long passes beneath the ship, slow enough that you can count the seconds. Then the deck lifts under your feet.

## Signature lexicon (the stranger test)

Terms only Deadwater Run has, pulled from the sources. Every rule above names at least one. Genre words never count: pirate, pirates, ship, ships, sea, seas, gold, hull, crew, captain, treasure, rum, cannon, cannons, deck, sword, plank, parrot, island, storm, map, sail, sails, port.

`Aquadia`, `Kraken`, `Fear`, `plunder`, `sink`, `Swordsman`, `Sniper`, `sniper's nest`, `gone for good`, `250 HP`, `rival crew`, `tentacle`, `Ship's Log`, `parchment`, `Grenze`, `Texturina`, `Black Lantern`, `wax`, `Describe`, `CaptainIs`, `LoseCrew`, `RandomOf`, `Deadwater Run`, `Tentacle slam`, `Kranky swipe`, `Tentacle grab`, `Arms of fury`, `Eyes of the sea`, `Team Leader`, `TeamLeader`, `Rally Cry`, `Grapple Hook`, `Veteran Crew`, `Fearmongerer`, `Smoke and Fire`, `Six Pistols`, `Dread Reputation`, `Conqueror`, `Eye of the Tiger`
