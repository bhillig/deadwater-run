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
| `UDwEventOutcome_CrewHealth` | `Delta`, `EDwCrewSelector` (`All`, `Random`, `Captain`, `Chosen`) |
| `UDwEventOutcome_CrewMana` | `Delta`, selector |
| `UDwEventOutcome_GiveItem` | item, count |
| `UDwEventOutcome_AbilityChoice` | count (reuses the rewards ability-choice flow) |
| `UDwEventOutcome_GrantXp` | amount, selector (may queue level-ups) |
| `UDwEventOutcome_Recruit` | unit definition (fails gracefully if crew full) |
| `UDwEventOutcome_LoseCrew` | selector (never the captain unless Brandon decides otherwise) |
| `UDwEventOutcome_RandomOf` | weighted list of nested outcomes (gambles) |

`UDwEventRequirement` (abstract, instanced): `_MinGold`, `_CrewCountAtLeast`, `_HasItem`, `_CaptainIs`.

`FDwEventContext`: `FDwRunState&`, `FRandomStream&`, chosen crew id, out log of applied changes (for the result text and `FDwRunStats`). Outcomes never touch the world.

## Flow (`FDwEventResolver`, Run/Events/, pure)
1. `Roll(State, Rng)`: valence = Good with `DT_RunTuning.GoodEventChance` (0.6), then a weighted pick among unseen events of that valence valid for the current layer; if none unseen, allow repeats; if none of that valence exist, use the other valence (validator warns).
2. `UDwRun` stores `FDwActiveEvent` and enters `Event`; the event view model shows title, body, choices (disabled with a reason when requirements fail).
3. `ChooseEventOption(Index)` -> `FDwEventResolver::Apply` -> result text + change list -> back to `Map` (or `LevelUp` if XP queued level-ups).

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
