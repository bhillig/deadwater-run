# 18 - Packaging and Release

## Problem
- Steam deadlines: store page submitted ~Nov 19, public "Coming Soon" by Nov 26, build submitted ~Dec 2, launch Dec 10 ($5; moved from Dec 3 on 2026-11-11).
- A Shipping build must package from the command line and launch to the main menu (release-engineer "done means").
- Editor-only plugins (MCP, Terminal, modelling tools) and dev content must not ship. No secrets in the repo.

## Decisions

### D1 - Steam integration depth
| Option | Verdict |
|---|---|
| **A. No Steamworks SDK code in v1.0: ship a plain Win64 build through SteamPipe depots** | **Chosen.** Achievements, cloud saves and controller support are cut; the Steam overlay works for games launched from Steam without SDK calls |
| B. `OnlineSubsystemSteam` for overlay/presence | Rejected: setup and test cost with no v1 feature using it |
| C. Full Steamworks (achievements, cloud) | Cut by Scope |

### D2 - Build pipeline
| Option | Verdict |
|---|---|
| **A. `Build/Package.ps1` wrapping `RunUAT BuildCookRun` (Shipping, Win64, cook, stage, pak + IoStore, archive to `Build/Out/<version>`), plus `Build/Steam/*.vdf` templates for SteamPipe; Brandon runs the upload** | **Chosen.** Reproducible, scriptable, matches "Brandon presses the button" |
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

## Steam
- `Build/Steam/app_build.vdf.template`, `depot_build_win64.vdf.template` with `<APPID>`/`<DEPOTID>` placeholders; real IDs and credentials never in git (`.gitignore` the filled files).
- Branches: `default` (release), `beta` (playtests, RCs).
- Store page checklist (`Docs/Release/StorePage.md`): capsule images in every required size, at least 5 screenshots from the real game, short and long description, tags, system requirements, price $5, release date Dec 10, content survey, trailer (optional for submission). **Screenshots require the Synty art pass (DW-307) by Nov 18.**
- Release checklist (`Docs/Release/Checklist.md`): Valve 30-day rule (fee paid), store page dates, build review lead time (~3-5 days; submit by Dec 2), launch-day steps.

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
5. DW-406 (week 4): confirm page live Nov 26; RC dry run; beta branch upload prep.
6. DW-501 (week 5): RC build for Brandon to submit Dec 2. DW-601 (week 6): launch checklist.

## How it's tested
- Each release PR includes the exact commands, exit codes, package size, and a main-menu screenshot from the packaged build.

## What the player sees
- A stable $5 build on Steam on Dec 10 that launches straight to the menu and runs the whole voyage without a hitch.
