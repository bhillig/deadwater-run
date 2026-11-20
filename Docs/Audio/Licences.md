# Audio licences

Record of every third-party audio source shipped in Deadwater Run. Raw packs stay outside the repo; only the imported `SW_*` assets for sounds we use are committed. Add a row before importing anything new.

| Pack | Seller / where bought | Used for | Licence terms (as recorded) | Credit required | Record checked |
|---|---|---|---|---|---|
| Pirate Game (16-bit/44.1k) | Epic Stock Media, epicstockmedia.com (bought 2026-11-20) | 121 SFX (`Content/DeadwaterRun/Audio/SFX/`), 2 tavern loops as Menu/Map music placeholders (`Audio/Music/SW_Music_Menu_TavernA_Loop`, `SW_Music_Map_TavernB_Loop`) | ESM EULA 2023 (`_END_USER_LICENSE_AGREEMENT_2023.rtf` in the download): commercial use in video games allowed (1.1); royalty-free, **no attribution** (1.2); single user, non-transferable (1.3, 1.4); must not redistribute or resell the content as a standalone product or sound library (2.3); no machine-learning training | No | Orchestrator read the EULA in the download, 2026-11-20 |
| Epic Battle Orchestral Music Pack | Music Tale, music-tale.itch.io/epic-battle-orchestral-music-pack (bought 2026-11-20) | 9 music cuts (`Content/DeadwaterRun/Audio/Music/`): combat, boss, enraged, victory, defeat | No licence file in the download. The itch.io page states the music is **royalty-free and licensed for commercial use** (confirmed by Brandon 2026-11-20) | Not stated - none given | Brandon read the itch page, 2026-11-20 |

## Rules that follow from these licences
- Never commit the raw packs (WAVs, CSVs, READMEs, EULAs) - they would be redistributing the library. Only imported assets of sounds the game uses.
- Keep the GitHub repo private for the same reason (imported `.uasset` files contain the audio data).
- Keep the purchase receipts (ESM: proof of purchase validates the licence).
- Keep a saved copy (PDF or screenshot) of the Music Tale itch page licence wording with the receipt, in case the page changes. Not stored in the repo.
- No credits screen is required for either pack. A courtesy credit is optional.
