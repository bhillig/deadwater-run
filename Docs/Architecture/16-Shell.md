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

### Dev entry points (DW-201, not in Shipping)
- `-DwCaptain=<TeamLeader|Fearmongerer|Conqueror> [-DwSeed=<n>]` on an L_Sea launch starts a run without the menu; without it L_Sea still starts the dev fight (`bStartDevFight`).
- `ADwHUD` can draw a text readout of the run over the screens (phase, crew, ship, gold, numbered routes); off by default since DW-211, `dw.Run.Readout 1` shows it.
- Console: `Dw.Run.Start [Captain] [Seed]`, `Dw.Run.Choose <n>`, `Dw.Run.Abandon`. Fight cheats stay with DW-111.
- `Tools/Capture.ps1 -Demo run` plays map -> fight -> map -> fight headless and saves a frame of each.

## Flows
- **New run:** Main menu -> Captain select -> Set Sail -> `UDwRunSubsystem::StartRun(Captain, Seed)` -> `OpenLevel(L_Sea)` -> map.
- **Pause:** `IA_Pause` -> `WBP_Pause` on `UI.Layer.GameMenu` -> `SetGamePaused(true)`. Turn logic is event-driven, so pausing only freezes presentation. Options: Resume, Settings, Abandon Run (confirm), Quit to Desktop (confirm, run lost).
- **Settings list (U3, decided):** resolution, window mode, vsync, 4 volumes, game speed (1x/1.5x/2x), screen shake, colourblind-friendly highlights.
- **Abandon (U5, decided):** `UDwRun::Abandon()` -> stats finalised -> `WBP_RunSummary` (defeat style, "Abandoned").
- **End of run:** Victory (Kraken) or Defeat (ship sunk, crew dead, or captain dead) -> run summary -> Main menu or New run. Summary fields (U4, decided): captain, seed, nodes cleared, fights by outcome (sunk/plundered/surrendered), gold, crew lost, kills, time - all from `FDwRunStats`, plus the end reason as the headline.
- **Quit or crash mid-run:** run is gone by design; nothing to clean up because nothing was saved.

## As built (DW-210, 2026-11-11)
- **Screens:**
  - `WBP_Menu` is one screen for the main menu, pause, confirm and run summary (`UDwMenuViewModel`: title, lines, buttons, Esc).
  - `WBP_CaptainSelect` (`UDwCaptainSelectViewModel`) and `WBP_Settings` / `WBP_SettingRow` (`UDwSettingsViewModel`, a < value > stepper per row).
  - They are built by `UDwUIContentLibrary` and listed in `DA_UIScreenSet` (`Menu`, `CaptainSelect`, `Settings`).
- **Navigation:** it lives in `UDwUIManagerSubsystem` (`UI/DwUIManagerShell.cpp`), which hands each view model its button intents.
  - The main menu shows when `ADwMenuGameMode` is the world's game mode, and that game mode clears any leftover run on `BeginPlay`.
  - A run starting closes all shell screens; Victory or Defeat (including Abandon) opens the summary.
  - Summary → New Run opens captain select in `L_Sea`; Main Menu clears the run and opens `L_MainMenu`.
- **Layers:** menus go on `UI.Layer.Menu`, pause on `UI.Layer.GameMenu` (the game is paused while it's up) and confirm on `UI.Layer.Modal`.
  - Closing everything clears those layers whole: popping two screens from one CommonUI stack in the same frame leaves the first one drawn.
- **Esc (CommonUI back action):**
  - Captain select and settings: back.
  - Pause: resume.
  - Confirm: No.
  - Main menu and summary: ignored (Quit and leaving are explicit buttons).
  - In `L_Sea` with the game in front: cancel targeting first, else open pause (`ADwPlayerController::OnPause` → `TogglePause`).
- **`UDwGameUserSettings` (pulled forward from DW-216, Brandon 2026-11-11):**
  - Every setting saves, and is applied and saved at once on each change.
  - Takes effect now: display (resolution, window mode, vsync) and game speed (`UDwPresentationSubsystem` playback rate, also set when a rendered game world starts).
  - Saved but no effect yet: volumes (no audio yet), screen shake (the camera doesn't shake yet), highlight palette (palette assets are DW-216). The screen says so on each row.
  - DW-216 keeps: `ApplyAudio` with sound classes, `UDwHighlightPalette` assets, music states.
- **Tests:** `DeadwaterRun.UI.Shell` and `DeadwaterRun.Shell.Settings`; settings tests use their own objects and never write the player's ini. `Tools/Capture.ps1 -Demo shell` plays menu → settings → captain select → Set Sail → pause → abandon → summary → New Run → abandon → Main Menu.

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
