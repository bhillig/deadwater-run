# Deadwater Run - Release Checklist (Itch.io)

Owner: Release Engineer. Brandon presses every Itch.io button (page visibility, payments, uploads with his butler login, publish). Nothing here is done on his behalf. Last updated 2026-11-20: release moved from Steam (Dec 10) to **Itch.io, Thu Dec 3** (Brandon).

Itch facts below are from memory of the itch.io docs and dashboard - verify each against the "Edit game" page and the butler docs before relying on it.

## 1. Dates (all self-imposed - Itch has no review)

| Date | Day | Milestone | Status | Who |
|---|---|---|---|---|
| **Nov 22** | Sun | Audio source picked (asset-scraper options) | open | Brandon |
| **Nov 25** | Wed | Itch account ready: developer name, payout method, tax interview if itch collects payments | open | Brandon |
| **Nov 27** | Fri | Itch page drafted and saved as **Draft** (cover, screenshots, copy, price) | open | Release Engineer prepares, Brandon creates the page |
| **Nov 30** | Mon | Release candidate packaged; first butler push to the draft page; install and play from the itch download | open | Release Engineer packages, Brandon pushes |
| **Dec 2** | Wed | Final build pushed; clean-download smoke (section 6) passes | open | Brandon + QA |
| **Dec 3** | Thu | **Launch**: page set to Public, $5 | open | Brandon |

## 2. How Itch differs from Steam (what changed in the plan)

- No store-page or build review and no waiting periods: a page is public the moment its visibility is set to Public. Uploads go live as soon as butler finishes processing.
- New accounts' pages may not show in itch's browse/search until itch reviews them (verify); the direct link works at once. Do not rely on itch browse traffic on launch day.
- No app fee. Itch takes an adjustable revenue share (default 10%, set in the page or account settings).
- Steam-only work is dropped: capsule/library art sizes, IARC/content survey, SteamPipe depots and the `Build/Steam/*.vdf.template` files (superseded; release-engineer removes them in DW-406).

## 3. Account and paperwork (Brandon)

- [ ] Itch.io account with the developer name to show on the page
- [ ] Payment mode chosen: "collected by itch.io, paid out later" (itch handles sales tax/VAT; needs the tax interview) or "direct to you" via PayPal/Stripe (you handle tax) - **needs Brandon**
- [ ] Revenue share set (default 10%)
- [ ] butler installed and logged in on Brandon's machine (`butler login`); credentials stay out of the repo
- [ ] Synty licence and any audio/font licence records kept

## 4. Itch page (`Docs/Release/StorePage.md`)

- [ ] Cover image **630 x 500** (minimum 315 x 250) - needs art (see StorePage section 2)
- [x] Screenshots (3-5 recommended) - 13 captured, 6 recommended; recapture map/shop/event/captain select now that they are real
- [ ] Short tagline + description picked (options drafted)
- [ ] Genre (one) + tags (up to 10) picked (options drafted)
- [ ] Price: paid, minimum $5.00
- [ ] AI generation disclosure answered (draft in StorePage section 6)
- [ ] Kind of project: Downloadable; platform: Windows; release status: Released
- [ ] Optional: banner image, background colours, trailer (YouTube/Vimeo link)

## 5. Build and packaging

First package made 2026-11-19 (DW-218 packaging part); results in `Docs/Release/BuildNotes.md`.

- [x] **`Build/Package.ps1`**: `RunUAT BuildCookRun` (Win64, Shipping/Development/Test, cook, stage, pak + IoStore, compressed, archive to `Build/Out/<version>-<config>`); first line `PACKAGE PASS/FAIL`; fails if an editor-only plugin file is staged
- [x] `Config/DefaultGame.ini` packaging section (MapsToCook `L_MainMenu` + `L_Sea`, `DirectoriesToNeverCook`, pak/IoStore, compression, shared shader code, `ProjectVersion=0.1.0`)
- [x] `Docs/Release/BuildNotes.md`
- [x] Staged build has no `ModelContextProtocol` / `Terminal` / `AllToolsets` / modelling-tools files, no `.pdb` (checked by `Package.ps1`)
- [ ] `Build/Itch/Push.ps1` (DW-406): wraps `butler push Build/Out/<version>-Shipping/Windows <user>/deadwater-run:windows --userversion <ProjectVersion>`; prints the command and refuses without `-Confirm` so Brandon runs it; replaces the Steam vdf templates
- [ ] Logging decision (Open-Questions O3): Shipping without logs (default), plus a Test build on a separate `windows-beta` channel (hidden upload) for diagnostics? - Brandon
- [ ] Smoke test per build (doc 18): package Shipping, exit code 0, launch to main menu, screenshot; full seeded run on a Development package; QA manual pass, one run per captain

## 6. Launch-day steps (Dec 3) - all by Brandon

1. On the draft page, download the `windows` upload as a customer would (or via the itch app), unzip to a clean folder, launch to the main menu.
2. Play a run: menu, Set Sail, a fight, quit.
3. Check the page: price $5.00 minimum, cover, screenshots, description, tags, platform Windows.
4. Set visibility to **Public**.
5. Open the public link in a private browser window and confirm the buy button works and shows $5.
6. Watch comments and crash reports; hotfixes are a new butler push to the same channel (players on the itch app update automatically).
7. Optional: devlog post announcing the launch.

## 7. Cut list reminder (do not promise in marketing)

Meta-progression, achievements, controller support, localization, extra captains/bosses, Steam release (Scope.md).

## 8. Open questions for Brandon

- Node count: `context/offer.md` says 7-8 node runs, `Docs/Scope.md` and `DeadwaterRun.md` say 8-9. Which goes in copy? (copy currently avoids a number)
- Payment mode (itch collects vs direct PayPal/Stripe).
- Ship a hidden Test build channel for diagnostics (O3)?
