# 11 - Events

## Problem
- Event nodes: a random event with 60% good / 40% bad odds; ~10-12 events for v1.0.
- Brandon writes the event text and decides outcomes; he must be able to add events without code or AI.
- Outcomes touch run state (gold, crew HP, ship HP, items, abilities, XP, recruits) and must use the same state methods as the shop and rewards.

## Decision
| Option | Verdict |
|---|---|
| **A. `UDwEventDefinition` DataAssets with choices; each choice holds `Instanced` polymorphic `UDwEventOutcome` and `UDwEventRequirement` objects edited inline in the details panel** | **Chosen.** Authoring in the editor with dropdowns, validated, no code per event; outcome types are a small closed set |
| B. Events in JSON/DataTable rows | Rejected: polymorphic outcomes in rows are clumsy; FText authoring is worse |
| C. Event Blueprints with graph logic | Rejected: logic in Blueprint, untestable, slow to author 12 |

## Data (Data/ and Run/Events/)

`UDwEventDefinition : UDwDefinitionBase`
| Field | Notes |
|---|---|
| `FText Title`, `FText Body` (multi-line) | Brandon's writing |
| `TSoftObjectPtr<UTexture2D> Illustration` | Optional |
| `EDwEventValence Valence` | `Good` or `Bad` - used by the 60/40 roll |
| `float Weight` | Selection weight within its valence |
| `int32 MinLayer`, `MaxLayer` | Where on the map it can appear |
| `TArray<FDwEventChoice> Choices` | 1-3 choices |

`FDwEventChoice`: `FText Label`, `FText ResultText`, `TArray<TObjectPtr<UDwEventRequirement>> Requirements` (all must pass to enable the choice), `TArray<TObjectPtr<UDwEventOutcome>> Outcomes` (applied in order).

`UDwEventOutcome` (abstract, `EditInlineNew`, `DefaultToInstanced`) - `virtual void Apply(FDwEventContext&) const`, `virtual FText Describe() const` (for the UI's "you will..." line and data validation):

| Outcome | Parameters |
|---|---|
| `UDwEventOutcome_Gold` | `Delta` (negative allowed, floor 0) |
| `UDwEventOutcome_ShipHealth` | `Delta` |
| `UDwEventOutcome_CrewHealth` | `Delta`, `EDwCrewSelector` (`All`, `Random`, `Captain`, `Chosen`). Whether negative deltas can kill (and end the run on the captain, C10) is C10a; placeholder: Health floors at 1 |
| `UDwEventOutcome_GiveItem` | item, count |
| `UDwEventOutcome_AbilityChoice` | count (reuses the rewards ability-choice flow) |
| `UDwEventOutcome_GrantXp` | amount, selector (may queue level-ups) |
| `UDwEventOutcome_Recruit` | unit definition (fails gracefully if crew full) |
| `UDwEventOutcome_LoseCrew` | selector (never the captain unless Brandon decides otherwise) |
| `UDwEventOutcome_RandomOf` | weighted list of nested outcomes (gambles) |
| `UDwEventOutcome_Nothing` | none - the explicit "nothing happens" (added 2026-11-17, DW-205) |

`UDwEventRequirement` (abstract, instanced): `_MinGold`, `_CrewCountAtLeast`, `_CrewCountAtMost`, `_HasItem`, `_CaptainIs`.

- Changed 2026-11-17 (DW-205): `_CrewCountAtMost` added so a Recruit choice can be disabled when the crew is full (the content critic flagged full-crew Recruit choices as traps). `GiveItem` adds what fits under `MaxInventory` and reports the rest as left behind. `Recruit` joins at the shop recruit level (P8) and says so when there is no bunk. C10a decided a): Health and hull never drop below 1 from an event.

- Changed 2026-10-25: `UDwEventOutcome_CrewMana` is removed. MP refills at every fight start (C9), so a mana change between fights would have no effect.

`FDwEventContext`: `FDwRunState&`, `FRandomStream&` (`FDwRunRandom::ForEvent(Seed, Node)`), chosen crew id, economy row, progression table and the captain's XP multipliers; out: change lines, level-ups, ability offers. Outcomes never touch the world.

The pool: `DA_GameData.Events`, imported from `Tuning/Events.json` by the content script (`FDwEventImporter`; the content pipeline's event format). Layers in the JSON are 1-based as the player counts nodes; the definitions store them 0-based like encounters. Validation: Title, Body, 1-3 choices, each with a label, result text and at least one outcome, and at least one choice with no requirement (so nobody is stuck); the import fails without a Good and a Bad event.

## Flow (`FDwEventResolver`, Run/Events/, pure)
1. `Roll(State, Rng)`: valence = Good with `DT_RunTuning.GoodEventChance` (0.6), then a weighted pick among unseen events of that valence valid for the current layer; if none unseen, allow repeats; if none of that valence exist, use the other valence (validator warns).
2. `UDwRun` stores `FDwActiveEvent` (and adds the event to `SeenEvents`) and enters `Event`; `WBP_NodeShell` (`UDwNodeShellViewModel`) shows title, body and one button per choice with its outcomes' "you will" lines, disabled with the first failing requirement. A dedicated `WBP_Event` is the UI engineer's.
3. `ChooseEventOption(Index, CrewId)` -> `FDwEventResolver::Apply` -> the run stays in `Event` with `FDwActiveEvent` holding the choice and change lines; the screen shows the result text and changes. A choice with a `Chosen` outcome first asks the player for a crew member (`GetChoosableCrew`; `LoseCrew` never offers the captain).
4. `LeaveEvent()` (Continue) -> `Rewards` if `AbilityChoice` offered moves (the normal claim flow, no gold), else `LevelUp` if XP queued level-ups, else `Map`. When no event fits the layer the screen is "Calm water" and Continue goes straight on.

## Connections
- `09-Run-Map.md`: node entry and seeds (`ForNode`).
- `10-Economy-Shop-Loot.md`: gold, items, ability choice flow, recruit via `FDwCrewFactory`.
- `12-Progression.md`: XP and level-ups.
- `14-UI.md`: `WBP_Event` + `UDwEventViewModel`.
- `Editor-Tools.md`: event authoring guide, validation, "Play Event" quick launch.

## Build order (gameplay-engineer)
1. DW-205: definition, choice struct, outcome/requirement classes, resolver, 2 placeholder events (one good, one bad).
2. DW-306: the 10-12 events Brandon writes (data only).

## How it's tested
- Pure (`DeadwaterRun.Rules.Events.*`): valence split over 10,000 rolls within 60% +/- 1.5%; unseen events preferred; requirements disable choices; each outcome type applies and clamps correctly; `RandomOf` stable per seed.
- Data validation: every event has 1-3 choices, non-empty text, at least one outcome per choice (or an explicit "nothing happens"), valid layer range; at least one Good and one Bad event exist.

## What the player sees
- A short illustrated scene - a drifting wreck, a smuggler's offer - with 1-3 choices that clearly show any cost. Most of the time the sea is kind; sometimes it isn't.
