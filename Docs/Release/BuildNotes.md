# Build notes

One entry per packaged build (Docs/Architecture/18-Packaging-Release.md "How it's tested"): commands, exit codes,
size, smoke results. Newest first. Archives live in `Build/Out/` (gitignored); Brandon uploads.

## 0.1.0 - first package (2026-11-19, DW-218 packaging part)

Content: master a898e4f (everything through DW-303) + the packaging settings. Not a release candidate.

| Step | Command | Result |
|---|---|---|
| Shipping package | `./Build/Package.ps1` | `PACKAGE PASS 0.1.0 Shipping (511 MB, 5 min)`, exit 0. Shipping compiled with no source changes (only engine-header deprecation warnings); cook `Success - 0 error(s), 1 warning(s)` (a socket host lookup, harmless) |
| Plugin exclusion | (checked by `Package.ps1`) | No `ModelContextProtocol` / `Terminal` / `AllToolsets` / modelling-tools files staged, no `.pdb` |
| Launch | `Build/Out/0.1.0-Shipping/Windows/DeadwaterRun.exe -windowed -ResX=1600 -ResY=900` | Main menu after < 30 s, ~1.3 GB working set. Screenshot: `Saved/Captures/DW-218/shipping-main-menu.png` |

Layout: `Windows/DeadwaterRun.exe` (bootstrap) + `Windows/DeadwaterRun/Binaries/Win64/DeadwaterRun-Win64-Shipping.exe`,
content in `Windows/DeadwaterRun/Content/Paks` (`.pak` + IoStore `.ucas/.utoc`). This `Windows/` folder is the depot's
`ContentRoot` (`Build/Steam/app_build.vdf.template`).

Logs: Shipping has none with the launcher engine (O3 still open: ship a Test build on `beta` for diagnostics?).

### Full seeded run on a Development package (cooked data)

Built from this branch + the shop screen + the snapshot fix (`gameplay-engineer/fix-snapshot-dangling`), the state
after merging all three.

| Step | Command | Result |
|---|---|---|
| Development package | `./Build/Package.ps1 -Config Development` | `PACKAGE PASS 0.1.0 Development (1113 MB)`, exit 0 |
| Voyage | `Build/Out/0.1.0-Development/Windows/DeadwaterRun.exe /Game/DeadwaterRun/Maps/L_Sea -windowed -ResX=1600 -ResY=900 -ForceRes -DwCapture=<png> -DwCaptureDemo=voyage -DwSeed=7 -DwCaptain=TeamLeader -DwMood=Day` | Exit 0 in 102 s: map, fight, rewards, level-up, shop, events, the Kraken in both phases, Victory after 10 stops. 20 frames in `Saved/Captures/DW-218/packaged-voyage-*.png` |

First attempt crashed at the Kraken fight (`FDwCombatQueries::FindAbility`, access violation): the dev capture held
pointers into a temporary `FDwCombatSnapshot` (`GetSnapshot().FindUnit(...)`). The editor build survived the undefined
behaviour by luck; the packaged build did not. Fixed on `gameplay-engineer/fix-snapshot-dangling` (the lookups are now
lvalue-only, so the pattern doesn't compile). Dev-only code, so Shipping was never affected, but it shows why packaged
runs belong in every RC check.
