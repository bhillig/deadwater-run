# Deadwater Run - Scope (v1.0)

- **Launch:** Thursday 2026-12-03, Steam, $5
- **Why Dec 3:** the 2026 Autumn Sale already ran (Oct 1-8); launching here gets two full-price weeks before the Winter Sale (Dec 17 - Jan 4)
- **Capacity:** 15 hrs/week of human time + multiple AI agents
- **Bottleneck:** Brandon's review and approval time, not agent output

## Hard external deadlines (Steam)
- [x] App fee paid. Valve requires 30 days between payment and release
- [ ] Store page submitted for review: ~Nov 12
- [ ] Store page public as "Coming Soon" (2-week minimum): by Nov 19
- [ ] Build submitted for review: ~Nov 25

## Must ship (v1.0)
- **Combat:** turn order (player first), crew HP/MP, ship HP; attack / ability / item / cannon actions; win by sinking the ship or killing the crew; lose the same ways; plunder on crew wipe
- **Run loop:** 3 captains (1 unique ability each), 7-8 node map following the node rules, shop, ~10-12 events (60/40 good/bad)
- **Content:** 4-5 enemy crews, ~12 crew abilities, 2 items (health / mana restore), Kraken (both phases + tentacle targeting)
- **Shell:** main menu, settings, pause, run summary. No mid-run saves (runs are ~30 min)
- **Presentation:** Synty Pirate + VFX art, readable combat UI, hit/cannon VFX, audio

## Cut until after launch
- Meta-progression / unlocks
- Achievements
- Controller support
- Localization
- Extra captains or bosses

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
| Release Engineer | Packaged builds, Steam depot upload, store page checklist |
| Asset Scraper | Finds art/audio options for approval (2-3 options per request) |

All systems are built to work together - no system in a silo. See `Docs/Orchestration.md`.

## Timeline
| Week | Dates | Exit criteria |
|---|---|---|
| 0 | Oct 23-25 | Unreal MCP connected, GAS enabled, agent definitions written, data layout agreed |
| 1 | Oct 26 - Nov 1 | One full combat playable with placeholder art |
| 2 | Nov 2-8 | Full run playable start to finish (placeholder boss); **audio source decided (Nov 8)** |
| 3 | Nov 9-15 | All content in, Kraken done, Synty art pass; **store page submitted (Nov 12)** |
| 4 | Nov 16-22 | Balance from simulations, playtests, polish, audio in; **store page live (Nov 19)** |
| 5 | Nov 23-29 | Release candidate; **build submitted (Nov 25)** |
| 6 | Nov 30 - Dec 3 | Fixes only; **launch Dec 3** |

## Rules
- Every agent delivery is a PR with tests and a clip or screenshot from the game, so it can be reviewed in minutes
- Approvals are batched once a day
- New ideas go on the post-launch list, never into v1.0

## Open items
- Audio source (SFX + music) - asset scout to bring options; decide by Nov 8
