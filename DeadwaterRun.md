# Deadwater Run

## Executive Summary

- Deadwater Run is a turn-based roguelike where you navigate a pirate-crew across the legendary seas of Aquadia. Combat will play out like a familiar turn-based game, but with a twist! Each pirate ship has a health bar too. Do you choose to sink your enemy’s ship allowing you to progress through the fight quicker or slay all enemies for the opportunity to plunder?

## Game Mechanics

### Main Game Loop:

- Pick a captain that suits your playstyle (3 captain choices with unique abilities)
- Navigate across a pirate map encountering Fights, Shops, and Events.
- Challenge the Kraken at the end to escape the Aquadia seas.

### Captain Choices

#### Team Leader

- What's different:
    - Leads a loyal crew
    - Crew members next to each other can perform synergy moves (dealing bonus damage/effects)
    - Can defend other crew members from attacks (move onto them to take any hits that would be addressed to them)

- Combat Abilities:
    - Rally Cry: Heals all crew by 15 and removes all negative status effects
    - Grapple Hook: Can pull a target to the spot right next to the captain or pull the captain to the spot right next to the target (target can be player crew or enemy) (range: 10 spots and within viewing distance)

- Run rule
    - Veteran Crew: Their crew gets +50% XP and levels up twice as fast

#### Fearmongerer

- What's different:
    - Known across the seas for his ruthlesness
    - Enemies collect Fear stacks. At 3 stacks, an enemy loses its next turn as it cowers
    - If every enemy is at 3 stacks, the crew surrenders: you win the fight, get the plunder, and nobody dies,
    - Adds a third choice: sink the ship, kill the crew, or scare them into giving up

- Combat Abilities:
    - Smoke and Fire: lights the fuses in his beard. Every enemy gets 1 Fear, and enemies already afraid take damage from smoke
    - Six Pistols: Six shots at random targets for 15 damage each. 

- Run rule
    - Dread Reputation: Each fight you win by surrender adds +1 starting Fear to every later right. His legend grows over the run

#### The Conqueror

- What's different:
    - Captain has one extra move and is significantly stronger than his crew members
    - Unlike other captains, the conqueror can only use a sword
    - Crew members only have one choice per turn (can choose to attack or move, can't do both)

 - Combat Abilities:
    - Charge: Dash up to six spaces and strike the first enemy hit for heavy damage
    - Eye of the Tiger: Target one enemy from anywhere on the map and freeze them from moving and acting for one turn

- Run rule
    - Every enemy the captain kills himself gives him +1% permanent damage (max of 20%)

### Your Ship
- Ship starts with 250 HP and two cannons
- Cannons deal 70 damage
- Can be upgraded/repaired at shops

### Your Crew:
- Start with a pirate crew of three (100 HP/20 MP)
    - Captain
    - Swordsman (Close range)
    - Sniper (Long range)
- Once a crew member dies… they are gone for good!
- Disband and recruit crew members for gold at shops
- Maximum crew of 4 members (including the captain)
- Upgrade your crew members with new abilities

### Encounter Structure
- Player crew spawns on their ship, enemy crew spawns on their ship
- The ships are right next to each other and crew members can jump onto each others ships to fight/attack
- Player crew cannot use enemy cannons, enemy crew cannot use player cannons
- Each crew member's turns are broken into movement and using an attack/item/special
    - They can move or take an action in any order, skipping either part or both is also allowed
- Each crew member can move 5 spaces per turn, unless otherwise specified
- The sniper's nest of a ship is available, this is useful for snipers or anyone trying to hide
- If player crew members are on an enemy ship when destroyed they die as well

### Progression During The Run:
- Obtain gold and new abilities upon successfully defeating a rival crew
    - Sinking an enemy ship (and by consequence all enemies die) grants you 1x the loot (e.g 150 gold and a new move)
    - Defeating all enemies and sparing the enemy ship grants you 1.5x the loot (e.g 225 gold and a new move)
- Defeating an enemy grants the crew member who killed them XP
- XP is granted to all crew members of the player's ship upon clearing an encounter
- When the crew member levels up (on average once per encounter) 
- Upgrade your crew and ship at the shop
- Benefit or suffer at a random event!

### Map Structure:
- Runs are 8-9 nodes long (excluding the boss) with 5 fights guaranteed
- No two shops in a row, no two events in a row, guaranteed shop option before the boss
- Map view giving the player a choice of two to three options to go next, selects the adjacent node to go there. Previous nodes visited marked as complete and the player can't go backwards only forwards

### At Each Fight Node
- Player always starts first
- During a player’s turn they can move and take an action for each party member
- Each action you can either: 
    - Attack an enemy pirate
    - Use a special ability (consuming MP) 
    - Consuming an item
    - Fire the ship’s cannon to attack the enemy ship
- The player progresses past a fight encounter if they either defeat all enemy pirates or sink their ship.
- Likewise, the player loses if all their crew members die or their ship is destroyed.
- Plundering occurs if the enemy pirates are defeated (grants gold and/or random item) incentivizing players with high risk / high reward.

### At Each Shop Node
- Repair your ship (100 gold)
- Unlock new abilities (50 gold each)
- Buy items (health/mana restore (30 gold each)

### At Each Event Node
- Random event occurs with 60% good / 40% bad odds

### At The Final Boss Node
- Players must fight a Kraken to beat the game
- Kraken has 500 HP
- Players can target the Kraken’s tentacles to reduce its actions/damage
- Initial phase (Kraken can perform the following moves):
    - Tentacle slam (One player or the ship gets dealt 50 damage)
    - Kranky swipe (Whole team deals 20 damage)
    - Tentacle grab (One crew member can’t move for a turn)
- Enraged final phase (health below 30%)
    - Arms of fury (randomly attacks 3-5 times for 40 HP each)
    - Eyes of the sea (Performs two actions next turn)

## AI Architecture

### AI Agents

#### Technical Architect

- **Role**: Makes high level technical decisions for Deadwater Run
- **Input & Output**: Prompt -> Technical Architect Document
- **What the player sees**: A well polished game where systems work together in harmony

#### Gameplay Engineer

- **Role**: Implements the pirate actions and gameplay abilities
- **Input & Output**: Document -> MCP -> Updated code/blueprints
- **What the player sees**: The player’s captain shoots a cannon at the enemy’s ship during his action

#### Tools Engineer

- **Role**: Implements developer facing tools and tests allowing me to iterate faster avoiding rampant token costs
- **Input & Output**: Document -> Unreal Engine Tool / Test Suite
- **What the player sees**: A well polished game made by a developer that didn’t break the bank on LLM cost

#### AI Engineer

- **Role**: Implements enemy AI behavior and decision making
- **Input & Output**: Document -> MCP -> Updated code/blueprints
- **What the player sees**: Engaging final boss Kraken that provide just the right level of challenge

#### UI Engineer

- **Role**: Implements Deadwater Run’s UI
- **Input & Output**: Mockup -> MCP -> Widget Blueprints
- **What the player sees**: An intuitive user experience that doesn’t get in the way of the game

#### UI Juice Engineer

- **Role**: Adds animations and sound effects to all UI interactions
- **Input & Output**: Prompt -> MCP -> Updated Widget Blueprints
- **What the player sees**: The attack button pulses as you hover over the button.

#### Balance Designer

- **Role**: Refines and iterates on enemy difficulty and player upgrade cost
- **Input & Output**: Prompt -> Updated JSONs
- **What the player sees**: The player is low health and has to decide between healing or upgrading their gear.

#### QA Tester

- **Role**: Attempts to break the game. Finds bugs and issues
- **Input & Output**: Prompt -> Game -> Bug report and suggested fix
- **What the player sees**: Polished game with zero to minimal bugs

#### Sound Wrangler

- **Role**: Integrates sound assets in the game with relevant settings
- **Input & Output**: Prompt -> Sound .uassets
- **What the player sees**: The enemy plays a satisfying death sound upon dying

#### Release Engineer

- **Role**: Packages builds and prepares Steam releases (depot uploads, store page checklist, deadlines)
- **Input & Output**: Prompt -> Packaged build / release checklist
- **What the player sees**: A stable build on Steam, launched on time

#### Asset Scraper

- **Role**: Finds art, audio and fonts the Synty packs don't cover. Lists multiple options for approval; never imports
- **Input & Output**: Prompt -> Asset options report
- **What the player sees**: A consistent look and sound across the whole game

#### Working together

- All systems must work together - no system is implemented in a silo
- The Technical Architect sets the high-level architecture direction that the other agents build on
- The Gameplay Engineer also owns run systems (map, shop, events, captains, economy, run state) and Niagara VFX

## Technical Strategy

### Core Principal

- All agent deliveries to Deadwater Run must pass my approval. This means any creative decisions made must be approved by me. All code deliveries must be run by me with a plan first.
- All tasks must be clearly logged.

### Engine & Tools

- Deadwater Run will be developed in Unreal Engine 5.8 using Unreal Engine’s MCP. Epic’s Gameplay Ability System will be used to aid with gameplay abilities and turn based effects.

### Managing Agents

All agents will be operated using Claude Code + Unreal Engine MCP so agents can directly modify the game files minimizing human handholding. An orchestration agent will call the relevant agent needed. Context windows will be cleared upon every fresh session minimizing context rot.

### Assets

- Assets for Deadwater Run (outside of concept art) will be human crafted where possible. Synty’s Studios’ Pirate and VFX pack will be used and be much of the game’s budget outside of token cost
- Directories will be listed for these assets for agent lookup.

- Asset scraping agents will be used to locate additional assets in the case an appropriate asset is not available
- When scraping assets, multiple options must be listed and I will approve which is taken into the project.
