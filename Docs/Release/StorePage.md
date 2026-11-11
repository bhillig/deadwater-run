# Deadwater Run - Steam Store Page Package (DW-308)

Prepared 2026-11-11 by the Release Engineer for submission by **2026-11-19** (originally Nov 12; schedule moved one week later on 2026-11-11). Nothing here has been submitted. Brandon presses every button in Steamworks.

Status key: **ready** = can be pasted/uploaded as is. **needs Brandon** = a decision, a fact only he has, or an account action. **blocked** = cannot be done until something else lands.

> Pixel sizes and field names below are from the Steamworks documentation as I know it. The Steamworks "Graphical Assets" and "Store Page" admin screens are the source of truth - the first thing to do before submitting is compare the sizes in section 2 with what the upload slots say, and tell me if any differ. I could not log in to check (out of scope).

---

## 1. Basic info

| Field | Value | Status |
|---|---|---|
| Game name | Deadwater Run | ready |
| Type | Game | ready |
| Price | **$5.00 USD** (Steam price point exists). Regional pricing: Steamworks suggests per-region prices from the USD price; accept the suggestion or edit | needs Brandon (accept regional suggestions? launch discount yes/no) |
| Release date | **2026-12-10** (Thursday). Set as "Coming Soon" with this date | ready (do not change; Brandon sets it) |
| Early Access | No | ready |
| Platforms | Windows 64-bit only. No Mac/Linux build exists | ready |
| Developer / Publisher | Not in the repo beyond `CopyrightNotice=Brandon Hillig 2026`. Use the name on the Steamworks account (individual or studio name) | needs Brandon |
| Website | Optional. None exists | needs Brandon (leave blank is fine) |
| Support URL / email | Steam asks for one. Nothing in the repo | needs Brandon |
| Copyright line | `Brandon Hillig 2026` (from `Config/DefaultGame.ini`) | ready |
| Languages | Interface: English. Full audio: none (no voice). Subtitles: none | ready (confirm; no localization per Scope) |
| Controller support | None (cut by Scope). Keyboard + mouse only. Say so in the description if Brandon wants no surprise refunds | ready |
| Steam features (achievements, cloud, trading cards, workshop, overlay) | None enabled. Architecture doc 18 D1: no Steamworks SDK code in v1.0. Do not tick Achievements / Cloud | ready |
| Steam Deck | Do not claim anything. Valve rates it themselves; unverified | ready (leave alone) |
| Mid-run saves | None (Scope). Worth one honest line in the description (runs are ~30 min) | ready |

## 2. Required graphical assets

None of the capsule/header/library art exists. **Capsules are required to submit the page for review**, so this is the critical path for tomorrow - see section 7 for how to produce them. No AI-generated art (values.md: only with Brandon's approval; none generated here).

All store images must show the game logo/title on the capsule types. Valve rules (verify): no review quotes or award text, no price or discount text, no "Coming Soon"/"Now Available"/release-date text on capsule art; keep the logo legible at the small size.

| Asset | Required pixel size | Format | Required? | Status |
|---|---|---|---|---|
| Header capsule | **920 x 430** | PNG/JPG | Required | needs Brandon (no art) |
| Small capsule | **462 x 174** | PNG/JPG | Required | needs Brandon (no art) |
| Main capsule | **1232 x 706** | PNG/JPG | Required | needs Brandon (no art) |
| Vertical capsule | **748 x 896** | PNG/JPG | Required | needs Brandon (no art) |
| Library capsule | **600 x 900** | PNG/JPG | Required (Steam library / Deck) | needs Brandon (no art) |
| Library header | **920 x 430** | PNG/JPG | Required (can reuse header capsule art) | needs Brandon (no art) |
| Library hero | **3840 x 1240** | PNG/JPG | Required for a good library page (no text/logo on it) | needs Brandon (no art) |
| Library logo | **1280 x 720 max** (logo scaled to fit, **transparent PNG**) | PNG | Required for a good library page | needs Brandon (no art) |
| Page background | **1438 x 810** | PNG/JPG | Optional | optional, skip |
| Community icon | **184 x 184** | JPG | Required | needs Brandon (no art) |
| Client icon | **32 x 32** (also a Steamworks icon upload; format per slot) | JPG/ICO | Required | needs Brandon (no art) |
| Screenshots | **min 5, 1920 x 1080 (16:9) recommended**; ticket minimum 1280x720 | PNG/JPG | Required, at least 5 | **ready** - 13 captured, see section 3 |
| Trailer | 1080p MP4/H.264 recommended | video | **Optional** for submission and launch | optional, none exists. A trailer is a strong wishlist driver; not a blocker |

One master composition at 3840 x 2160 (or the 3840 x 1240 hero crop plus a 1232 x 706 crop) can feed most slots. See option B/C in section 7.

## 3. Screenshots (real, from the current build)

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

Honest read for Brandon: shots 1-6 are the store page. They are six moments of the same screen, so the gallery will look repetitive; the Synty ships and characters are the selling point. If time allows after the Nov 22 art/UI pass, recapture 7-9 and 11 (and the shop/events once built) and swap them in - screenshots can be edited after the page is approved (verify in Steamworks whether edits to an approved page need re-review). Combat HUD polish items I noticed (not mine to fix): hint bar at the bottom touches the bottom edge of the action cards; the top "Round 1 - YOUR TURN" text sits close to the top edge.

**Order of the gallery is a creative choice - options below:**
- Lead A: `03-combat-cannon` (shows the unique ship-HP hook first)
- Lead B: `01-combat-move` (the clearest "what is this game" frame)
- Lead C: `06-combat-victory` (outcome and plunder; punchy but more text)

## 4. Descriptions (Brandon picks one of each)

Only features in Scope.md "Must ship" (plus the design doc's core loop). Numbers like node counts are left out because `context/offer.md` says 7-8 node runs while Scope.md says 8-9 - **Brandon: which is right?** Captain names are from `DeadwaterRun.md`.

### Short description (limit 300 characters)

- **S1 (pitch-led, 285 chars):** Battle out of the Aquadia seas in this exciting and tense turn-based pirate roguelike. Pick a captain, chart a route through fights, shops and events, and choose how to win: sink their ship fast, or slay the crew and plunder. Face the Kraken to escape.
- **S2 (hook-led, 266 chars):** Every ship has a health bar. Sink your rival's ship to end the fight fast, or cut down the crew to plunder their gold. A 30-minute turn-based pirate roguelike with three captains, a branching sea chart and a Kraken waiting at the end.
- **S3 (crew-led, 277 chars):** Command a small pirate crew across the seas of Aquadia. Move, fight, fire the cannons, then decide: sink the ship or take the plunder. Lose a pirate and they're gone for good. Survive the sea chart and take on the Kraken. About 30 minutes a run.

(Character counts measured with a shell script on 2026-11-11; Steam limit is 300.)

### Long description ("About This Game", Steam BBCode)

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

Copy claims that depend on unfinished work (do not publish unless true at launch): "three captains... own combat abilities" (the Conqueror/Fearmongerer abilities show placeholder text, DW-207/208), "shops", "events" (shells today, DW-204/205), "Kraken ... two phases / tentacles" (DW-209 in progress), "rival crews with their own tactics" (DW-106 default AI only). Brandon: copy can go in tomorrow, but re-read it against the build on Dec 2.

## 5. Tags, genres, categories

Tags are a creative/marketing choice; two options (Steam allows up to 20; the first few weigh most, I believe - verify):
- **Tags option 1 (tight, 10):** Roguelike, Turn-Based Strategy, Turn-Based Tactics, Pirates, Strategy, Singleplayer, Indie, Permadeath, Naval, Roguelite
- **Tags option 2 (broader, 14):** Roguelike, Turn-Based Strategy, Pirates, Turn-Based Tactics, Roguelite, Strategy, Tactical RPG, Singleplayer, Indie, Permadeath, Procedural Generation, Grid-Based Movement, Naval, Difficult
- Not appropriate (not in the game): Deckbuilding, Card Game, Controller, Co-op, Multiplayer, Early Access.
- "Difficult" only if Balance (DW-401) lands somewhere hard; ICP wants medium difficulty - Brandon's call.

Genres: Strategy (primary), Indie. Add RPG only if Brandon wants it (crew levels and perks exist). - needs Brandon
Categories: Single-player. Nothing else.

## 6. System requirements (DERIVED - guesses flagged)

Basis: Unreal Engine 5.8 (`DeadwaterRun.uproject`), Win64 only, `Config/DefaultEngine.ini` enables Lumen (`r.DynamicGlobalIlluminationMethod=1`, `r.ReflectionMethod=1`), Virtual Shadow Maps (`r.Shadow.Virtual.Enable=1`) and mesh distance fields, no static lighting. That means a DirectX 12 / Shader Model 6 GPU. **No Shipping package or benchmark has been run yet (Build/Package.ps1 / DW-218 still open), so every number below except the OS family is a GUESS** until measured on real hardware.

| | Minimum (guess) | Recommended (guess) |
|---|---|---|
| OS | Windows 10 64-bit | Windows 10/11 64-bit |
| Processor | Quad-core, ~2018 (e.g. Intel Core i5-8400 / AMD Ryzen 5 1600) - GUESS | 6-core (e.g. Ryzen 5 3600 / Core i5-10400) - GUESS |
| Memory | 8 GB RAM - GUESS | 16 GB RAM - GUESS |
| Graphics | DX12 / SM6 GPU, ~GTX 1060 6 GB / RX 580 - GUESS | ~RTX 2060 / RX 5700 - GUESS |
| DirectX | Version 12 | Version 12 |
| Storage | **3 GB** - GUESS. Source `Content/` is 1.0 GB uncooked; the cooked/paked size is unknown until the first package | same |
| Notes | 1080p target. Keyboard and mouse. | |

To replace guesses with facts (release engineer, after Package.ps1 exists): package Shipping, record disk size, run the smoke test on the weakest available GPU and read frame times via `stat unit` in a Test build, then lower or raise the tiers. Fallback if no time: ship the guesses but keep them conservative (higher), since a wrong low spec causes refunds.

## 7. Content survey (DRAFT answers - Brandon must answer himself in Steamworks)

Valve's questionnaire wording changes; these are my draft positions against the build as of today.

| Topic | Draft answer | Basis |
|---|---|---|
| Nudity / sexual content | None | None in game |
| Violence | Yes, stylized and mild: swords, muskets, cannons on low-poly characters. Not frequent graphic violence | Screenshots show no blood. **Brandon: confirm no blood/gore VFX exist or are planned** |
| Gore | None (confirm per above) | |
| Mature themes | Piracy, combat, death of crew (permadeath) - nothing beyond that | |
| Alcohol / drugs | "Grog" is a healing item name; no depiction of drinking beyond an item label | Item list in build |
| Gambling / loot boxes / real-money | None. Random events use in-game gold only | |
| In-app purchases / DLC | None | $5 premium |
| Online / multiplayer | None | |
| User-generated content / chat | None | |
| Generative AI disclosure (Steam requires a statement) | **Brandon decides.** Draft: no AI-generated art, audio or text ships in the game; AI was used as a development/coding tool. Valve's guidance (verify wording) separates AI-assisted tooling from pre-generated content shipped to players. Check: the in-game UI and VFX were built with agent help (values.md allows it) - confirm none of it is AI-generated imagery | values.md: AI art only with approval; none approved |
| Third-party content | Synty Pirate + VFX packs (licensed), Unreal Engine (Epic EULA), engine Roboto font | Confirm Synty licence permits commercial Steam use (it normally does) and keep invoices |
| Ratings (IARC questionnaire inside Steamworks) | Expect a low age rating (cartoon violence) | Complete once; Brandon answers |
| Steam "mature content" description / adult-only flag | No | |

## 8. Creating the capsule / header art (options - Brandon picks)

No art exists and no AI art was generated. Effort figures are my estimates; commission prices are rough and unverified.

**A. DIY from an in-game render + a font (fastest, ~4-8 hours for the whole set)**
- Capture a hero shot of the Synty ships at high res (Capture.ps1 goes up to the desktop size, 2560x1440; a 2x upscale is soft - a purpose-built hero camera in the editor would be better, which is a small gameplay/tools-engineer task and needs your approval since it touches game code).
- Put the "DEADWATER RUN" logo over it in an open-licence display font. The Asset Scraper can bring 2-3 font options (DW-005 was meant to; UI doc U6 is still open) for your approval - do not use a font before the licence is checked.
- Compose one 3840x2160 master in Photopea/GIMP/Affinity, crop to all ten sizes in section 2, export the logo as a transparent PNG for the library logo.
- Pros: done in 1-2 days, consistent with the game, uses real Synty art. Cons: looks like a screenshot with a title; no key-art polish.

**B. Commission key art (best look, ~1-3 weeks wall-clock, my rough guess $150-$600 for a capsule set)**
- Brief an illustrator for one key illustration (ship vs Kraken or the two ships) plus logo; they deliver layered files; crop to sizes.
- Pros: strongest wishlist conversion, reusable for the trailer and the library hero. Cons: **unlikely to land by Nov 19**, and capsules are required to submit. Risks the Nov 26 Coming Soon deadline unless combined with C.

**C. Hybrid (recommended path to evaluate; ~2-3 hours now, art later)**
- This week: build a minimal title-card set from option A (solid/gradient sea colour or a screenshot crop + logo) that meets every size so the page can be submitted and reviewed.
- Commission or polish real key art during Nov 19-Dec 9 and swap it in (Valve allows replacing assets after approval; I believe it does not restart the 2-week Coming Soon clock - verify).
- Pros: keeps the Nov 19 / Nov 26 dates safe. Cons: the Coming Soon page, where wishlists accrue, is the weak version for the first 2 weeks.

## 9. Everything blocking submission (by Nov 19)

1. **Capsule/library art** - none exists (10 sized images). Hard blocker for submit. Decide A/B/C today.
2. **Developer/publisher name, support email/URL** - needs Brandon.
3. **Short + long description picks, tags, genre** - needs Brandon (options above).
4. **Content survey, IARC rating, AI-disclosure** - Brandon answers in Steamworks.
5. **Steamworks paperwork (tax, bank, identity)** - must be complete before release; check it shows "complete" now. needs Brandon.
6. **App fee paid date** - Scope says paid but no date is recorded. Release Dec 10 requires payment on or before **Nov 10** (30-day rule). Brandon: confirm the payment date.
7. System requirements are guesses; acceptable for submission, replace later.
8. No build is needed to submit the store page, but **no Shipping package has been made yet**; see `Docs/Release/Checklist.md`.
