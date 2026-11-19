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

Still to do for this build: a full seeded run on a Development package (the cooked data, not just the menu).
