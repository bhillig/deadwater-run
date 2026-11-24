# Deadwater Run - Scope (v1.0)

- **Launch:** Thursday 2026-12-03, Itch.io, $5
- **Why Dec 3 / Itch.io:** on 2026-11-20 Brandon moved the release from Steam to Itch.io and the launch from Dec 10 to Dec 3. Itch has no store-page or build review, so there are no external deadlines left. Every date below is self-imposed
- **Capacity:** 15 hrs/week of human time + multiple AI agents
- **Bottleneck:** Brandon's review and approval time, not agent output

## Release milestones (Itch.io, self-imposed)
- [ ] Audio source picked by Brandon: by Sun Nov 22
- [ ] Itch account ready: payout method + tax interview (if itch collects payments), developer name: by Nov 25
- [ ] Itch page drafted (cover image, screenshots, copy, price), kept as **Draft**: by Nov 27
- [ ] Release candidate build uploaded with butler to the draft page: Mon Nov 30
- [ ] Final build uploaded, page checked from a clean download: Wed Dec 2
- [ ] Page set to **Public**: Thu Dec 3 (Brandon)

## Must ship (v1.0)
- **Combat:** turn order (player first), crew HP/MP, ship HP; attack / ability / item / cannon actions; win by sinking the ship or killing the crew; lose the same ways; plunder on crew wipe
- **Run loop:** 3 captains (2 combat abilities + 1 run rule each), 8-9 node map (excluding boss, 5 fights guaranteed) following the node rules, shop, ~10-12 events (60/40 good/bad)
- **Content:** 4-5 enemy crews, ~12 crew abilities, 2 items (health / mana restore), Kraken (both phases + tentacle targeting)
- **Shell:** main menu, settings, pause, run summary. No mid-run saves (runs are ~30 min)
- **Presentation:** Synty Pirate + VFX art, readable combat UI, hit/cannon VFX, audio

## Cut until after launch
- Meta-progression / unlocks
- Achievements
- Controller support
- Localization
- Extra captains or bosses
- Steam release (Itch.io only for v1.0)

## Agents
| Agent | Owns |
|---|---|
| Orchestrator | Turns this scope into tickets and routes them to agents |
| Technical Architect | High-level architecture; sets direction the other agents build on |
| Gameplay Engineer (C++/GAS) | Combat, turn manager, attributes, abilities, effects; map, shop, events, captains, run state (data-driven); Niagara VFX |
| Tools Engineer | Editor tools, test harness, headless run simulator |
| AI Engineer | Enemy crews and Kraken behavior |
| UI Engineer (UMG) | Combat HUD, map, shop, menus |
| UI Juice Engineer | UI animations and UI sounds |
| Balance Designer | Tuning from simulations: win rate, run length, costs |
| QA Tester | Breaks the game, bug reports with suggested fixes |
| Sound Wrangler | Sound assets, mix, combat sound cues |
| Release Engineer | Packaged builds, Itch.io butler uploads, Itch page checklist |
| Asset Scraper | Finds art/audio options for approval (2-3 options per request) |
| Event Writer | Generator in the content loop: drafts events (text, choices, outcomes) |
| Content Critic | Evaluator in the content loop: scores generated content 0-100 with reasons against the GDD, tuning and style guide |
| Content Refiner | Refiner in the content loop: rewrites only what the critic scored low |
| Content Director | Runs the content loop from my request: brief -> generate -> score -> refine, no human step, scored drafts back |

All systems are built to work together - no system in a silo. See `Docs/Orchestration.md`.

## Timeline
| Week | Dates | Exit criteria |
|---|---|---|
| 0 | Oct 23-25 | Unreal MCP connected, GAS enabled, agent definitions written, data layout agreed |
| 1 | Oct 26 - Nov 1 | One full combat playable with placeholder art |
| 2 | Nov 2-8 | Full run playable start to finish (placeholder boss); **audio source decided (Nov 8)** - missed, now Nov 22 |
| 2b | Nov 9-15 | Catch-up: full run playable with placeholder Kraken (done Nov 11), Synty art phase 1, store page package |
| 3 | Nov 16-22 | All content in, Kraken done (both done Nov 19); **audio source picked (Nov 22)** |
| 4 | Nov 23-29 | Audio in, balance from simulations, QA content sweep, Itch page drafted (Nov 26-27 is US Thanksgiving) |
| 5 | Nov 30 - Dec 3 | Release candidate Nov 30, regression + fixes only Dec 1-2, **launch Thu Dec 3** |

Moving the launch to Dec 3 removes a week. At risk unless something is cut: UI juice (DW-402), mix pass (DW-403), balance dashboard (DW-309), content wizard (DW-213), packaged autoplay (DW-407), Synty animation pass (DW-307 phase 2). Brandon decides what goes.

## Rules
- Every agent delivery is a PR with tests and a clip or screenshot from the game, so it can be reviewed in minutes
- Approvals are batched once a day
- New ideas go on the post-launch list, never into v1.0

## Open items
- Audio source (SFX + music): asset-scraper options requested 2026-11-20; Brandon picks by Nov 22
- Which at-risk items to cut for Dec 3 (list above)
