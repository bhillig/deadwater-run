# 18 - Packaging and Release

## Problem
- Release on **Itch.io, Thu 2026-12-03, $5** (2026-11-20: moved from Steam, Dec 10). No external review deadlines; RC Nov 30, final build pushed Dec 2.
- A Shipping build must package from the command line and launch to the main menu (release-engineer "done means").
- Editor-only plugins (MCP, Terminal, modelling tools) and dev content must not ship. No secrets in the repo.

## Decisions

### D1 - Store integration depth
| Option | Verdict |
|---|---|
| **A. No store SDK code in v1.0: ship a plain Win64 build** (Itch.io via butler since 2026-11-20; was SteamPipe depots) | **Chosen.** Achievements, cloud saves and controller support are cut; itch needs no SDK at all |
| B. `OnlineSubsystemSteam` for overlay/presence | Rejected (and moot on Itch) |
| C. Full Steamworks (achievements, cloud) | Cut by Scope; Steam is post-launch |

### D2 - Build pipeline
| Option | Verdict |
|---|---|
| **A. `Build/Package.ps1` wrapping `RunUAT BuildCookRun` (Shipping, Win64, cook, stage, pak + IoStore, archive to `Build/Out/<version>`), plus `Build/Itch/Push.ps1` for butler (was `Build/Steam/*.vdf` templates); Brandon runs the upload** | **Chosen.** Reproducible, scriptable, matches "Brandon presses the button" |
| B. Packaging from the editor menu | Rejected: not reproducible, not scriptable |
| C. CI server | Rejected for v1: no time to stand up; scripts are CI-ready later |

## Settings (release-engineer owns `Config/DefaultGame.ini` packaging sections)
- `+MapsToCook=/Game/DeadwaterRun/Maps/L_MainMenu` and `L_Sea`; `bCookAll=False`.
- Asset Manager primary types `CookRule=AlwaysCook` (`13-Data-Layout.md`).
- `+DirectoriesToNeverCook=(Path="/Game/DeadwaterRun/Dev")`.
- `bUsePakFile=True`, IoStore on, compression on, `bShareMaterialShaderCode=True`.
- `ProjectVersion` bumped per RC; build notes in `Docs/Release/BuildNotes.md`.
- Plugins limited to the Editor target (`01-Project-Structure.md`); verify the staged build contains no `ModelContextProtocol`/`Terminal` binaries.
- Logging: with the launcher (installed) engine, Shipping has no log output (`bUseLoggingInShipping` needs a source engine). Decision for Brandon (Open-Questions O3): ship Shipping without logs (default), or ship a Test configuration as a beta branch for diagnosing player bugs.
- Tests and `Dev/` code compile out of Shipping (`WITH_DEV_AUTOMATION_TESTS`, `UE_BUILD_SHIPPING` guards on `UDwCheatManager` and dev settings use); `-DwAutoplay` is compiled into Development and Test only.

## Itch.io
- Release moved from Steam to Itch.io on 2026-11-20 (Brandon). Itch has no store-page or build review, so the only dates are self-imposed: RC Nov 30, final build pushed Dec 2, page Public Dec 3.
- Uploads use **butler** (itch's command-line uploader): `butler push Build/Out/<version>-Shipping/Windows <user>/deadwater-run:windows --userversion <ProjectVersion>`. butler sends only the changed bytes and players on the itch app auto-update. `Build/Itch/Push.ps1` (DW-406) wraps it: it prints the command and runs only with `-Confirm`, so Brandon does the push with his own `butler login`. No API keys in git.
- Channels: `windows` (release). Optional hidden `windows-beta` channel for a Test build (O3).
- Page checklist (`Docs/Release/StorePage.md`): cover image 630 x 500, 3-5 screenshots, description, genre, tags, $5 minimum price, AI disclosure.
- Release checklist (`Docs/Release/Checklist.md`): account/payout setup, dates, clean-download smoke, launch-day steps.
- `Build/Steam/*.vdf.template` are superseded; DW-406 removes them.
## Smoke test per build
1. `Build/Package.ps1 -Config Shipping` -> exit code 0.
2. Launch `DeadwaterRun.exe`, reach the main menu (screenshot).
3. Development/Test build: `DeadwaterRun.exe -DwAutoplay -DwSeed=1 -DwAutoplaySpeed=8` plays a full seeded run and exits with code 0 (built by tools-engineer DW-407).
4. QA manual pass on the Shipping build (one run per captain).

## Connections
- `01-Project-Structure.md`: modules and plugin targets.
- `13-Data-Layout.md`: cook rules.
- `16-Shell.md`: default maps, settings ini.
- `17-Testing-Simulator.md`: autoplay smoke.

## Build order (release-engineer)
1. DW-006 (week 0): `Docs/Release/Checklist.md` with all dates; `Build/Package.ps1`; first Shipping package of the cleaned project.
2. DW-116 (week 1): package the playable combat; verify plugin exclusion.
3. DW-218 (week 2): full-run package, store page checklist, Steam vdf templates.
4. DW-308 (week 3): store page assets assembled for Brandon to submit Nov 19.
5. DW-406 (week 4): Itch page package for Brandon to create as Draft by Nov 27; `Build/Itch/Push.ps1`; remove `Build/Steam`; RC dry run.
6. DW-501 (Nov 30): RC build for Brandon to push to the draft page. DW-601 (Dec 1-3): launch checklist; final push Dec 2, Public Dec 3.

## How it's tested
- Each release PR includes the exact commands, exit codes, package size, and a main-menu screenshot from the packaged build.

## What the player sees
- A stable $5 build on Itch.io on Dec 3 that launches straight to the menu and runs the whole voyage without a hitch.
