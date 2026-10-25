# 16 - Shell: Levels, Menu, Settings, Pause, Run Summary

## Problem
- Must ship: main menu, settings, pause, run summary. No mid-run saves (runs are ~30 minutes).
- Run state must survive the menu -> game travel; quitting mid-run ends the run.
- Settings must persist and drive audio volumes, display and game speed.

## Decisions

### D1 - Levels
| Option | Verdict |
|---|---|
| **A. Two levels: `L_MainMenu` (menu backdrop) and `L_Sea` (the whole run: map, combat, shop, events are phases inside one level; combat boards spawn and despawn)** | **Chosen.** No loads between nodes; one travel per run; the simulator never needs a level |
| B. A level per node type (map level, combat level, shop level) | Rejected: load times every node, state hand-off on each travel |
| C. One level for everything including the menu | Rejected: menu and run lifetime blur; harder to guarantee a clean run start |

### D2 - Persistence
| Option | Verdict |
|---|---|
| **A. Only settings persist (`UDwGameUserSettings` -> `GameUserSettings.ini`); runs never save** | **Chosen.** Matches Scope; meta-progression is cut |
| B. Save the run on quit | Rejected: Scope says no mid-run saves |

## Classes

| Class | Folder | Owner | Responsibility |
|---|---|---|---|
| `ADwMenuGameMode : AGameModeBase` | Core/ | gameplay-engineer | Menu level: shows the main menu via the UI manager, ends any stale run |
| `ADwGameMode : AGameModeBase` | Core/ | gameplay-engineer | `L_Sea`: binds `UDwRun::OnPhaseChanged`, starts/ends encounters, positions the camera rig for map vs combat, handles dev quick-launch (`UDwDevSettings`, PIE only) |
| `UDwRunSubsystem` | Run/ | gameplay-engineer | `StartRun` (then `OpenLevel(L_Sea)`), `EndRun` (summary data kept until the summary closes), `AbandonRun` |
| `UDwGameUserSettings : UGameUserSettings` | Shell/ | gameplay-engineer | Volumes (Master, Music, SFX, UI), `GameSpeed` (`EDwGameSpeed`: `Normal` 1x, `Fast` 1.5x, `Fastest` 2x - presentation playback rate only, never rules, U1), `bScreenShake`, `HighlightPalette` (`Default`, `ColorblindSafe`, U3), display settings inherited (resolution, window mode, vsync); `ApplyAudio()`, `ApplyPresentation()` |
| Screens (`WBP_MainMenu`, `WBP_CaptainSelect`, `WBP_Settings`, `WBP_Pause`, `WBP_Confirm`, `WBP_RunSummary`) and their view models | UI/ | ui-engineer | Layout and intents only |

- `Config/DefaultEngine.ini`: `GameUserSettingsClassName=/Script/DeadwaterRun.DwGameUserSettings`, `GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient`, default maps.

## Flows
- **New run:** Main menu -> Captain select -> Set Sail -> `UDwRunSubsystem::StartRun(Captain, Seed)` -> `OpenLevel(L_Sea)` -> map.
- **Pause:** `IA_Pause` -> `WBP_Pause` on `UI.Layer.GameMenu` -> `SetGamePaused(true)`. Turn logic is event-driven, so pausing only freezes presentation. Options: Resume, Settings, Abandon Run (confirm), Quit to Desktop (confirm, run lost).
- **Settings list (U3, decided):** resolution, window mode, vsync, 4 volumes, game speed (1x/1.5x/2x), screen shake, colourblind-friendly highlights.
- **Abandon (U5, decided):** `UDwRun::Abandon()` -> stats finalised -> `WBP_RunSummary` (defeat style, "Abandoned").
- **End of run:** Victory (Kraken) or Defeat (ship sunk, crew dead, or captain dead) -> run summary -> Main menu or New run. Summary fields (U4, decided): captain, seed, nodes cleared, fights by outcome (sunk/plundered/surrendered), gold, crew lost, kills, time - all from `FDwRunStats`, plus the end reason as the headline.
- **Quit or crash mid-run:** run is gone by design; nothing to clean up because nothing was saved.

## Connections
- `09-Run-Map.md`: run lifetime and phases.
- `14-UI.md`: screens and layers.
- `15-Audio-VFX.md`: volume application, pause mix, menu music state.
- `18-Packaging-Release.md`: default maps and cook list.

## Build order
1. DW-002 (gameplay-engineer): empty `L_MainMenu` and `L_Sea`, game mode stubs, default maps.
2. DW-201 (gameplay-engineer): run start/travel/end, abandon.
3. DW-210 (ui-engineer): menu, captain select, settings, pause, confirm, summary screens.
4. DW-216 (gameplay-engineer): `UDwGameUserSettings` + apply functions; settings view model reads/writes it.

## How it's tested
- Test (`DeadwaterRun.Shell.*`): settings round-trip to ini (including game speed and highlight palette); `ApplyPresentation` sets the playback rate to 1.0/1.5/2.0; a seeded fight gives the same `FDwCombatResult` at every game speed; `ApplyAudio` sets each class override; abandon produces summary stats.
- QA: start -> abandon -> new run starts clean (gold, crew, map reset); quit from pause; settings persist across restarts; Esc behaviour on every screen.

## What the player sees
- A pirate-themed main menu, a captain select with each captain's rules spelled out, settings that stick, a pause menu that never loses their place, and a run summary that tells the story of the voyage and makes them want one more run.
