# Music Pick List (DW-311)

**Status (2026-11-20): approved by Brandon. The 9 primary cuts and the 2 tavern stopgap loops are imported (11 `SW_Music_*` assets, 60.7 MB). No alternates imported. Licence unconfirmed - Brandon checking.**

Source: "Epic Battle Orchestral Music Pack" (bought by Brandon), 12 tracks, 50 WAVs, all stereo 44.1 kHz 16-bit, kept outside the repo. Picks are made from file names, cut lengths and the architecture states only: I cannot listen to audio, so please audition the picks (especially loop seams and the long INTRO cuts) before approving.

## Licence caveat (confirm before release)

- **Licence unconfirmed - Brandon checking.**
- **No licence file came in the download.** The store page says only "royalty-free". That is not a licence: it does not say whether commercial Steam release, redistribution inside a game build, or credit requirements are covered.
- Brandon must confirm the licence terms (store licence page, or the seller) and keep a copy in `Docs/Audio/` before release. This does not block wiring or testing now. The same check applies to the Pirate Game pack (its README stayed outside the repo).

## States and picks (`Docs/Architecture/15-Audio-VFX.md`, D2)

States: `Music.Menu`, `Music.Map`, `Music.Combat`, `Music.Boss`, `Music.Boss.Enraged`, `Music.Victory`, `Music.Defeat`. I would add no new states: shop, events and the run summary can reuse `Music.Map`.

Cut names: INTRO plays once, then LOOP repeats; TAG is a short ending used as a one-shot stinger. Asset names are `SW_Music_<State>_<Track>_<Cut>` under `Content/DeadwaterRun/Audio/Music/`, class `SC_Music`, streaming (`LoadOnDemand`), compression quality 60, looping on LOOP cuts only. The machine-readable list is `Tools/Content/music_pick_manifest.csv`.

| State | Primary (imported on approval) | Cuts and lengths | Alternate (not imported unless asked) |
|---|---|---|---|
| `Music.Combat` (2 tracks rotating, see below) | **Sea Warfare** and **Sword Fight** | each LOOP only: 115.6 s and 128.0 s | Furious Fight: INTRO 2.4 s + LOOP 114.3 s |
| `Music.Boss` | **Boss Battle** | INTRO 8.1 s + LOOP 112.0 s + TAG 3.6 s | Epic Clash: INTRO 4.3 s + LOOP 137.1 s |
| `Music.Boss.Enraged` | **Frightening Assault** | INTRO 4.0 s + LOOP 96.0 s | Deadly Encounter: LOOP 115.4 s |
| `Music.Victory` | **Heroic Mission** TAG | 3.9 s stinger | Battlefield TAG, 6.3 s (Boss Battle TAG, 3.6 s, is also imported and can end a boss win) |
| `Music.Defeat` | **Subterranean Monsters** TAG | 6.1 s stinger | Deadly Encounter TAG, 5.9 s |
| `Music.Menu` | **not covered by this pack** | | |
| `Music.Map` | **not covered by this pack** | | |

### Combat variety
- Two combat tracks, picked per fight (no repeat of the last one). Sea Warfare and Sword Fight fit the pirate theme by name and their LOOP cuts are long (about 2 min), so a typical fight never repeats the loop.
- Sea Warfare and Sword Fight have long INTRO cuts (135.6 s and 32.1 s). They are skipped: a fight should start in the music, not 30 s to 2 min of build-up. If your ear says the LOOP cut starts too suddenly, the short intros of Furious Fight (2.4 s) or Epic Clash (4.3 s) are the safe alternates.
- Combat tracks do not end with their TAG: fights end on the Victory or Defeat stinger, so the combat TAGs are not imported. Crossfade out on the state change.
- Optional later: Enemies Coming ships a PERCUSSION ONLY LOOP at the same length (58.5 s) as its LOOP, so it could act as a "light combat" layer for the first turns, with the full LOOP fading in later. Not in this list: it needs a layered music design (DW-114 supports one stream per state).

### Kraken enrage
- Boss Battle plays through phase 1. At the phase change (`Music.Boss.Enraged`, below 30% health) the subsystem crossfades to Frightening Assault (the intro is a short 4 s riser into a 96 s loop).
- I did not pick a percussion-only layer for the enrage: the percussion-only cuts in this pack (Enemies Coming, Jungle Battle, Sword Fight) are matched to their own tracks, so layering them over Boss Battle would clash in tempo. A crossfade to a different track is the reliable option.

### Menu and Map: uncovered, and what I would use meanwhile
This pack is all battle music, so Menu and Map are expected to stay open (a shanty pack may be bought). Stopgap options:
1. **Pirate Game pack tavern loops** (already in your pack folder, `Music_Loops`; **needs your approval, since the SFX approval did not cover them**): `Music.Menu` = Song A "Tavern Exterior" (42.0 s loop), `Music.Map` = Song B "Tavern Exterior" (27.4 s loop). Jaunty folk fits the theme far better than battle music. **Approved placeholder - replace with a shanty pack post-launch or if bought.** Imported as `SW_Music_Menu_TavernA_Loop` and `SW_Music_Map_TavernB_Loop` (manifest `Stopgap` rows). The "Interior" versions of both songs are available as alternates. Short loops: a 27 s Map loop will be noticed on a long map screen, so this is a stopgap only.
2. From this pack, if you prefer one family of sound: Heroic Mission LOOP (77.5 s, by name the least aggressive, not auditioned) for `Music.Map`, and Battlefield LOOP (83.5 s) for `Music.Menu`. I would not choose this over the tavern loops.

**Flag:** until real Menu and Map tracks exist, shipping with the stopgaps (or silence) is a conscious choice. Do you want the asset-scraper to look for a shanty or exploration pack?

## Loop lengths (all cuts in the pack that are used or alternates)

| Track | INTRO | LOOP | TAG | FULL | WAV MB (INTRO / LOOP / TAG) |
|---|---|---|---|---|---|
| Sea Warfare | 135.6 | 115.6 | 2.6 | 254.4 | 22.8 / 19.4 / 0.4 |
| Sword Fight | 32.1 | 128.0 | 2.4 | 162.6 | 5.4 / 21.5 / 0.4 |
| Boss Battle | 8.1 | 112.0 | 3.6 | 124.1 | 1.4 / 18.8 / 0.6 |
| Frightening Assault | 4.0 | 96.0 | 4.6 | 104.8 | 0.7 / 16.2 / 0.8 |
| Heroic Mission | 77.6 | 77.5 | 3.9 | 158.9 | 13.1 / 13.0 / 0.7 |
| Subterranean Monsters | none | 109.7 | 6.1 | none | - / 18.5 / 1.0 |
| Furious Fight | 2.4 | 114.3 | 4.9 | 121.9 | 0.4 / 19.2 / 0.8 |
| Epic Clash | 4.3 | 137.1 | 4.6 | 145.9 | 0.7 / 23.1 / 0.8 |
| Deadly Encounter | 115.5 | 115.4 | 5.9 | 237.2 | 19.4 / 19.4 / 1.0 |
| Battlefield | 10.7 | 83.5 | 6.3 | 100.7 | 1.8 / 14.0 / 1.1 |

Seconds, all 44.1 kHz stereo. Heroic Mission, Deadly Encounter and Sea Warfare have INTRO cuts as long as their LOOP: those INTROs look like a full first pass of the track, not a short lead-in, which is why only their TAG or LOOP is used. Not used at all: the FULL cuts (rendered whole songs, duplicates of INTRO+LOOP+TAG), Enemies Coming, Jungle Battle (a jungle theme is an odd fit), and the percussion-only cuts.

## What `UDwMusicSet` / `UDwMusicSubsystem` need (DW-114, for gameplay-engineer)
- Per state: an optional INTRO wave then a looping LOOP wave (play the intro once, then the loop, gapless), or a one-shot stinger (Victory, Defeat) with no loop.
- Per state: a pool of entries (Combat has two) with "pick one, not the one played last"; the alternates can join the pool later without code changes.
- Boss end: Boss Battle TAG as an optional end stinger on `Music.Boss`; otherwise Victory plays.
- Crossfade out of any loop on state change (the TAGs are only used where listed).

## Size on disk (git, no LFS)

The throwaway test showed imported assets at about 0.66x the source WAV size (Sword Fight LOOP: 21.5 MB WAV, 14.2 MB `.uasset`). The 0.43x seen on a 3.9 s tag is not representative, so tags are counted at 0.66x.

| Set | Files | WAV MB | Estimated `.uasset` MB |
|---|---|---|---|
| Primaries (imported on approval) | 9 | 80.3 | **about 53** |
| Alternates (only if asked) | 10 | 64.9 | about 43 |
| Stopgap tavern loops (only if approved) | 2 | 12.2 | about 8 |
| Primaries + stopgap | 11 | 92.5 | about 61 |

- The whole pack is 600 MB of WAVs, so importing only the cuts we use keeps the music at about 9% of that.
- The repo already gained 58 MB for the 121 SFX; the music primaries bring it to roughly 111 MB of audio in git. Git LFS for `.uasset` audio is worth deciding before more packs come in.
- Cheaper options if size matters: drop one combat track (Sea Warfare or Sword Fight) to save about 13 to 14 MB, at the cost of combat variety.

## Import (after approval)

```powershell
# primaries only
./Tools/ImportAudio.ps1 -Set Music -PackRoot "C:\Users\Brandon\Downloads\Epic Battle Orchestral Music Pack\Epic Battle Orchestral Music Pack"
# plus the Pirate Game tavern stopgap loops
./Tools/ImportAudio.ps1 -Set Music -PackRoot <epic pack> -IncludeStopgap -StopgapRoot <Pirate_Game_16bit root>
# plus alternates
./Tools/ImportAudio.ps1 -Set Music -PackRoot <epic pack> -IncludeAlternates
```
