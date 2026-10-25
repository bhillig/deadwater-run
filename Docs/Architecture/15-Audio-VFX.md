# 15 - Audio and VFX Hookup

## Problem
- Hit and cannon VFX (Synty VFX pack) and audio are Must-ship. Audio source is decided by Nov 8.
- Rules from agent definitions: VFX and combat sounds are triggered by GameplayCues, never called directly from gameplay code; one mix; concurrency limits; settings volumes.
- VFX (gameplay-engineer) and sound (sound-wrangler) are different owners - they must not fight over the same binary asset.
- The simulator and tests must produce no VFX or audio.

## Decisions

### D1 - Combat presentation trigger
| Option | Verdict |
|---|---|
| **A. GameplayCues on GEs and abilities; cue notifies spawn Niagara; sound comes from a shared `UDwAudioCueMap` lookup inside our C++ cue notify base** | **Chosen.** Epic-intended, one trigger for VFX and sound, separate assets per owner |
| B. Sound in the same cue notify asset (Burst "Sounds" list) | Rejected: two agents editing the same binary asset -> merge conflicts and ownership blur |
| C. Gameplay code calls `SpawnSystemAtLocation` / `PlaySound` | Rejected: violates the cue rule, breaks headless mode |

### D2 - Music
| Option | Verdict |
|---|---|
| **A. `UDwMusicSubsystem` (`UGameInstanceSubsystem`) switches tracks by state tag (`Music.Menu`, `Music.Map`, `Music.Combat`, `Music.Boss`, `Music.Boss.Enraged`, `Music.Victory`, `Music.Defeat`) from `UDwMusicSet`, with crossfades** | **Chosen.** Survives level travel, data-driven |
| B. Music placed in levels | Rejected: can't follow run phases |

## Classes

| Class | Folder | Owner | Responsibility |
|---|---|---|---|
| `UDwGameplayCueNotify_Burst : UGameplayCueNotify_Burst` | Presentation/ | gameplay-engineer | One-shot cues (hits, cannon fire/impact, death, heal). After the Burst's VFX, looks up `UDwAudioCueMap` for its cue tag and plays the sound at the cue location |
| `ADwGameplayCueNotify_Looping : AGameplayCueNotify_Looping` | Presentation/ | gameplay-engineer | Status loops (frozen ice, fear smoke, guarded shield, enraged Kraken) with start/loop/stop sounds from the map |
| `UDwAudioCueMap : UPrimaryDataAsset` | Presentation/ (class), Content Audio/ (asset) | class: gameplay-engineer; `DA_AudioCueMap` contents: sound-wrangler | `TMap<FGameplayTag, FDwCueSound>` (sound, volume, pitch range, concurrency, `bLoop`) |
| `UDwMusicSet : UPrimaryDataAsset` | same | class: gameplay-engineer; contents: sound-wrangler | `TMap<FGameplayTag, TSoftObjectPtr<USoundBase>>`, crossfade time |
| `UDwMusicSubsystem` | Presentation/ | gameplay-engineer | `SetMusicState(Tag)`; listens to run phase and `FDwCombatEvent` (Kraken phase change) |
| `UDwPresentationSubsystem` | Presentation/ | gameplay-engineer | Instant mode, playback rate (`02-GAS-Foundation.md`) |
| `GC_*` cue notify assets, `NS_*` Niagara | Content VFX/ | gameplay-engineer | Built from the Synty VFX pack, pooled (`Niagara` pooling method `AutoRelease`), bounded lifetimes |
| Sound classes `SC_Master`, `SC_Music`, `SC_SFX`, `SC_UI`; mix `SM_Base`; concurrency `SCON_Hits`, `SCON_Cannon`, `SCON_UI` | Content Audio/ | sound-wrangler | One mix, volumes for settings, stacking limits |

## Cue catalogue (v1.0)

| Cue tag | Fired by | VFX | Sound |
|---|---|---|---|
| `GameplayCue.Hit.Melee` | `GE_Damage` (melee abilities set the cue) | slash spark | sword hit |
| `GameplayCue.Hit.Ranged` | ranged abilities | muzzle + impact | gunshot + impact |
| `GameplayCue.Cannon.Fire` | `UDwGA_FireCannon` (on the cannon tile) | muzzle smoke | cannon boom |
| `GameplayCue.Cannon.Impact` | cannon GE on the ship | splinters, smoke | hull crack |
| `GameplayCue.Ship.Sink` | `HandleShipSunk` via ASC `ExecuteGameplayCue` | splash, sinking | creak + splash |
| `GameplayCue.Unit.Death` | `HandleUnitDied` | death puff | death sound |
| `GameplayCue.Unit.Heal` | `GE_Heal` | heal glow | heal chime |
| `GameplayCue.Status.Fear`, `.Frozen`, `.Rooted`, `.Guarded`, `.Cowering` | status GEs (looping) | status loop | start/stop |
| `GameplayCue.Ability.<Name>` | content abilities as needed (Smoke and Fire, Six Pistols, Rally Cry, Charge, Grapple, Eye of the Tiger) | per ability | per ability |
| `GameplayCue.Kraken.<Move>`, `GameplayCue.Kraken.Enrage` | Kraken abilities, phase change | per move | per move |

- Cues carry the instigator, target and hit location in the effect context; Six Pistols fires one cue per hit.
- In instant mode ASCs suppress cues (`bSuppressGameplayCues`), so simulator and tests are silent.
- UI sounds are not cues; they go through `DA_UISoundMap` (`14-UI.md`).

## Settings and mix
- `UDwGameUserSettings` stores Master/Music/SFX/UI volumes (0-1) and applies them with `UGameplayStatics::SetSoundMixClassOverride` on `SM_Base` for each sound class referenced in `UDwGameData` (`16-Shell.md`).
- Pause: SFX keep their state (world pause stops montages and cue timelines); music ducks via a pause mix (sound-wrangler).

## Connections
- `02-GAS-Foundation.md`: cue tags on GEs and abilities, instant mode.
- `03-Combat-Turns.md`: death/sink cues; combat events for music.
- `08-Kraken-Boss.md`: enrage cue and music state.
- `14-UI.md`: UI sound map.
- `16-Shell.md`: volume settings.
- Asset sources: Synty Pirate + VFX packs; audio options from asset-scraper (DW-005), decided by Brandon Nov 8.

## Build order
1. DW-114 (gameplay-engineer): cue notify bases, `UDwAudioCueMap`, `UDwMusicSet`, `UDwMusicSubsystem`, placeholder `GC_Hit_Melee` and `GC_Cannon_Impact` with engine-default VFX.
2. DW-113 (sound-wrangler): sound classes, mix, concurrency, empty `DA_AudioCueMap`/`DA_MusicSet`, one placeholder sound wired through the map.
3. DW-307 (gameplay-engineer): Synty VFX for every cue in the catalogue.
4. DW-311 (sound-wrangler): import approved audio, fill the cue map and music set.
5. DW-403 (sound-wrangler): loudness pass, concurrency tuning, settings verification.

## How it's tested
- Data validation: every cue tag used by a GE/ability has a cue notify asset; every cue notify tag has an audio map entry (warning until audio lands, error at RC).
- Test world: instant mode suppresses cues (count of spawned Niagara components = 0).
- Clip with sound in every sound-wrangler PR; QA checks a big turn (Arms of Fury, Six Pistols) doesn't stack into noise.

## What the player sees and hears
- The cannon booms, the hull splinters, the enemy pirate cries out as he falls, and the music swells when the Kraken enrages - always in sync with the hit, never on a dead unit, never deafening.
