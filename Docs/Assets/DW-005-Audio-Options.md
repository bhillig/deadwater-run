# DW-005 - Audio options (asset-scraper, 2026-11-20)

Nothing downloaded, bought or imported. Prices as listed on 2026-11-20. Licences were read from storefront pages only - read the licence file in each download before relying on it. File lists could not be opened; coverage is from page descriptions.

Synty Pirate + VFX packs contain no audio, so audio is a full gap. Integration: sound-wrangler (DW-113/114 foundation, DW-311 import, DW-403 mix).

## (a) SFX

| Option | Price | Licence | Coverage | AI | Notes |
|---|---|---|---|---|---|
| **A1. Epic Stock Media "Pirate Game"** - https://epicstockmedia.com/product/pirate-game/ (buy direct, not Unity Asset Store) | $94 (sale from $129) | Single-user, royalty-free; EULA not read (attribution / redistribution / AI-training unclear) | Swords, flintlock, boat guns, cannons, explosions, wood creaks, ship foley, water, ship + ambience loops, chests, pick-ups, UI, notifications, voice, magic, creatures (partial Kraken), some music loops | No statement | 3,200 files, 9.2 GB, 96k/24 + 44.1k/16 WAV. Cinematic, slightly realistic for low-poly |
| A2. Fusehive "Pirate Adventure SFX Library" - https://fusehive.itch.io/pirate-adventure-sound-effects-library-caribbean-gold-treasure-theme-royalty-f | $99.99 ($399.99 bundle) | "Perpetual, royalty-free"; terms unclear | Cannons, swords, ship battles, ambience, treasure. No UI, coins, Kraken, storm; pistol unclear | Bundle: no AI; single: no statement | Less coverage than A1 for the same price - not recommended |
| A3. Free stack: Kenney Interface/UI/Impact (CC0) + Sonniss GDC 2026 bundle (free, no attribution, no AI training) + Kraken pack | $0-19 | CC0 / Sonniss v2.0 / CC BY 4.0 (credit) | UI and impacts certain; cannon, guns, ocean, storm unconfirmed in Sonniss | Mixed | Lots of sorting time, risky for Dec 3 |

Kraken add-on: Epic Stock Media "Evolved Game Creatures" - https://epicstockmedia.itch.io/evolved-game-creatures - $18.85 (sale), Kraken set (attacks, pain, death, spawn, impacts), "No generative AI". Free alternative: thesoundrack "Monster Roar" - https://thesoundrack.itch.io/monster-roar-sound-pack - pay what you want, CC BY 4.0 (needs a credit).

**Recommendation:** A1 + Evolved Game Creatures (~$113). Add Kenney Interface Sounds (free) only if A1's UI set is weak.

## (b) Music

| Option | Price | Licence | States | AI | Notes |
|---|---|---|---|---|---|
| **B1. Music Tale "Epic Battle Orchestral Music Pack"** - https://music-tale.itch.io/epic-battle-orchestral-music-pack | $19.99 | "Royalty-free"; commercial terms not stated - check before buying | Combat ("Sea Warfare", "Battlefield"...), Boss ("Boss Battle"), Enraged ("Frightening Assault"), victory/defeat tags. No menu/map | No statement | 12 tracks, intro/loop/tag cuts, 44.1k/16 WAV |
| **B2. Tim Beek "Sea Shanties: Pirate Edition"** - https://timbeek.itch.io/seashanties | $20 | Commercial, no credit needed; licence PDF in download | Menu, Map | "100% human-composed" | 8 shanties, WAV + OGG; loopability not stated |
| B3. Cybersprocket "Galleons and Gunpowder" - https://cybersprocket.itch.io/galleons-and-gunpowder | ~$6-7 | Commercial, credit required | Cinematic pirate, boss/tense | **AI-assisted** - needs Brandon's explicit approval (values.md) | Not recommended |

Free swap for menu/map: thesoundrack "Pirate Music Pack" - https://thesoundrack.itch.io/pirate-music-pack - CC BY 4.0, loops + stems, no AI. Fallback for combat/boss if B1's licence is unclear: PPEAK "RPG Bosses" - https://ppeak.itch.io/rpg-bosses - CC BY 4.0.

**Recommendation:** B1 (combat/boss/enraged) + B2 (menu/map), ~$40.

## Total and gaps

- Recommended: ~$153 (~$134 with free Kraken + free menu/map). All one-time purchases.
- Gaps: storm ambience (only likely in A1), Kraken slam/grab foley (layer from A1 water/impacts), status loops (Fear, Frozen, Rooted, Guarded, Cowering) and heal chime (partial), pirate death voices (A1 "Voice" may be enough).
- Before buying: read the Epic Stock Media EULA and Music Tale licence; ask both about AI. CC BY items need a credits screen (ui-engineer).
- Buy by ~Nov 24 to leave DW-311 2-3 days.
