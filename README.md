# Deadwater Run

A turn-based pirate roguelike built in Unreal Engine 5.8 by one developer and a crew of coordinated AI agents.

## Combat Playable: DW-103 / 104 / 105 (2026-11-09)

| # | Ticket | Branch | What it is |
|---|---|---|---|
| 1 | **DW-103** Combat core | `orchestrator/dw-103-combat-core` | `UDwCombatSubsystem` (one validated `SubmitCommand` for player, AI, tests, simulator), `UDwTurnManager` (player first, enemies one by one, skips, durations), outcome rules (defeats first), death and sinking, replayable `FDwCombatRecord` |
| 2 | **DW-104** Core actions | `orchestrator/dw-104-core-actions` | Move (one per turn), payload attacks, cannons, items, presentation tasks with instant mode; placeholder content: `GA_*`, `DA_Ability_*`, items, units, `DA_Encounter_Placeholder` |
| 3 | **DW-105** Input and camera | `orchestrator/dw-105-input-camera` | `ADwPlayerController` + Enhanced Input (`IMC_Combat`), `UDwCombatInteraction` (select, path preview, targets, damage preview, cancel), `ADwCameraRig`; placeholder decks, HP labels and a text HUD until DW-107 |

**Play it:** open the project, press Play on `L_Sea`. A 3 v 3 starts on two 250 HP ships. Left-click a pirate, then a blue tile to move or a lit enemy to attack. Keys `1`-`6` pick an action, right-click cancels, `Tab` picks the next pirate, `Space` ends the turn, `WASD` pans and the wheel zooms. Enemies don't act yet: their AI is DW-106.

**Scripts** (first output line is PASS/FAIL):
- `./Tools/Build.ps1` then `./Tools/Test.ps1`: 145 tests
- `./Tools/CreateCombatContent.ps1`: rebuilds the placeholder content and syncs the DataTables it reads, headless and idempotent
- `./Tools/Capture.ps1 -Demo move|aim|cannon`: plays `L_Sea` and saves a screenshot to `Saved/Captures/`, so proof doesn't need the editor MCP

## First Build: Week 0 Foundation (2026-11-08)

The first three pieces from [Docs/Architecture/Roadmap.md](Docs/Architecture/Roadmap.md), built from the architecture docs.

| # | Ticket | Branch | What it is | Proof |
|---|---|---|---|---|
| 1 | **DW-002** Template cleanup + skeleton | `orchestrator/dw-002-cleanup` | Deleted the UE template code and content (Strategy, TwinStick, TopDown). New `Build.cs` and `.uproject` plugins (MCP/Terminal set to editor-only). `ADwGameMode`, `ADwMenuGameMode`, `ADwPlayerController`. Empty `L_MainMenu` and `L_Sea` levels set as the default maps | Editor build passes with 0 warnings. A headless script loads both maps and all 3 classes |
| 2 | **DW-003** Editor module + scripts | `orchestrator/dw-003-editor-module` | `DeadwaterRunEditor` module. `Tools/Build.ps1` and `Tools/Test.ps1` print `PASS`/`FAIL` on the first line and return non-zero on failure. Placeholder test | `BUILD PASS`, `TEST PASS 1/1`. Failure paths checked on purpose: a forced compile error gives BUILD FAIL, a wrong expectation gives TEST FAIL, and a filter that matches nothing also fails |
| 3 | **DW-004** Shared core layer | `orchestrator/dw-004-core-layer` | 88 native gameplay tags, log categories, shared types. `UDwDefinitionBase`, `UDwAssetManager`, `UDwGameData`. All 15 definition classes and 12 tuning row structs. `Tuning/*.json` holds every number from the design doc and is imported into the `DT_*Tuning` tables and `DA_GameData` | Editor and Game targets build with 0 project warnings. `TEST PASS 15/15`: tags, the Asset Manager, design numbers read back from the DataTables, and validation rules |

I can run them via: `./Tools/Build.ps1` then `./Tools/Test.ps1`. Set `$env:UE_ENGINE_DIR` if UE 5.8 is somewhere unusual.

### What We Caught

**Bugs found and fixed while building**
- **Every commandlet exited 1.** The project was already logging a `GameFeatureData` Asset Manager error at startup (it's in `Saved/Logs/DeadwaterRun_2.log` too). That error would have made every PASS/FAIL script fail. Fixed in `DefaultGame.ini`
- **The Asset Manager preloaded nothing in the editor.** At `StartInitialLoading` the asset registry is still scanning, so the eager load quietly loaded 0 types. It now runs after the scan, and a test fails if it ever stops working
- **"Zero warnings" couldn't be met on the Game target.** Epic's own 5.8.4 headers produce 19 deprecation warnings. `Build.ps1` now fails only on warnings in our files and reports the engine count separately
- **Doc 01's cleanup list was incomplete.** It missed `Content/__ExternalActors__` and `__ExternalObjects__`, the One File Per Actor (OFPA) data for the deleted template levels. Those are removed too
- **UE 5.8 isn't installed in the default folder.** It's at `%USERPROFILE%\Epic Games\UE_5.8` and isn't in the launcher manifest, so the scripts look for it instead of assuming a path


- **Design doc:** [DeadwaterRun.md](DeadwaterRun.md) · **Scope:** [Docs/Scope.md](Docs/Scope.md) · **Architecture:** [Docs/Architecture/](Docs/Architecture/)

## The Game

Lead a pirate crew across the legendary seas of Aquadia and escape by defeating the Kraken.

- **The twist:** every ship has a health bar too. Each fight asks one question:
  - **Sink the ship** - faster fight, 1x loot
  - **Kill the crew** - riskier, but you plunder the ship for 1.5x loot
- **Pick a captain** - each has 2 combat abilities and a run rule that changes how you play:
  - **Team Leader** - crew synergy moves, bodyguarding, Rally Cry, Grapple Hook; crew levels twice as fast
  - **Fearmongerer** - stacks Fear on enemies until they cower or surrender (a third way to win)
  - **The Conqueror** - a sword-only powerhouse with an extra move; grows stronger with every kill
- **Turn-based boarding combat** - ships sit side by side; crew jump between decks, climb the sniper's nest, or fire cannons. Each crew member gets a move and an action (attack, ability, item, or cannon) per turn
- **Permadeath crew** - start with a Captain, Swordsman and Sniper. The fallen are gone for good; recruit replacements at shops (max crew of 4)
- **Branching map** - 8-9 nodes of Fights, Shops and Events (60% good / 40% bad), always moving forward
- **The Kraken** - a two-phase final boss. Cut down its tentacles to limit its attacks before it enrages
- **~30 minute runs** - built for "one more run"

## How It's Built: Coordinated AI Agents

Deadwater Run is built by **11 specialist AI agents** running in Claude Code, each with one area of ownership. They don't work in silos - they share one architecture, one set of GAS tags/attributes, one data layout and one run state.

- **Orchestrator** (main Claude Code session) - turns the scope into tickets and routes each to the agent that owns it. Runs independent tickets in parallel, each in its own git worktree
- **Technical Architect** - writes the architecture docs in `Docs/Architecture/` that every other agent builds from
- **Engineers** - modify the Unreal project directly through the **Unreal Engine MCP**, minimizing hand-holding
- **Tools Engineer** - builds the test harness and headless run simulator, so balance and QA can iterate without burning LLM tokens
- **QA Tester** - verifies every delivery and sends bug reports back to the owning agent
- **Brandon (human)** - approves every plan, every creative decision and every delivery. Agents never push or open PRs

### Agent Roster

| Agent | Input → Output | Owns |
|---|---|---|
| Technical Architect | Prompt → Architecture doc | Module layout, how systems connect, GAS structure, data layout |
| Gameplay Engineer | Doc → MCP → Code/Blueprints | Combat, GAS abilities, map, shop, events, captains, economy, run state, Niagara VFX |
| AI Engineer | Doc → MCP → Code/Blueprints | Rival crew decisions, Kraken behavior |
| UI Engineer | Mockup → MCP → Widget Blueprints | HUD, map, shop, menus |
| UI Juice Engineer | Prompt → MCP → Widget Blueprints | UI animations and UI sounds |
| Tools Engineer | Doc → Editor tool / Test suite | Editor tools, automation tests, headless simulator |
| Balance Designer | Simulator data → Updated JSON/DataTables | Difficulty, costs, loot, ability numbers |
| Sound Wrangler | Approved audio → Sound .uassets | Sound cues, MetaSounds, mixes, combat sound hooks |
| Asset Scraper | Prompt → Asset options report | Finds art/audio/fonts Synty doesn't cover (never imports) |
| QA Tester | Game → Bug report + suggested fix | Breaking the game, verifying deliveries |
| Release Engineer | Build → Packaged build / Checklist | Packaging, Steamworks, store page, deadlines |

### Agent Roles, Connections and Data Flow

```mermaid
flowchart TD
    Brandon(["👤 Brandon<br/>approves plans, creative calls, deliveries"])
    Scope[/"Docs/Scope.md"/]
    Orch{{"Orchestrator<br/>main Claude Code session"}}

    Brandon -->|sets scope| Scope
    Scope -->|broken into tickets| Orch
    Orch -->|"batched reports,<br/>creative questions"| Brandon

    subgraph Design["1 · Design"]
        TA["Technical Architect"]
        ArchDocs[/"Docs/Architecture/*.md"/]
        TA -->|writes| ArchDocs
    end

    subgraph Assets["Asset Sourcing"]
        AS["Asset Scraper<br/>options report, read-only"]
    end

    subgraph Build["2 · Build via Unreal Engine MCP"]
        GE["Gameplay Engineer<br/>combat · GAS · run systems · VFX"]
        AIE["AI Engineer<br/>rival crews · Kraken"]
        UIE["UI Engineer<br/>UMG screens"]
        UIJ["UI Juice Engineer<br/>UI animation · UI sound"]
        SW["Sound Wrangler<br/>sound assets · mixes"]
        UIE -->|widgets| UIJ
        GE -->|GameplayCues| SW
    end

    subgraph Tune["3 · Test & Tune"]
        TE["Tools Engineer<br/>tests · headless simulator"]
        BD["Balance Designer<br/>tuning JSON / DataTables"]
        TE -->|simulator results| BD
    end

    Game[("Unreal Project<br/>Deadwater Run")]

    subgraph Ship["4 · Verify & Ship"]
        QA["QA Tester"]
        RE["Release Engineer"]
        Steam(["Steam"])
        RE -->|depot upload| Steam
    end

    Orch -->|architecture tickets first| Design
    ArchDocs -->|every agent builds from| Build
    ArchDocs --> Tune
    Orch -->|tickets| Build
    Orch -->|tickets| Tune
    Orch -->|tickets| Assets
    Orch -->|tickets| Ship

    AS -->|asset options| Brandon
    Brandon -->|approved assets| SW

    Build -->|code · blueprints · uassets| Game
    BD -->|tuning data| Game
    TE -->|test harness| QA
    Game -->|playtest| QA
    Game -->|packaged build| RE

    QA -.->|bug reports + fixes| Orch
    TA -.->|architecture gaps| Orch
    Brandon -->|approve & push| Game
```

### Rules Every Agent Follows

- **Plan first** - code work starts with a plan Brandon approves
- **Proof required** - every delivery includes build result, test output and a screenshot or clip
- **Creative calls stay human** - agents offer 2-3 options; Brandon picks
- **Tracked** - branches are `<agent>/<topic>`, commits end with `Agent: <name>`, and a hook logs every edit and MCP call to `.claude/logs/agent-log.jsonl`
- **Stay in scope** - anything outside `Docs/Scope.md` goes on the post-launch list

Full details: [Docs/Orchestration.md](Docs/Orchestration.md) · Agent definitions: [.claude/agents/](.claude/agents/)

**Architecture gaps for the Technical Architect**
- **Prices are defined twice.** Doc 10 puts `AbilityUnlockCost`, `ItemCost` and `RecruitCostDefault` in `DT_EconomyTuning`. Doc 13 also puts `ShopPrice`, `Price` and `RecruitCost` on each row. That breaks rule 1 (one source of truth). For now the design numbers (50/30) live only in the economy table, and the per-row fields are 0
- **Ship upgrade price has no home.** Doc 06 says upgrades have a "tuning row (price)", but doc 13 lists no ship-upgrade table
- **AI ownership overlaps.** DW-004 says "all tuning rows and definitions", but doc 00 gives `FDwAiTuningRow` and `UDwAiProfile` to ai-engineer (DW-106). I only stubbed the profile class so encounters can reference it
- **`FDwUnitHandle` was never specified.** I made it an `int32` id, where `INDEX_NONE` means invalid
- **Enemy ship stats aren't in the design doc.** Only the player ship (250 HP, 70 cannon) has numbers. Enemy ships arrive with DW-305

**Design questions**
- **Veteran Crew (K7):** the design text says both "+50% XP" and "levels twice as fast". The JSON has both literally, which is about 3x speed
- **Schedule:** the Roadmap has "Full run playable" due today (Nov 8), but Week 0 code had not started. The store page is due Nov 12
