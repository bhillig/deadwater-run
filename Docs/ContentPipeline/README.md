# Content Loop - Deadwater Run

I built a self-correcting content loop for Deadwater Run's map events. A **Generator** writes a batch, an **Evaluator** scores it 0-100 with a reason, and a **Refiner** fixes what scored low. It keeps going until the score clears the bar or stops improving. There's no human step inside the loop. I start it by asking the **content-director** agent for content, and nothing runs unless I do.

```
 my request ──► content-director ──► briefs.json
                                          │
                                          ▼
 GDD, voice sheet, store page,     lib/style.js ──► style-guide.md (21 rules, every one cited, stranger test)
 14-UI art direction, 11-Events,   lib/facts.js ──► game facts (outcomes, items, captains, guardrails)
 tuning, Tuning/Events.json              │  (both rebuilt from the repo on every run)
                                         ▼
 event-writer (Generator) ──► draft ──► lib/checks.js (code) ──► content-critic (Evaluator) ──► score 0-100 + reason
                                ▲                                                                   │
                                │        score >= 85, no blocker, no code major ──► accepted ◄──────┤
                                │                                                                   ▼
                                └────────── content-refiner (Refiner), always from the BEST draft so far
                                            stops on its own: accepted / plateau / max rounds / budget
```

## What I asked for, and where it is

| Requirement | How it's met | Where |
|---|---|---|
| Style guide pulled from my GDD and game, nothing generic | Every rule is compiled from my docs and quotes its source `file:line`. The build fails if a quote moves | [`lib/style.js`](../../Tools/ContentPipeline/lib/style.js) → [`style-guide.md`](style-guide.md) |
| A stranger can tell exactly what game it's for | **Stranger test (code):** every rule has to name something only Deadwater Run has (Aquadia, the Kraken, the three captains and their abilities, sink vs plunder, gone for good, the Ship's Log). Genre words like gold, ship and crew don't count. A test proves a generic guide fails | `strangerTest()`, tested |
| ...and the content passes it too | The evaluator scores `specificity` as the stranger test. It's also a **gate**: under 70, the batch is capped at 69 and can never be accepted, however polished | [`lib/score.js`](../../Tools/ContentPipeline/lib/score.js) |
| Evaluator emits a score, not pass/fail, with a reason | Five dimensions, each 0-100 with a reason, plus a score per event and a one-line stranger test. Code combines them the same way every time and lists every term in the reason | `scoreEvaluation()` |
| Generator → Evaluator → Refiner, no human | `event-writer` → `content-critic` → `content-refiner`, run by `content-pipeline.js run` | [`content-pipeline.js`](../../Tools/ContentPipeline/content-pipeline.js) |
| It's an agent | `content-director` turns my request into a brief, proves the tool, runs the loop and reports the scores | [`.claude/agents/content-director.md`](../../.claude/agents/content-director.md) |

## The score

`score = weighted mean of 5 dimensions - code-check penalties`, then the caps.

| Dimension | Weight | What it judges |
|---|---|---|
| `rules` | 20 | Only outcomes, items, units, captains and numbers that exist |
| `text_data` | 15 | Text is true to the data, in every RandomOf branch |
| `design` | 20 | Real decisions, sink-or-plunder tension, ship vs crew costs, fair for a ~30 min run |
| `voice` | 20 | My voice rules, the store page lines and the shipped pool; the Ship's Log aesthetic |
| `specificity` | 25 | The stranger test |

- Code checks take off 15 / 6 / 1 per blocker / major / minor, so the critic can't talk its way past a broken event.
- Any blocker caps the score at 49. Specificity under 70 caps it at 69.
- **Accepted** = 85+, no blocker anywhere, no code major. Anything else ends as `best effort` with the reason it stopped.

## Keeping it honest with no human in the loop

- **Calibration.** Before the loop trusts the critic, it scores a generic pirate control batch against four of my shipped events. The control is well-written and valid: treasure map, merchant galleon, mutiny, storm. Only the stranger test should sink it. If calibration fails, the loop refuses to run. It recalibrates on its own when anything the critic depends on changes, since `stamp.json` holds a hash of its prompt, rubric, agent definition and sources.
- **Best draft wins.** The refiner always starts from the best-scoring draft, so a bad revision can't make the result worse. A test plants a refiner that makes everything worse, and the final is still round 1.
- **Locks.** Events that already scored 85+ with no findings are locked. If the refiner touches one anyway, code puts it back.
- **It stops on its own:** accepted, no gain for 2 rounds, 5 rounds, or the budget. Running out of budget keeps the best draft instead of crashing.

## Results

Calibration passed. The control scored **52** with specificity **18**. My shipped events scored **67** with specificity **70**. [Report](runs/calibration/report.md)

| Batch | Score by round | Final | Status |
|---|---|---|---|
| [`early-waters`](runs/early-waters/report.md) | 69 → 84 → 84 → 83 | **84** | best effort (plateau) |
| [`deep-water`](runs/deep-water/report.md) | 79 → 82 → 84 → 84 → 88 | **88** | accepted |
| [`kraken-reach`](runs/kraken-reach/report.md) | 82 → 85 | **85** | accepted |

12 events, 7 Good / 5 Bad, no code findings, no pool findings. $6.23 for the whole run including calibration (24 agent calls, Opus 5.5). [Pool summary](runs/summary.md)

**Do they sound like Deadwater Run?** Yes, and not like any other pirate game. Most of them are built on the hook itself:
- *The Grounded Sloop:* "Fire the cannons" for 60 gold, or "Board her" for 90 and cuts across the crew. The Fearmongerer gets a third option: "Smoke rolls off your beard and across the reef. Her crew hand up the hold and ask to keep their buckets."
- *The Scuttled Sloop:* "Whoever sank her took the quick way and never set foot on her deck, so her hold is still full." That's the 1x sink rule used as a story.
- *The Locked Hulls:* "Both crews went for the plunder instead of the cannons, and nobody was left to take it."
- The captains play to the GDD. The Team Leader takes the hits meant for his crew and uses the Grapple Hook, and the Conqueror goes over alone with only his sword.

**What I'd still change:** early-waters stalled at 84. The critic called *The Empty Hulk* "the shipped Drifting Wreck again", and the refiner couldn't get away from the wreck premise. Some RandomOf results are written vaguely so they read true for both branches.

## Problems I hit (and fixed)

- **The old pipeline was already broken.** `11-Events.md` gained `Nothing` and `CrewCountAtMost`, and the facts guard stopped every run. I added both, plus a code check that an ungated `Recruit` is a trap.
- **First calibration failed.** The generic control scored 58 because good writing carried it, even at specificity 20. With specificity worth only 25%, a polished generic batch could reach about 87 and be accepted, which is exactly what I didn't want. That's why the stranger test is now a gate.
- **One rule said more than my docs.** S07 said a Good event *must* cost something. My voice sheet only says the sea is "mostly kind but never safe". I fixed the rule to say Good events *can* cost something, and the critic stopped marking my shipped events down for it.
- **Honest note:** my own shipped pool lands right at the stranger floor (68-70). The critic calls *Squall Line* and *Deserters in a Longboat* generic. I agree with it.

## Running it

Normally I just ask the agent: *"content-director: 4 Kraken-omen events for layers 7-9"*. By hand:

```powershell
node Tools/ContentPipeline/test/pipeline.test.js                     # 19 tests, stub agents, free -> PIPELINE TEST PASS 19/19
node Tools/ContentPipeline/content-pipeline.js style                 # rebuild style-guide.md (stranger test)
node Tools/ContentPipeline/content-pipeline.js calibrate             # critic: generic control vs shipped events
node Tools/ContentPipeline/content-pipeline.js run --batch deep-water   # one brief through the loop (launches agents, costs money)
node Tools/ContentPipeline/content-pipeline.js run --all             # every brief + pool summary
node Tools/ContentPipeline/content-pipeline.js check <draft.json>    # code checks only
```

Options: `--threshold 85`, `--max-rounds 5`, `--patience 2`, `--total-budget-usd 25`, `--budget-usd 3` (per call). To change a rule I edit the source doc (GDD, `voice.md`, `14-UI.md`, tuning). The style guide, the facts and the calibration all follow on the next run.

## Limits

- **Drafts only.** Nothing goes into `Content/` or `Tuning/`. What makes it into the game is still my call.
- **One content type:** map events. The style guide and critic aren't event-specific, so crews (N2) or abilities (N1) would need their own generator and data checks.
- **The critic is an LLM.** The same draft can move a couple of points between calls. The code penalties, caps, locks and calibration are there so that noise can't ship generic or broken content.
- The guardrails are still placeholders derived from tuning, for the balance-designer to confirm.

The first version (writer → critic → one revision → me) and its outputs are in [`out/`](out/summary.md).
