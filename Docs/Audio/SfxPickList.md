# SFX Pick List (DW-311)

**Status (2026-11-20):** approved and imported (121 primaries). Wired in DW-311 via `Tools/Content/audio_cue_map.json`: every combat, cannon, ship, death, heal, Fear and Kraken cue the game fires. Imported but not wired yet: the `GameplayCue.Ability.*` and Frozen/Rooted/Guarded/Cowering picks (post-launch, Brandon 2026-11-20), the Fear loop (Fear fires as a one-shot), ambience (no player yet), UI sounds (`DA_UISoundMap`, DW-402).

Source: Epic Stock Media "Pirate Game" (bought 2026-11-20), 16-bit / 44.1 kHz, kept outside the repo. Audio gaps flagged here follow the cue catalogue in `Docs/Architecture/15-Audio-VFX.md`.

## Summary

- **121 files to import** (114 one-shots and 7 loops, all stereo 44.1 kHz): 95 on `SC_SFX`, 26 on `SC_UI`. Total length 464 s (about 82 MB of source data once imported, see Risks).
- **49 alternates** (not imported unless asked): 1-2 per cue, every one named below.
- **9 music placeholder files** (not imported): stingers and loops that can stand in until the music purchase.
- Variations: 2-4 files wherever a sound repeats a lot (melee, shots, cannon fire and impact, hovers and clicks, Kraken hits).
- Style: bright low-poly pirate, tense not horror. The default day sea uses the calm afternoon sea ambience, not the "Ghostship Creepy" ones. The only ghostship file picked is the quiet Fear status bed.

### Coverage by category (catalogue cues only; suggested extras are listed but not counted)

| Category | Cues | Covered | Partial | Missing |
|---|---|---|---|---|
| Combat (melee, ranged, death, heal) | 4 | 4 | 0 | 0 |
| Cannon (fire, impact) | 2 | 2 | 0 | 0 |
| Ship (sink) | 1 | 1 | 0 | 0 |
| Status (fear, frozen, rooted, guarded, cowering) | 5 | 1 | 4 | 0 |
| Abilities (smoke and fire, six pistols, rally cry, charge, grapple, eye of the tiger) | 6 | 5 | 1 | 0 |
| Kraken (slam, swipe, grab, arms of fury, eyes of the sea, enrage) | 6 | 0 | 6 | 0 |
| UI (hover, click, confirm, cancel, purchase, error, turn start) | 7 | 6 | 1 | 0 |
| Ambience (day sea, storm) | 2 | 2 | 0 | 0 |
| **Total** | **33** | **21** | **12** | **0** |

"Partial" means a usable one-shot exists but the effect is layered from near matches, or the pack has no loop for a looping status. Real gaps (nothing in the pack): a **status loop** for Fear (only a creepy ghostship bed), Frozen, Guarded, Cowering and the enraged Kraken; a **wet tentacle whip/slam/constrict** sound; a **clear short UI refusal tone**; an **eerie glow swell** for Eyes of the Sea. Suggested ways to fill them are in each Notes cell. Combat music with a real loop is also missing (see Music).

### How to read the tables

- Filenames are the CSV `Filename` column. When a pick has several files, the name shown is the first variant; the trailing ` NN_ESM_PG.wav` number changes per variant and the listed variants are all picked.
- Asset names are `SW_<Name>_NN` under `Content/DeadwaterRun/Audio/SFX/<Category>/`. The full machine-readable list (every file, class, concurrency, loop, compression) is `Tools/Content/audio_pick_manifest.csv`; the import script reads that file.
- All files are stereo. Stereo sounds are not spatialised by attenuation in UE, so cues will play as 2D (see Needs from DW-114).
- Sound class: everything except UI is `SC_SFX`; UI is `SC_UI`. Concurrency: `SCON_Hits` melee, ranged, death, Six Pistols and the Kraken hit moves; `SCON_Cannon` cannon fire, impact and miss; `SCON_UI` all UI; none for the rest.

## Picks

### Combat

| Cue | Cover | Pick (asset, count, file) | Alternates | Dur | Ch | Loop | Notes |
|---|---|---|---|---|---|---|---|
| `GameplayCue.Hit.Melee` | Covered | **SW_MeleeHit** x3: `WEAPSwrd_Weapons Melee Sword Iron Slash Hit Body Cloth Heavy Impact Gritty Cut Metal Smack 01_ESM_PG.wav (variants 01,02,03)`<br>**SW_MeleePain** x3: `VOXReac_Voice Pirate Pain Male Hit Generic Medium Injury Grunt 01_ESM_PG.wav (variants 01,02,03)` | `WEAPSwrd_Weapons Melee Sword Iron Sharp Slash Cut Mid Gritty Grind Scrape Strike Impact 01_ESM_PG.wav (variants 01,02)`<br>`WEAPSwrd_Weapons Melee Sword Blade Parry Strike Metal Ring Impact Heavy Layered Clang Hit 01_ESM_PG.wav` | 0.5-0.78 s | stereo | no | Body-hit sword plus a pain grunt layer (map plays both). |
| `GameplayCue.Hit.Ranged` | Covered | **SW_RangedShot** x4: `GUNAntq_Weapons Flintlock Pistol Gun Shot Near Heavy Firearm Loose Metal Blast 01_ESM_PG.wav (variants 01,02,03,04)`<br>**SW_RangedImpact** x2: `BLLTRico_Weapons Flintlock Bullet Impact Ricochet Buzz Wood Thud Whir Crunch 01_ESM_PG.wav (variants 01,02)` | `GUNAntq_Weapons Flintlock Gun Pistol Small Caliber Blast Mechanism Near Dry Quick Shot 01_ESM_PG.wav (variants 01,02)`<br>`BLLTRico_Weapons Flintlock Bullet Impact Ricochet Buzz Metal Smack Whir Ding 01_ESM_PG.wav` | 0.22-1.93 s | stereo | no | Pistol shot plus wood ricochet impact. Six Pistols plays this once per hit, so keep the pitch range wide. |
| `GameplayCue.Unit.Death` | Covered | **SW_DeathVoice** x3: `VOXReac_Voice Pirate Pain Male Death Groan Strianed Long Strained Yell Groan 01_ESM_PG.wav (variants 01,02,03)`<br>**SW_DeathVoiceShort** x1: `VOXReac_Voice Pirate Pain Male Death Groan Cough Short Coughed Grunt 01_ESM_PG.wav`<br>**SW_DeathFall** x2: `FGHTBf_Weapons Bodyfall Gore Dry Drop Hit Thump Cloth Limb Hit Thud 01_ESM_PG.wav (variants 01,02)` | `VOXReac_Voice Pirate Pain Male Death Groan Strianed Long Breathy Groan Fade 01_ESM_PG.wav` | 0.24-2.53 s | stereo | no | Voice plus dry body thud. Picked to avoid wet gore (tone: bright, not horror). |
| `GameplayCue.Unit.Heal` | Covered | **SW_Heal** x3: `MAGShim_Magic Collect Orb Power Up Boost Coin Hybrid Hit Kicked Bass Reverb Reverse 01_ESM_PG.wav (variants 01,02,03)` | `MAGShim_Magic Collect Jug Loot Potion Liquid Juice Power Up Pitching Ceramic Water Splash 01_ESM_PG.wav (variants 01,02)`<br>`MAGShim_Magic Item Award Hit Bass Chime Ding Ring Positive Synth Tap Reverse Shimmer 01_ESM_PG.wav` | 1.62-1.76 s | stereo | no | 1.6-1.8 s; the map should fade the tail. |

### Cannon

| Cue | Cover | Pick (asset, count, file) | Alternates | Dur | Ch | Loop | Notes |
|---|---|---|---|---|---|---|---|
| `GameplayCue.Cannon.Fire` | Covered | **SW_CannonFire** x4: `GUNCano_Weapons Cannon Shot Boom Near Bright Blast Explosion Short Tail 01_ESM_PG.wav (variants 01,02,03,04)` | `GUNCano_Weapons Cannon Explosion Large Blast Low Muffled Metal Rattle Recoil 01_ESM_PG.wav (variants 01,02)`<br>`GUNCano_Weapons Cannon Shot Boom Long Outdoor Short Fire Long Natural Tail 01_ESM_PG.wav` | 0.94-1.65 s | stereo | no | Near, bright, short tail (1-1.6 s) so volleys stay readable. |
| `GameplayCue.Cannon.Impact` | Covered | **SW_CannonImpact** x4: `WOODCrsh_Weapons Cannon Ship Impact Explosion Wood Break Hull Damage Scatter 01_ESM_PG.wav (variants 01,02,03,04)` | `WOODCrsh_Ship Foley Ramming Massive Damage Impact Wood Break Crash Crunch Splinter 01_ESM_PG.wav (variants 01,02)` | 0.92-1.23 s | stereo | no | Hull break and splinter scatter. |
| `GameplayCue.Attack.Miss` (added 2026-11-20; cannons never miss, so it is the splash of any tile-aimed miss - the Kraken's Slam/Grab) | Covered | **SW_CannonMiss** x2: `WATRImpt_Water Splash Weapon Cannon Hit Big Impact Dunk Bass Disturbance 01_ESM_PG.wav (variants 01,02)` | `WATRSplsh_Source Water Splash Drop Heavy Impact Solid Cannonball Ripple Splash Drop Fall 01_ESM_PG.wav` | 2.24-2.45 s | stereo | no | Not in the catalogue; a missed shot needs a splash. Needs a cue tag (DW-114 / gameplay). |

### Ship

| Cue | Cover | Pick (asset, count, file) | Alternates | Dur | Ch | Loop | Notes |
|---|---|---|---|---|---|---|---|
| `GameplayCue.Ship.Sink` | Covered | **SW_SinkCrash** x2: `DESTRCrsh_Ship Foley Shipwrecking Cannon Explosion Heavy Metal Wood Debris Scatter 01_ESM_PG.wav (variants 01,02)`<br>**SW_SinkCreak** x2: `WOODFric_Ship Foley Wood Creak Hull Depth Deep Groan Crackle Stressed Lean Friction 01_ESM_PG.wav (variants 01,02)`<br>**SW_SinkSplash** x1: `WATRImpt_Water Splash Weapon Cannon Hit Big Impact Near Low Air Projectile Whoosh 01_ESM_PG.wav` | `DESTRCrsh_Ship Foley Shipwrecking Moving Island Hit Wood Impact Boom Creak Crunch 01_ESM_PG.wav` | 2.82-5.69 s | stereo | no | Three layers fired together: crash, hull groan, big splash. |

### Status

| Cue | Cover | Pick (asset, count, file) | Alternates | Dur | Ch | Loop | Notes |
|---|---|---|---|---|---|---|---|
| `GameplayCue.Status.Fear` | Partial | **SW_FearStart** x2: `VOXReac_Voice Pirate Pain Male Hit High Surprised Groan Heavy Injury 01_ESM_PG.wav (variants 01,02)`<br>**SW_FearLoop** x1: `AMBDsgn_Ambience Loop Ghostship Ghostly Moans Whispers Creepy Voice Low Rumble Whistle 01_ESM_PG.wav` | `UIAlert_Notification Friction Scary Tonal Moan Gritty Grind Pulsed Dark Tension 01_ESM_PG.wav` | 0.65-38.07 s | stereo | yes: SW_FearLoop | Start is a startled yelp. No clean fear loop in the pack; the only fit is the ghostship moans bed (creepy, so quiet and status-only). Gap: a short bright dread pulse loop. |
| `GameplayCue.Status.Frozen` | Partial | **SW_FrozenStart** x2: `MAGShim_Magic Glass Impact Break Shatter Tonal Hum Open Ice Mid Wind Swoop 01_ESM_PG.wav (variants 01,02)`<br>**SW_FrozenEnd** x1: `MAGShim_Magic Ice Impact Crystal Glass Debris Crate Smash Break Crunch Item Hit 01_ESM_PG.wav` | `MAGShim_Magic Ice Impact Crystal Glass Debris Crate Smash Break Crunch Item Double Hit 01_ESM_PG.wav` | 0.79-2.78 s | stereo | no | Start and shatter are good. No ice loop in the pack: the start tail carries the hum, no loop. |
| `GameplayCue.Status.Rooted` | Covered | **SW_RootedStart** x2: `CHAINMvmt_Ship Foley Moving Hull Chains Clank Clink Slide Drag Jingle Metal 01_ESM_PG.wav (variants 01,02)`<br>**SW_RootedEnd** x1: `CHAINMvmt_Ship Foley Moving Hull Chains Clink Handling Jingle Movement Drop 01_ESM_PG.wav` | `WEAPPole_Weapons Boat Gun Harpoon Spear Projectile Hit Grab Wood Plank Impact Clatter 03_ESM_PG.wav` | 1.76-3.13 s | stereo | no | Chain clank on, chain drop off. |
| `GameplayCue.Status.Guarded` | Partial | **SW_GuardedStart** x2: `WEAPSwrd_Weapons Melee Sword Blade Parry Strike Metal Long Bright Ring Resonant Impact Light 01_ESM_PG.wav (variants 01,02)`<br>**SW_GuardedEnd** x1: `WEAPSwrd_Weapons Melee Sword Sheathe Metal Neutral Smooth Slide Friction Put Away 01_ESM_PG.wav` | `MAGShim_Magic Hit Affirm Confirm Bass Rumble Chime Metal Debris Resonance 01_ESM_PG.wav` | 0.29-0.37 s | stereo | no | Bright sword ring on, sheathe off. No shield loop in the pack; ring tail only. Gap: soft shield hum loop. |
| `GameplayCue.Status.Cowering` | Partial | **SW_CoweringStart** x2: `VOXReac_Voice Pirate Pain Male Hit Blubbering Heavy Damage Injury 01_ESM_PG.wav (variants 01,02)` | `VOXReac_Voice Pirate Pain Male Hit Coughed Gut Shot Grunt 01_ESM_PG.wav` | 0.47-0.63 s | stereo | no | Whimper on start only; no loop. |

### Abilities

| Cue | Cover | Pick (asset, count, file) | Alternates | Dur | Ch | Loop | Notes |
|---|---|---|---|---|---|---|---|
| `GameplayCue.Ability.SmokeAndFire` | Covered | **SW_SmokeFireBurst** x3: `FIREWhsh_Weapons Explosion Molotov Glass Bottle Break Large Flames Fire 01_ESM_PG.wav (variants 01,02,03)`<br>**SW_SmokeFireFuse** x1: `FIRESizz_Source Black Powder Fuse Ignite Short Fast Fire Burn Burst Quick Bright 01_ESM_PG.wav` | `FIRESizz_Source Black Powder Fuse Ignite Steady Burn Bursts Flare Sizzle Dynamite Bomb 01_ESM_PG.wav` | 1.16-2.14 s | stereo | no | Burst of flame plus a fuse flare. |
| `GameplayCue.Ability.SixPistols` | Covered | **SW_SixPistolsCock** x1: `GUNMech_Weapons Flintlock Pistol Pirate Gun Recharge Load Cock Hammer Gunflint 01_ESM_PG.wav` | `GUNMech_Weapons Flintlock Pistol Pirate Gun Recharge Load Cock Hammer Gunflint 02_ESM_PG.wav` | 1.83 s | stereo | no | One cock-the-hammer flourish at the start; each of the six hits plays Hit.Ranged. |
| `GameplayCue.Ability.RallyCry` | Covered | **SW_RallyCryHigh** x1: `VOXMale_Voice Pirate Bandit Custom Lines Male Yell Crew All Aboard High 01_ESM_PG.wav`<br>**SW_RallyCryMid** x1: `VOXMale_Voice Pirate Bandit Custom Lines Male Yell Crew All Aboard Mid 01_ESM_PG.wav`<br>**SW_RallyCryLow** x1: `VOXMale_Voice Pirate Bandit Custom Lines Male Yell Crew All Aboard Low 01_ESM_PG.wav` | `VOXMale_Voice Pirate Young Custom Lines Male Man The Cannons 01_ESM_PG.wav` | 1.62-2.83 s | stereo | no | Crew shout (three pitches, each its own file). |
| `GameplayCue.Ability.Charge` | Covered | **SW_ChargeShout** x2: `VOXEfrt_Voice Pirate Fighting Male Attack Low Gritty Heavy Swing 01_ESM_PG.wav (variants 01,02)`<br>**SW_ChargeWhoosh** x1: `WEAPSwrd_Weapons Melee Sword Iron Combat Whoosh Swish Metal Slice Thick Low Mid Cut 01_ESM_PG.wav` | `VOXEfrt_Voice Pirate Fighting Male Attack Strained Throaty Swing Hit 01_ESM_PG.wav` | 0.43-0.54 s | stereo | no | War cry plus blade whoosh. |
| `GameplayCue.Ability.Grapple` | Covered | **SW_GrappleFire** x2: `WEAPMisc_Weapons Boat Gun Handheld Grappling Hook Fire Rope Ratchet Spin 01_ESM_PG.wav (variants 01,02)`<br>**SW_GrappleHit** x2: `WEAPPole_Weapons Boat Gun Harpoon Spear Projectile Hit Grab Wood Plank Impact Clatter 01_ESM_PG.wav (variants 01,02)` | `WEAPPole_Weapons Boat Gun Harpoon Spear Projectile Shoot Wood Trigger Rope Ratchet 01_ESM_PG.wav` | 0.93-1.2 s | stereo | no | Hook launch plus a wood grab on landing. |
| `GameplayCue.Ability.EyeOfTheTiger` | Partial | **SW_TigerGrowl** x2: `CREABeast_Creature Land Beast Bear Lion Deep Vicious Monster Attack 01_ESM_PG.wav (variants 01,02)`<br>**SW_TigerShine** x1: `MAGShim_Magic Airy Light Whoosh Click Button Soft Tap Multi Swell Bright Shine 01_ESM_PG.wav` | `CREABeast_Creature Land Beast Bear Lion Deep Vicious Monster Idle 01_ESM_PG.wav` | 1.26-2.96 s | stereo | no | Big-cat growl plus a shine. Ability intent assumed to be a focus/buff; the growl is bear/lion, there is no tiger in the pack. |

### Kraken

| Cue | Cover | Pick (asset, count, file) | Alternates | Dur | Ch | Loop | Notes |
|---|---|---|---|---|---|---|---|
| `GameplayCue.Kraken.TentacleSlam` | Partial | **SW_SlamImpact** x1: `WOODImpt_Ship Foley Impact Deep Ship Hull Clank Hit Wood Metal Bassy Thunk 01_ESM_PG.wav`<br>**SW_SlamSplash** x2: `WATRImpt_Water Splash Weapon Cannon Hit Big Impact Dunk Bass Disturbance 03_ESM_PG.wav (variants 03,04)`<br>**SW_SlamVocal** x2: `CREAAqua_Creature Deep Sea Kraken Slow Large Gutteral Sluggish Attack Short 01_ESM_PG.wav (variants 01,02)` | `CREAAqua_Creature Sea Monsters Leviathan Whoosh Serpent Giant Attack Tail Whip Thud 01_ESM_PG.wav` | 0.8-2.98 s | stereo | no | No clean tentacle slam. Layer: hull thunk + big splash + short Kraken grunt. Gap: wet tentacle whip-crack. |
| `GameplayCue.Kraken.KrankySwipe` | Partial | **SW_SwipeWhip** x2: `CREAAqua_Creature Sea Monsters Leviathan Whoosh Serpent Giant Attack Tail Whip Thud 01_ESM_PG.wav (variants 01,02)`<br>**SW_SwipeVocal** x1: `CREAAqua_Creature Deep Sea Kraken Slow Large Gutteral Sluggish Attack Heavy 01_ESM_PG.wav` | `CREAAqua_Creature Sea Monsters Kraken Roar Octopus Tentacles Attack Whoosh 01_ESM_PG.wav` | 1.64-4.68 s | stereo | no | Serpent tail whip (leviathan) as the sweep, Kraken grunt on top. Gap: tentacle-specific wide sweep. |
| `GameplayCue.Kraken.TentacleGrab` | Partial | **SW_GrabGrowl** x2: `CREAAqua_Creature Sea Monsters Kraken Gutteral Hiss Octopus Tentacles Grumble 01_ESM_PG.wav (variants 01,02)`<br>**SW_GrabDrag** x1: `CHAINMvmt_Ship Foley Moving Hull Chains Rattle Jingle Dragging Across Wood 01_ESM_PG.wav` | `CREAAqua_Creature Sea Monsters Leviathan Vocal Serpent Giant Deep Hollow Roar 01_ESM_PG.wav` | 2.13-4.49 s | stereo | no | Kraken hiss plus chains dragged over wood. Gap: wet squeeze/constrict. |
| `GameplayCue.Kraken.ArmsOfFury` | Partial | **SW_FuryHit** x3: `CREAAqua_Creature Deep Sea Kraken Slow Large Gutteral Sluggish Attack Short 03_ESM_PG.wav (variants 03,04,05)` | `CREAAqua_Creature Deep Sea Behemoth Large Growl Bassy Prime Attack 01_ESM_PG.wav` | 1.01-1.16 s | stereo | no | Three short Kraken hits, one cue per hit (3-5 hits per cast); pitch-vary in the map. Gap: rapid flurry whoosh. |
| `GameplayCue.Kraken.EyesOfTheSea` | Partial | **SW_EyesIdle** x2: `CREAAqua_Creature Deep Sea Kraken Slow Large Gutteral Sluggish Idle 01_ESM_PG.wav (variants 01,02)` | `CREAAqua_Creature Sea Monsters Kraken Gutteral Hiss Octopus Tentacles Grumble 03_ESM_PG.wav` | 1.56-1.73 s | stereo | no | Self buff: slow Kraken breath. Gap: an eerie glow swell (the pack magic swells are too long and bright). |
| `GameplayCue.Kraken.Enrage` | Partial | **SW_EnrageRoar** x2: `CREAAqua_Creature Sea Monsters Kraken Roar Octopus Tentacles Attack Whoosh 01_ESM_PG.wav (variants 01,02)` | `CREAAqua_Creature Sea Monsters Kraken Vocal Distressed Octopus Tentacles Roar 01_ESM_PG.wav`<br>`AMBUndwtr_Ambience Loop Underwater Shipwreck Creaks Whirring Low Reverb Rumble 01_ESM_PG.wav` [rumble bed] | 4.72-5.33 s | stereo | no | Roar is covered (4.7-5.3 s). The enraged looping status has no loop in the pack: schedule Kraken idle one-shots on a timer, or add a quiet underwater rumble bed. |
| `GameplayCue.Kraken.Appear / .Death / .TentacleCut` (added 2026-11-20) | Covered | **SW_KrakenAppear** x1: `CREAAqua_Creature Sea Monsters Kraken Wail Octopus Tentacles Water Appear 01_ESM_PG.wav`<br>**SW_KrakenDeath** x1: `CREAAqua_Creature Deep Sea Kraken Slow Large Gutteral Sluggish Death 01_ESM_PG.wav`<br>**SW_TentacleCut** x1: `CREAAqua_Creature Sea Monsters Kraken Vocal Distressed Octopus Tentacles Roar 01_ESM_PG.wav` | `CREAAqua_Creature Deep Sea Behemoth Large Growl Bassy Prime Death 01_ESM_PG.wav` | 2.04-6.36 s | stereo | no | Boss entrance, death and tentacle cut are not in the catalogue; recommend adding the tags. |

### UI

| Cue | Cover | Pick (asset, count, file) | Alternates | Dur | Ch | Loop | Notes |
|---|---|---|---|---|---|---|---|
| `UI.Sound.Hover` | Covered | **SW_UiHover** x3: `UIClick_UI Inventory Button Item Hover Wood Lever Pull Slide Friction 01_ESM_PG.wav (variants 01,02,03)` | `UIClick_UI Inventory Button Item Hover Metal Canisters Tap Bright Movement 01_ESM_PG.wav`<br>`UIClick_UI Inventory Button Item Hover Soil Hit Neutral Mid Tap Short 01_ESM_PG.wav` | 0.23-0.26 s | stereo | no | Wood lever scrape, 0.23-0.26 s. Keep quiet. |
| `UI.Sound.Click` | Covered | **SW_UiClick** x3: `UIClick_UI Inventory Button Click Tab Menu Proceed Latch Switch Flip Mid Press 01_ESM_PG.wav (variants 01,02,03)` | `UIClick_UI Button Metallic Single Tonal Tap Short Hit Smack 01_ESM_PG.wav` | 0.13-0.17 s | stereo | no | Latch click, 0.13-0.17 s. |
| `UI.Sound.Confirm` | Covered | **SW_UiConfirm** x2: `UIClick_UI Inventory Button Click Crate Drop Confirm Use Bass Impact Clatter Short 01_ESM_PG.wav (variants 01,02)` | `UIClick_UI Button Bass Stomp Hit Smack Affirm Layered Impact Metallic Twang 01_ESM_PG.wav` | 0.57-0.62 s | stereo | no | Crate-thump confirm, ~0.6 s. |
| `UI.Sound.Cancel` | Covered | **SW_UiCancel** x2: `UIClick_UI Inventory Button Click Close Heavy Door Slide Wood Bass Stomp 01_ESM_PG.wav (variants 01,02)` | `UIClick_UI Inventory Pirate Quest Book Close Wood Cover Mid Hit Short Wet 01_ESM_PG.wav` | 0.4-0.43 s | stereo | no | Heavy door slide close, ~0.4 s. |
| `UI.Sound.Purchase` | Covered | **SW_UiPurchase** x3: `UIClick_UI Notification Button Click Market Interact Item Buy Bag Drop Coins Jangle 01_ESM_PG.wav (variants 01,02,03)` | `UIClick_UI Coin Thump Punchy Collect Touch Tap Tab Click Bass Complex 01_ESM_PG.wav` | 0.78-1.22 s | stereo | no | Coin jangle, 0.8-1.2 s. |
| `UI.Sound.Error` | Partial | **SW_UiErrorTap** x1: `UIClick_UI Button Glass Fast Double Tap Negative Plop Layered Hit 01_ESM_PG.wav`<br>**SW_UiErrorThud** x2: `UIClick_UI Button Bouncy Bass Hit Click Fall Tumble Low Muffled Warm Punchy 01_ESM_PG.wav (variants 01,02)` | `UIAlert_Notification Inventory Trash Bin Empty Scrap Double Hit Thud Clamor Negative 01_ESM_PG.wav` | 0.1-0.79 s | stereo | no | The glass double-tap is only 0.1 s and the bouncy thud is organic rather than a buzzer; audition in context. Gap: a clear short refusal tone. |
| `UI.Sound.TurnStart` | Covered | **SW_UiTurnPlayer** x2: `UIAlert_UI Bell Ding Two Hit Solo Chime Clean High Pitch Ring Short 01_ESM_PG.wav (variants 01,02)`<br>**SW_UiTurnEnemy** x1: `UIAlert_Notification Shipfoley Shiphull Mast Sine Bell Warning Double Hit Metal Ring Quick 01_ESM_PG.wav` | `UIAlert_UI Metal Notification Soft Drum Bell Like Ding Mid Chime Single Hit Tonal 01_ESM_PG.wav` | 1.23-3.88 s | stereo | no | Two bells: clean ding for the player turn, ship-bell warning for the enemy turn (the tag needs a split or a parameter). |
| `UI.Sound.* extras (suggested: Sell, Gold, Reward, LevelUp, Page)` | Covered | **SW_UiSell** x1: `UIClick_UI Notification Button Click Market Interact Item Sell Hit Drop Coins Jangle 01_ESM_PG.wav`<br>**SW_UiGold** x2: `OBJCoin_Item Action Collect Loot Treasure Coins Jewelry Bright Jingle Ground Grab 01_ESM_PG.wav (variants 01,02)`<br>**SW_UiReward** x1: `MAGShim_Magic Chest Open Unlock Treasure Pick Up Success Chime Glisten 01_ESM_PG.wav`<br>**SW_UiLevelUp** x1: `MAGShim_Magic Award Collect Achievement Positive Treasure Gold Soft Kick Chime 01_ESM_PG.wav`<br>**SW_UiPageFlip** x1: `UIClick_UI Inventory Pirate Quest Book Next Page Flip Turn Swipe 01_ESM_PG.wav`<br>**SW_UiPageOpen** x1: `UIClick_UI Inventory Pirate Quest Book Open Layered Pages Friction Swipe 01_ESM_PG.wav` | `MAGShim_Magic Item Award Hit Bass Chime Ding Ring Positive Synth Tap Reverse Shimmer 02_ESM_PG.wav` | 0.6-4.16 s | stereo | no | For the shop, gold counter, rewards, level-up and map/book screens (UI Juice). |

### Ambience

| Cue | Cover | Pick (asset, count, file) | Alternates | Dur | Ch | Loop | Notes |
|---|---|---|---|---|---|---|---|
| `Ambience.Sea.Day (default sea)` | Covered | **SW_AmbSeaDay** x1: `AMBSea_Ambience Loop Island Sea Calm Winds Afternoon Water Stream Flow Wave Swell 01_ESM_PG.wav`<br>**SW_AmbSeaDayBirds** x1: `AMBSea_Ambience Loop Island Sea Waves Crash Light Wind Birds Chirp Sing Daytime 01_ESM_PG.wav`<br>**SW_AmbHullCreak** x1: `WOODFric_Ship Loop Wood Hull Creak Friction Movement Squeak Solo Spaced 01_ESM_PG.wav`<br>**SW_AmbWake** x1: `WATRFlow_Ship Loop Water Flow Sail Light Steady Waves Raft Canoe Travel 01_ESM_PG.wav` | `AMBSea_Ambience Loop Dock Water Boat Wood Creak Sea Wind Waves Blowing Birds 01_ESM_PG.wav` [port / shop]<br>`AMBSea_Ambience Loop Island Sea Prominent Waves Light Wind Daytime Near Water 01_ESM_PG.wav` | 16.45-64.43 s | stereo | yes | Calm afternoon sea (no ghostship ambiences). Bird variant for variety; hull creak and wake as quiet layers. |
| `Ambience.Sea.Storm` | Covered | **SW_AmbStorm** x1: `AMBSea_Ambience Loop Island Sea Storm Heavywaves Strongwinds Daytime Thunder Rumble 01_ESM_PG.wav`<br>**SW_AmbStormShip** x1: `AMBSea_Ambience Loop Ship Sailing Storm Eerie Steady Water Wood Creak Distant Thunder 01_ESM_PG.wav` | `AMBSea_Ambience Loop Island Sea Rain Waves Water Wash Crash Winds Daytime 01_ESM_PG.wav` [rain bed] | 33.11-38.18 s | stereo | yes | Storm may be darker. Stack the rain bed on top for the heaviest weather. |
| `Ambience.Tavern (optional, events)` | Covered | (not imported) | `AMBRest_Ambience Loop Tavern Drinkplace Drinks Cups Desk Glass Metal Clink Shuffle Taps 01_ESM_PG.wav` [tavern event] | 35.45 s | stereo | no | Optional bed for tavern events. Not imported by default. |



## Music (separate purchase; placeholders only, not imported)

The pack's `Music_Loops` folder is tavern folk, not combat music. These can stand in so the music state machine can be wired and tested before the real tracks arrive.

| Music state | File | Dur | Ch | Note |
|---|---|---|---|---|
| `Music.Victory` | `UIAlert_Notification Musical Stinger Fanfare Achievement Victory Celebratory Orchestral 01_ESM_PG.wav (variants 01,02)` | 8.41-8.42 s | stereo | 8.4 s stinger; plays as a one-shot placeholder. |
| `Music.Defeat` | `UIAlert_Notification Musical Stinger Fanfare Fail Defeat Unsuccessful Orchestral Tension 01_ESM_PG.wav (variants 01,02)` | 5.93-6 s | stereo | About 6 s stinger. |
| `Music.Boss (intro stinger)` | `UIAlert_Notification Musical Stinger Attack Ship Invasion Start Full Orchestral Percussion 01_ESM_PG.wav` | 9.4 s | stereo | 9-10.5 s. |
| `Music.Combat (intro stinger, no loop in pack)` | `UIAlert_Notification Musical Stinger Ship Combat Duel Start Fast Tension Full Strings Drums 01_ESM_PG.wav` | 11.87 s | stereo | 11.9 s; works as a combat-start sting over a drum stem loop. |
| `Music.Menu / Music.Map (loop)` | `MUSCLoop_Music Loop Song A Full Musical Stinger Tavern Exterior Song 01_ESM_PG.wav` | 41.98 s | stereo | 42 s loop, jaunty tavern folk. |
| `Music.Menu / Music.Map (loop)` | `MUSCLoop_Music Loop Song B Full Musical Stinger Tavern Exterior Song 02_ESM_PG.wav` | 27.43 s | stereo | 27 s loop. |
| `Music.Combat (drum stem)` | `MUSCLoop_Music Loop Song A Instrument Frame Drum Percussion Song 01_ESM_PG.wav` | 21 s | stereo | 21 s percussion stem; Song A stems (tabourine, rhythm) can be layered for tension. |


Suggested music wiring until the purchase lands: `Music.Menu` and `Music.Map` use the Song A tavern loop (Song B as the second map variant), `Music.Combat` uses the combat-start stinger over the drum stem, `Music.Victory` and `Music.Defeat` use the fanfares as one-shot stingers. There is no combat or boss loop in the pack.

## Risks and notes

- **Repo size:** 121 imported waves are about 82 MB of `.uasset` data in git (no LFS rules exist in `.gitattributes`). Say if you want LFS first, or a smaller list.
- **Licence:** Epic Stock Media EULA - see `Docs/Audio/Licences.md`. Only the sounds on this list are imported; the pack, its CSV and its README stay where they are. Imported assets record only the bare file name, not the local pack path (checked in the saved `.uasset`).
- **Loudness:** the pack is not loudness-matched across categories (stingers and designed hits are louder than foley). The first import gets uniform defaults; the DW-403 pass sets per-entry volume in the cue map.
- **Long tails:** Kraken roars, Kraken Appear and the Sink layers are 3-6 s. The cue map should use short fades on these.
- **Tag asks:** Brandon took `Kraken.Appear/Death/TentacleCut` and the miss splash (as `Attack.Miss`) on 2026-11-20 - native tags with sound-only GC_* notifies. Still open: TurnStart player/enemy variants and the extra `UI.Sound.*` tags (Sell, Gold, Reward, LevelUp, PageFlip, PageOpen) are suggestions, not catalogue entries.
- The `DW-005-Audio-Options.md` document named in the brief does not exist in the repo, so the scraper's gap notes were not available; the gaps above come from searching the pack's CSV directly.

## Approval and import

1. Brandon approves or edits this list (edit `Tools/Content/audio_pick_manifest.csv` rows, set `Import` to 0 or 1).
2. `./Tools/CreateAudioContent.ps1` (already run; creates classes, mixes, concurrency).
3. `./Tools/ImportAudio.ps1 -PackRoot '<...>\Pirate_Game_16bit\Pirate_Game_16bit\Pirate_Game_16bit'` (add `-IncludeAlternates` to also import the 49 alternates, `-Only <regex>` for a subset).
4. DW-311 fills `DA_AudioCueMap` once DW-114 has landed.

## Needs from DW-114 (gameplay-engineer)

- `UDwAudioCueMap` entries need: an array of sounds per cue tag (random pick among variants, no immediate repeat), volume, pitch range, concurrency override, loop flag, and optional layers (Ship.Sink and the Kraken moves fire 2-3 sounds together).
- Cue notify bases must play sounds as 2D by default (the sources are stereo), or the map needs an attenuation field.
- Looping status cues: start sound, optional loop sound, optional stop sound (Fear is the only status with a loop file).
- `UDwMusicSet` should accept a looping flag per state and one-shot stinger states (Victory, Defeat).
- Push `SM_Base` once at startup and `SM_Pause` while paused; apply user volumes with `SetSoundMixClassOverride` on `SM_Base` for each of the four classes.
