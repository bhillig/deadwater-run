# Dynamic Content Pipeline - Deadwater Run

For this assignment I built a content pipeline for Deadwater Run around **map events**. A writer agent reads my GDD and drafts events. A critic agent checks them against the game, the writer revises once, and the drafts come back to me for approval. I ran it on three briefs and got a draft of the whole v1.0 event pool: **12 events in 3 batches**, 7 Good / 5 Bad (my scope calls for ~10-12 at 60/40).

```
 Tools/ContentPipeline/briefs.json           lib/facts.js  (read from the repo on every run)
   (what to write)                     ┌──── GDD "Game Mechanics", 11-Events.md, 09-Run-Map.md,
        │                              │     DwEventDefinition.h, DwDataTypes.h, Tuning/*.json, voice.md
        ▼                              ▼
  event-writer agent ──► draft ──► lib/checks.js (code) ──► content-critic agent ──► findings
        ▲                                                                              │
        └──────────── blocker/major findings (code + critic), 1 revision ◄─────────────┘
                                                   ▼
                     Docs/ContentPipeline/out/<batch>/report.md   (drafts only - nothing goes into Content/)
```

## Deliverables

| # | Deliverable | Where |
|---|---|---|
| 1 | Content agent (reads the GDD, generates content) | [`.claude/agents/event-writer.md`](../../.claude/agents/event-writer.md). I kept it to one content type, events: title, body, 1-3 choices, outcomes and result text, in my `UDwEventDefinition` format |
| 2 | Critic agent | [`.claude/agents/content-critic.md`](../../.claude/agents/content-critic.md). I made this one generic so it can review any content type later. It's read-only and returns blocker/major/minor findings plus a voice score |
| 3 | Pipeline | [`Tools/ContentPipeline/`](../../Tools/ContentPipeline/): facts, code checks, the revision loop and 10 tests |
| 4 | Three generated outputs | [`out/early-waters`](out/early-waters/report.md), [`out/deep-water`](out/deep-water/report.md), [`out/kraken-reach`](out/kraken-reach/report.md), plus the [pool summary](out/summary.md) |
| 5 | Voice sheet the critic scores against | [`voice.md`](voice.md) |

I added both agents to my roster in `DeadwaterRun.md`, `Scope.md` and `Orchestration.md`.

## What I generated

| Batch | Brief | Events | Highlights |
|---|---|---|---|
| [`early-waters`](out/early-waters/report.md) | Layers 1-3. Low stakes; teach the player that events are trade-offs | 3 Good, 1 Bad | The Drifting Wreck, A Peddler's Sloop, Castaway on a Spar, The Hidden Shoal |
| [`deep-water`](out/deep-water/report.md) | Layers 4-6. Costs bite; touch the crew roster; one gamble | 2 Good, 2 Bad | The Rival's Cache, Deserters in a Longboat (recruit), Squall Line, Mutterings Below Deck (can cost a crewmate for good) |
| [`kraken-reach`](out/kraken-reach/report.md) | Layers 6-9. Kraken omens; one `CaptainIs` choice per captain | 2 Good, 2 Bad | The Tally Rock (Conqueror), Wreckers Among the Hulls (Fearmongerer), Something Under the Keel (TeamLeader) |

Each batch folder keeps the full trail: `draft-1.json` → `review-1.json` → `draft-2.json` → `review-2.json` → `final.json`. The `calls/` folder holds the raw agent replies (cost, turns, model usage). Here's one final event in the authoring format from my [Editor-Tools.md](../Architecture/Editor-Tools.md):

```
Title:    The Tally Rock
Valence:  Good      Layers: 6-9        Weight: 1
Body:     A black rock stands alone in the swell, cut all over with names. Every crew that sailed on to the
          Kraken stopped here first. Under a cairn at its foot sits a strongbox and a page of notes, weighted with a stone.
Choice 1: Open the strongbox  -> Outcomes: Gold +120  -> Result: Old coin, still dry. Whoever left it isn't coming back for it.
Choice 2: Read the notes aloud  -> Outcomes: GrantXp 60 (All)  -> Result: Scrawled warnings about tentacles, timing and where to stand. The crew make you read it twice.
Choice 3: Keep the notes to yourself  -> Requires: CaptainIs Conqueror -> Outcomes: GrantXp 80 (Captain); Gold +60
          -> Result: You read every page alone, then pocket half the coin and leave the rest under the cairn. The crew don't need to know how it ends. You'll be the one ending it.
```

## Does it sound like Deadwater Run?

**Mostly, yes.** The critic gave every batch **4/5** in both rounds against [`voice.md`](voice.md)

- **What I liked:** it's written in second person and present tense. It's close to the tone I want. Lines I like: *"The hull barely notices. The crew's shoulders do."*, *"Small, but it's yours."*, *"Whoever left it isn't coming back for it."* There are no stage-pirate words, no magic and no lore dumps. The events also use the game's real systems: permadeath (*Mutterings Below Deck* can lose a crewmate for good), Fear (the Fearmongerer "lights the fuses in his beard", straight from my GDD) and the Kraken (the Tally Rock notes are about tentacles).
- **What I didn't like:** some result texts for gambles describe both outcomes at once (*"comes up with a full chest, or comes up empty-handed"*). That's a limit of the data format I generated, since a choice has only one `ResultText` (see open questions). There's also some melodrama (*"You'll be the one ending it."*) and a few phrases repeat across batches ("the sea decides").
- For now, a 4/5 means is good. Once I finalize that sheet, every later review follows it.

## What the critic caught

Across the 3 batches the critic raised **10 blockers in round 1**, and 6 remained after the first revision. It also raised 30 minor findings, and the code checks raised 4 minor format issues. The writer fixed or dropped most of them. The rest are in each report for me to decide. All three batches end as **"needs Brandon"** because I set the loop to one revision: after that, the call is mine, not the critic's.

| What it caught | Example (batch) |
|---|---|
| **Gambles with no real downside** | *Drifting Wreck*: diving the hold averaged 84 gold against a sure 40, and its worst case was barely worse. The writer changed the bad branch to 0 gold and -30 HP (early-waters) |
| **Dead buttons and traps from game rules** | *Castaway on a Spar* and *Deserters in a Longboat*: `Recruit` silently does nothing with a full crew of 4, but the button stays enabled. In *Deserters* it still took 40 gold. My event format has no requirement that can gate it, so this points at a missing feature, not just bad writing (early-waters, deep-water) |
| **Bad events that are free when you're broke** | Gold floors at 0, so a choice that pays 80 gold with no `MinGold 80` costs a broke player nothing. The critic found this 3 times, and the writer added `MinGold` each time (early-waters, kraken-reach) |
| **Strictly dominant choices** | The Fearmongerer's choice in *Wreckers* gave +40 gold for free in a **Bad** event. It took two rounds: the first fix still beat every other option in every branch (kraken-reach) |
| **Text that contradicts the data** | **Blocker:** the revised *Wreckers* result says you got gold *and* got rammed, but each RandomOf branch applies only one of them. *"Pay for the whole lot"* promised the medicine chest, but the outcome gives only hull repair (kraken-reach) |
| **Bugs introduced by the revision** | In kraken-reach, round 2 found a new blocker *created by the writer's fix*. That's why I have the critic re-review every revision |
| **Cross-checking against my docs** | The early-waters critic asked whether crew HP carries between fights. The deep-water critic answered it from my docs (*"It does: C9"*) and confirmed the costs are real |

**The code checks** (`lib/checks.js`) only found minor format issues in the real runs, because the writer gets the same facts and stayed within them. Their value is the guarantee, which the tests prove: they plant 7 kinds of mistakes and each one is caught. The planted mistakes are a removed outcome (`CrewMana`), an unknown item, "Pay 40 gold" on a -30 outcome, `LoseCrew` on the captain, an event where every choice has a requirement, +500 gold, and "matey". The critic agent covers what code can't judge: design, fairness and voice.

## Problems I hit (and fixed)

- **`claude -p --agent` ignores `--json-schema`:** the reply had no `structured_output`. The writer happened to match the schema, but the critic made up its own keys, and my first two runs stopped. **Fix:** I now put the schema and an example shape in every prompt. `callAgentChecked()` verifies the shape and retries once with the errors. A test covers this path, and the raw replies are saved in `calls/` so a failure can be traced.
- **Facts parser:** the regex for the requirement list first matched an earlier mention of the class. It's now anchored to the start of the line. The facts also stop the run if any source changes shape (tested), so the critic never checks against nothing.

## Cost

$2.36 for the three batches (12 agent calls, $0.13-0.27 each, Opus 5.5), plus about $1 on the two failed runs before the schema fix. Each call is capped with `--max-budget-usd 3`, and the whole run with `--total-budget-usd 20`.

## Running it

```powershell
node Tools/ContentPipeline/content-pipeline.js facts                      # the game facts both agents get
node Tools/ContentPipeline/content-pipeline.js check <draft.json>         # code checks only, no agents
node Tools/ContentPipeline/content-pipeline.js run --batch deep-water     # one batch (launches agents, costs money)
node Tools/ContentPipeline/content-pipeline.js run --all                  # all briefs + pool summary
node Tools/ContentPipeline/content-pipeline.js report                     # rebuild out/summary.md
node Tools/ContentPipeline/test/pipeline.test.js                          # 10 tests, stub agents -> PIPELINE TEST PASS 10/10
```

To add a batch, I add a brief to `briefs.json`. To change a rule, I edit the repo (tuning, `11-Events.md`, `voice.md`). Both agents pick up the change on the next run, with no prompt edits.

## Limits

- **Drafts only.** The pipeline creates no `DA_Event_*` assets. The event classes (DW-205) aren't built yet, and I write and approve every event that goes into the game. The JSON maps 1:1 to `UDwEventDefinition`/`FDwEventChoice`, so an importer for DW-306 could read `final.json` directly.
- **The guardrails are placeholders** derived from tuning: an event pays at most a fight's gold (150) and hurts at most one cannon hit (70) or half a crewmate's HP (50). I'll have the balance-designer confirm them with simulator data.
- **One revision per batch** by default (`--revisions N`). More rounds cost more, and the critic keeps finding new issues in each revision, so I'm the one who decides when a batch is done.
- **One writer, one content type.** Enemy crews (N2) and abilities (N1) would each get their own writer agent. The critic is already generic, so I'd reuse it.
