# Deadwater Run - Itch.io Page Package

First prepared 2026-11-11 for Steam (DW-308); re-targeted to **Itch.io** on 2026-11-20 (Brandon moved the release to Itch.io, launch **Thu 2026-12-03**). Nothing here has been published. Brandon creates the page and presses every button on itch.io.

Status key: **ready** = can be pasted/uploaded as is. **needs Brandon** = a decision, a fact only he has, or an account action. **blocked** = cannot be done until something else lands.

> Field names and image sizes below are from the itch.io "Edit game" page as I know it. That page is the source of truth - compare before uploading and tell me if anything differs.

---

## 1. Basic info (itch "Edit game" fields)

| Field | Value | Status |
|---|---|---|
| Title | Deadwater Run | ready |
| Project URL | `<username>.itch.io/deadwater-run` | needs Brandon (username) |
| Short description / tagline | One line under the title - see section 4 | needs Brandon (pick) |
| Classification | Games | ready |
| Kind of project | Downloadable | ready |
| Release status | Released | ready |
| Pricing | **Paid, minimum $5.00 USD**. Optional: "let people pay more" / launch sale | needs Brandon (sale yes/no) |
| Uploads | One `windows` channel pushed with butler (Shipping zip); tick "Windows" as the platform | blocked on RC (Nov 30) |
| Genre | Strategy (one only) | needs Brandon |
| Tags | Up to 10 - section 5 | needs Brandon |
| AI generation disclosure | Section 6 | needs Brandon |
| Community | Comments on (recommended for bug reports) or off | needs Brandon |
| Visibility | **Draft** until launch day, then **Public** | Brandon on Dec 3 |
| Developer name | Not in the repo beyond `CopyrightNotice=Brandon Hillig 2026`. Shown from the itch account | needs Brandon |
| Languages / input | English; keyboard and mouse only (controller support cut). Say so in the description | ready |
| Mid-run saves | None (Scope). Worth one honest line in the description (runs are ~30 min) | ready |

## 2. Graphical assets

Itch needs far less than Steam: one cover image plus screenshots. No AI-generated art (values.md: only with Brandon's approval; none generated here).

| Asset | Size | Required? | Status |
|---|---|---|---|
| Cover image | **630 x 500** recommended (minimum 315 x 250); shown in browse, collections and embeds | Effectively required (pages without one look broken in browse) | needs art - see section 7 |
| Screenshots | 3-5 recommended, shown in the page sidebar; any 16:9 size, 1920 x 1080 fine | Recommended | **ready** - 13 captured, see section 3 |
| Banner / header image | Wide image at the top of the page (theme editor); no fixed size | Optional | optional |
| Background / theme colours | Theme editor | Optional | optional: deep sea blue to match the game |
| Trailer | YouTube or Vimeo link | Optional | none exists; not a blocker |
| Window/exe icon | 256 x 256 .ico for `DeadwaterRun.exe` (project settings) | Optional | still the UE default - small polish item |

## 3. Screenshots (real, from the current build; itch shows them in the sidebar, 3-5 recommended)

Captured 2026-11-11 with `Tools/Capture.ps1`, all **1920 x 1080 PNG**, current build on branch `orchestrator/dw-210-shell` (a0437f6). Saved in `Docs/Release/Screenshots/`.

| # | File | Caption | Status / quality note |
|---|---|---|---|
| 1 | `01-combat-move.png` | Your crew spreads across the deck - pick a pirate, plot a path | ready. Highlight tiles + crew cards readable |
| 2 | `02-combat-aim-musket.png` | The sniper takes the crow's nest and lines up a musket shot with damage preview | ready. Shows damage preview ("-20 HP") |
| 3 | `03-combat-cannon.png` | Man the cannon: fire on the enemy ship and sink it before the crew even fights | ready. Best illustration of the ship-health hook ("Enemy ship: -70 HP") |
| 4 | `04-combat-hit-frozen.png` | A hit lands: floating damage, health bars, and a frozen pirate | ready. Good "action" frame |
| 5 | `05-combat-enemy-turn.png` | The rival crew takes its turn and their cannon hammers your hull (-70) | ready |
| 6 | `06-combat-victory.png` | Victory - the enemy crew is defeated, plunder awaits | ready. Leftover "-100" floaters are fine; recapture if cleaner is wanted |
| 7 | `07-map-midrun.png` | Chart your course across the sea: fights, shops, events, and the Kraken at the end | usable. Flat UI, small text, dark - weak next to the combat shots |
| 8 | `08-rewards-plunder.png` | Their crew is beaten - plunder, XP and level-ups | usable but weak: small text, empty background |
| 9 | `09-level-up-perk.png` | Level up and choose a perk | usable but weak, same reason |
| 10 | `10-main-menu.png` | Main menu | weak: placeholder dark-blue menu, no logo art. Not recommended as a lead |
| 11 | `11-captain-select.png` | Choose your captain | **not recommended as is**: shows "(Placeholder: no effect until DW-207.)" text in the description. Recapture after DW-207/208 land or skip |
| 12 | `12-shop-shell-NOT-RECOMMENDED.png` | Shop (shell) | **not recommended**: says "The shopkeeper is still unpacking (shop: DW-204)" |
| 13 | `13-event-placeholder-NOT-RECOMMENDED.png` | Event (shell) | **not recommended**: says "Nothing happens... yet (events: DW-205)" |

Honest read for Brandon: shots 1-6 are the store page. They are six moments of the same screen, so the gallery will look repetitive; the Synty ships and characters are the selling point. Shop, events and captain abilities are real now (DW-204..208), so recapture 7-13 before launch and swap them in - itch pages can be edited any time, no review. Combat HUD polish items I noticed (not mine to fix): hint bar at the bottom touches the bottom edge of the action cards; the top "Round 1 - YOUR TURN" text sits close to the top edge.

**Order of the gallery is a creative choice - options below:**
- Lead A: `03-combat-cannon` (shows the unique ship-HP hook first)
- Lead B: `01-combat-move` (the clearest "what is this game" frame)
- Lead C: `06-combat-victory` (outcome and plunder; punchy but more text)


## 4. Descriptions (Brandon picks one of each)

Only features in Scope.md "Must ship" (plus the design doc's core loop). Numbers like node counts are left out because `context/offer.md` says 7-8 node runs while Scope.md says 8-9 - **Brandon: which is right?** Captain names are from `DeadwaterRun.md`.

### Opening paragraph (S1-S3; first lines of the itch description)

- **S1 (pitch-led, 285 chars):** Battle out of the Aquadia seas in this exciting and tense turn-based pirate roguelike. Pick a captain, chart a route through fights, shops and events, and choose how to win: sink their ship fast, or slay the crew and plunder. Face the Kraken to escape.
- **S2 (hook-led, 266 chars):** Every ship has a health bar. Sink your rival's ship to end the fight fast, or cut down the crew to plunder their gold. A 30-minute turn-based pirate roguelike with three captains, a branching sea chart and a Kraken waiting at the end.
- **S3 (crew-led, 277 chars):** Command a small pirate crew across the seas of Aquadia. Move, fight, fire the cannons, then decide: sink the ship or take the plunder. Lose a pirate and they're gone for good. Survive the sea chart and take on the Kraken. About 30 minutes a run.

(Character counts measured 2026-11-11. Itch shows the tagline as one line, so S1-S3 are better used as the first paragraph of the description; a tagline should be under ~100 characters, e.g. "Sink the ship or slay the crew - a 30-minute pirate roguelike.")

### Long description (itch rich-text editor: paste, then turn [h2] into headings and [list] into bullets)

**L1 - feature list**
```
[h2]Battle out of the Aquadia seas[/h2]
Deadwater Run is a turn-based pirate roguelike. Lead a small crew across the legendary seas of Aquadia, fight rival crews deck to deck, and challenge the Kraken to escape.

[h2]Sink the ship, or slay the crew?[/h2]
Every ship has a health bar. Fire your cannons and sink the enemy ship to end the fight quickly, or fight it out on deck and kill the crew to plunder their gold and loot. Quick and safe, or slow and rich - your call, every fight.

[h2]Features[/h2]
[list]
[*] Turn-based crew combat: move, attack, use abilities and items, or man the cannons
[*] Three captains, each with their own combat abilities and a rule that changes how the whole run plays
[*] Permadeath crew: a fallen pirate is gone for good
[*] A sea chart of fights, shops and events - pick your route, no turning back
[*] Level up your crew, learn new moves, and spend plunder at the shop
[*] Rival pirate crews with their own tactics
[*] Random events: good or bad luck on the open sea
[*] A Kraken final boss with two phases - target its tentacles to blunt its attacks
[*] About 30 minutes a run, built for "one more run"
[/list]

[h2]Know before you buy[/h2]
Keyboard and mouse. Single player. No mid-run saves.
```

**L2 - narrative / pitch**
```
The seas of Aquadia have never been kind, and the Kraken has never been beaten. You have a ship, a captain and two crewmates. That will have to do.

Deadwater Run is a turn-based pirate roguelike built around one question: what do you want from this fight? Every ship has a health bar. Put your cannons to work and sink the enemy before their crew can touch you - or board them, cut down every pirate and take the plunder, with the risk that comes with it.

Choose one of three captains and sail a chart of rival crews, shops and unpredictable events. Level up, learn new moves, patch the hull, and keep your crew alive - once a pirate falls, they stay down. At the end of the chart, the Kraken is waiting.

[b]Single player. Keyboard and mouse. About 30 minutes a run.[/b]
```

**L3 - short and punchy**
```
[b]Sink the ship. Slay the crew. Take the plunder.[/b]

Deadwater Run is a tense turn-based pirate roguelike with a twist: ships have health bars too. End a fight fast by sinking the enemy, or cut down the crew for a bigger haul.

[list]
[*] Three captains, three ways to play
[*] A branching sea chart of fights, shops and events
[*] Permadeath crew
[*] A two-phase Kraken boss with targetable tentacles
[*] About 30 minutes a run
[/list]

Battle out of the Aquadia seas.
```

Copy claims to re-check against the build on Dec 2 (as of Nov 20 captains, shop, events, Kraken phases/tentacles and five rival crews with their own tactics are all real): anything about **music or sound** - none ships until DW-311 lands; "30 minutes a run" - the simulator estimates ~8 min of actions, so confirm with playtests (DW-404) before using a number.


## 5. Genre and tags

Itch allows one genre and up to 10 tags; tags drive itch browse pages.
- **Genre:** Strategy (alternative: Role Playing if Brandon wants the crew levels/perks to lead)
- **Tags option 1 (10):** roguelike, turn-based, turn-based-strategy, tactical, pirates, roguelite, permadeath, singleplayer, low-poly, 3d
- **Tags option 2 (10):** roguelike, turn-based-tactics, pirates, naval, strategy, procedural-generation, grid-based, permadeath, indie, singleplayer
- Not appropriate (not in the game): deckbuilding, card-game, multiplayer, co-op, controller.

## 5b. System requirements (put in the description; DERIVED - guesses flagged)

Basis: Unreal Engine 5.8 (`DeadwaterRun.uproject`), Win64 only, `Config/DefaultEngine.ini` enables Lumen (`r.DynamicGlobalIlluminationMethod=1`, `r.ReflectionMethod=1`), Virtual Shadow Maps (`r.Shadow.Virtual.Enable=1`) and mesh distance fields, no static lighting. That means a DirectX 12 / Shader Model 6 GPU. Shipping 0.1.0 packaged on 2026-11-19 is **511 MB** (BuildNotes.md); no benchmark has been run, so CPU/RAM/GPU numbers are still **GUESSES** until measured on real hardware.

| | Minimum (guess) | Recommended (guess) |
|---|---|---|
| OS | Windows 10 64-bit | Windows 10/11 64-bit |
| Processor | Quad-core, ~2018 (e.g. Intel Core i5-8400 / AMD Ryzen 5 1600) - GUESS | 6-core (e.g. Ryzen 5 3600 / Core i5-10400) - GUESS |
| Memory | 8 GB RAM - GUESS | 16 GB RAM - GUESS |
| Graphics | DX12 / SM6 GPU, ~GTX 1060 6 GB / RX 580 - GUESS | ~RTX 2060 / RX 5700 - GUESS |
| DirectX | Version 12 | Version 12 |
| Storage | **1 GB** (Shipping 0.1.0 is 511 MB; headroom for audio) | same |
| Notes | 1080p target. Keyboard and mouse. | |

To replace guesses with facts (release engineer, after Package.ps1 exists): package Shipping, record disk size, run the smoke test on the weakest available GPU and read frame times via `stat unit` in a Test build, then lower or raise the tiers. Fallback if no time: ship the guesses but keep them conservative (higher), since a wrong low spec causes refunds.


## 6. AI generation disclosure (Brandon answers on itch)

Itch asks whether the project contains AI-generated content (verify the exact wording and categories on the Edit game page).

| Topic | Draft answer | Basis |
|---|---|---|
| Graphics | No AI-generated graphics - Synty Pirate + VFX packs and in-engine UI | values.md: AI art only with approval; none approved. **Brandon: confirm** |
| Sound / music | Depends on the audio pick (asset-scraper options, Nov 22). Confirm the chosen packs are not AI-generated | open |
| Text / dialogue | **Brandon decides.** Event text was drafted by the event-writer agent and approved/edited by Brandon (Docs/ContentPipeline) - that is AI-generated text by most definitions | Content pipeline, 2026-11-17 |
| Code | AI was used as a coding tool. Itch's disclosure is about content shipped to players; mention it in the description only if Brandon wants to | |

Other content notes (no rating questionnaire on itch, but say it honestly in the description): stylized cartoon violence (swords, muskets, cannons) with no blood; "Grog" is an item name only; no gambling, no in-app purchases, no online features.

Third-party content: Synty Pirate + VFX packs (licensed), Unreal Engine (Epic EULA; 5% royalty only above $1M gross), engine Roboto font, audio TBD. Keep invoices.

## 7. Making the cover image (options - Brandon picks)

**A. DIY from an in-game render + a font (recommended, ~1-2 hours)**
- Capture a hero shot of the Synty ships (Capture.ps1, 2560x1440 desktop size is plenty for 630x500).
- Put "DEADWATER RUN" over it in an open-licence display font (asset-scraper can bring 2-3 options; check the licence first).
- Crop to 630 x 500 (and 1260 x 1000 for sharpness on high-DPI screens if itch accepts it - verify).

**B. Commission key art (~1-3 weeks, rough guess $100-$400)**
- Unlikely before Dec 3; can replace the DIY cover after launch at no cost - itch pages can be edited any time.

## 8. Everything blocking the Dec 3 launch

1. **Cover image** - none exists. Option A above.
2. **Itch account, developer name, payment mode** - needs Brandon (Checklist section 3).
3. **Tagline + description picks, genre, tags** - needs Brandon (options above).
4. **AI disclosure** - Brandon answers (section 6).
5. **Release candidate build** (Nov 30) and butler push (Brandon).
6. **Audio** - the game has none yet; description must not promise music until it ships.
7. Recapture screenshots 7-13 now that shop, events and captain abilities are real.
