# Deadwater Run - Release Checklist

Owner: Release Engineer. Brandon presses every Steamworks button (submit, publish, upload, release). Nothing here is done on his behalf. Last updated 2026-11-11 (DW-308; folds in the docs part of DW-006).

## 1. Valve dates

Schedule moved one week later on 2026-11-11 (Brandon): launch Dec 3 -> **Dec 10**.

| Date | Day | Milestone | Status | Who |
|---|---|---|---|---|
| **Nov 10** | Tue | Latest allowed app-fee payment for a Dec 10 release (Valve: 30 days between paying the fee and release) | Scope.md says the fee is paid; **payment date is not recorded - confirm it is on or before Nov 10** | Brandon |
| **Nov 19** | Thu | Submit store page for review (needs the assets in `StorePage.md`). Earlier is better - every day before Nov 19 is slack | open - blocked on capsule art + Brandon's picks | Brandon |
| ~Nov 20-25 | | Store page review (see lead times) | | Valve |
| **Nov 26** | Thu | Store page public as "Coming Soon" - the 2-week minimum before a Dec 10 release means **Nov 26 is the last day, zero slack**. Nov 26-27 is US Thanksgiving: get approval before then | open | Brandon (approval is not publication: after Valve approves, Brandon must press Publish) |
| Nov 30 | Mon | Release candidate build ready (2 days before submit) | open | Release Engineer |
| **Dec 2** | Wed | Submit build for review | open | Brandon |
| **Dec 10** | Thu | **Launch**, $5 | open | Brandon |
| Dec 17 - Jan 4 | | Steam Winter Sale (Dec 10 launch: one full-price week before it) | | |

## 2. Review lead times (my understanding - verify in Steamworks docs)

- Store page review: typically a few business days. From Nov 19, a 3-5 business day review ends Nov 24-26 - and Nov 26-27 is US Thanksgiving, when Valve may be slow or closed (verify). **If Valve asks for any change, a second round can break the Nov 26 deadline.** So submit as early as possible before Nov 19, even with the weakest acceptable art (option C in `StorePage.md`), and polish afterward.
- Build review: about 3-5 business days. Dec 2 + 3-5 business days lands Dec 7-9, a day or two before launch. A rejected build has very little room to fix. Recommended: submit the RC earlier than Dec 2 if it is ready (the Nov 30 line above), and keep Dec 2 as the last acceptable day.
- The release date is set in Steamworks by Brandon; I never change it.
- Nothing else in Steamworks (price, tags, screenshots, descriptions) should need a re-review after approval, but I believe description/media edits to a live page publish quickly - verify.

## 3. Account and paperwork (Brandon)

- [ ] App fee paid on or before Nov 10 (record the date here: ______)
- [ ] Steamworks paperwork complete: tax interview, bank/payment info, identity verification
- [ ] Developer/publisher name decided; support email/URL set
- [ ] Content survey, IARC rating, AI-content disclosure answered (draft in `StorePage.md` section 7)
- [ ] Synty licence and font licence invoices/records kept

## 4. Store page (`Docs/Release/StorePage.md`)

- [ ] Capsule/library/icon art in all sizes - **blocked: none exists**
- [x] At least 5 screenshots at 1920x1080 - 13 captured, 6 recommended (`Docs/Release/Screenshots/`)
- [ ] Short + long description picked (options drafted)
- [ ] Tags/genres picked (options drafted)
- [x] Price $5, release date Dec 10 recorded
- [ ] System requirements - guesses drafted, to be measured
- [ ] Trailer - optional, none
- [ ] Submit for review (by Nov 19) - Brandon
- [ ] Publish Coming Soon (by Nov 26) - Brandon

## 5. Build and packaging (open - not done yet)

These are still open; this ticket did not touch them (out of scope).

- [ ] **`Build/Package.ps1`** (DW-006 packaging part / DW-218): not written. The `Build/` folder does not exist. No Shipping package of the project has been made by the release engineer yet.
- [ ] `Config/DefaultGame.ini` packaging section (MapsToCook, `DirectoriesToNeverCook`, pak/IoStore) per doc 18: not applied (only the asset manager and UI input settings are in the file today)
- [ ] `Build/Steam/app_build.vdf.template`, `depot_build_win64.vdf.template` (no credentials in git)
- [ ] `Docs/Release/BuildNotes.md`
- [ ] Verify the staged build has no `ModelContextProtocol` / `Terminal` / editor binaries
- [ ] Logging decision (Open-Questions O3): Shipping without logs, plus a Test build on a `beta` branch?
- [ ] Smoke test per build (doc 18): package Shipping, exit code 0, launch to main menu, screenshot; autoplay run on Development; QA manual pass, one run per captain

Content that must be real before the Dec 2 build (so copy and screenshots are true): captain abilities for Fearmongerer and Conqueror (DW-207/208 - placeholder text visible today), shop (DW-204), events (DW-205), Kraken (DW-209 in progress), Synty/UI art pass (DW-307), audio decision/integration.

## 6. Launch-day steps (Dec 10) - all by Brandon

1. Confirm the approved build is set live on the `default` branch in SteamPipe (Builds tab -> set live). Verify by installing from the Steam client on a clean account/PC and launching to the main menu.
2. Confirm the store page is public with price $5.00, release date shown correctly, and all assets/screenshots/descriptions as intended.
3. Press the Release button in Steamworks (the Release tab). Releasing is a manual action as I understand it - plan to do this on the morning of Dec 10 (time of day is Brandon's choice); verify the exact flow in Steamworks the week before.
4. Install and play a run from Steam as a customer would: menu, Set Sail, a fight, quit.
5. Watch for crash reports and early reviews/discussions; keep a `beta` branch for hotfix candidates; hotfix build goes through the same upload by Brandon.
6. Optional: social/community announcement, wishlist email goes out automatically from Steam.

## 7. Cut list reminder (do not promise in marketing)

Meta-progression, achievements, controller support, localization, extra captains/bosses (Scope.md).

## 8. Open questions for Brandon

- Node count: `context/offer.md` says 7-8 node runs, `Docs/Scope.md` and `DeadwaterRun.md` say 8-9. Which goes in copy? (copy currently avoids a number)
- Fee payment date (30-day rule).
- Ship a `beta` Test build for diagnostics (O3)?
