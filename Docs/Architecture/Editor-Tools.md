# Editor Tools and Authoring Workflows

Goal: Brandon can add and test content in the editor without code and without an AI agent. Everything here is owned by **tools-engineer** unless stated. Editor code lives in `Source/DeadwaterRunEditor/`; editor-only assets in `Content/DeadwaterRun/Dev/Tools/` (never cooked).

## Tool list

| # | Tool | What it does | Implemented by | Ticket |
|---|---|---|---|---|
| T1 | Deadwater menu | One "Deadwater" menu in the level editor toolbar with every tool below | `FDwEditorMenus` (`UToolMenus` extension) | DW-108 |
| T2 | Content Wizard | "New Ability / Item / Perk / Perk Pool / Encounter (enemy crew) / Unit / Captain / Event / Deck Layout / Ship Upgrade" - creates the asset from a template, names it, puts it in the right folder, adds a stub row to the right `Tuning/*.json` and reimports | `UDwContentWizardSubsystem : UEditorSubsystem` + `EUW_ContentWizard` | DW-213 |
| T3 | Data validation | Per-class `IsDataValid` (gameplay-engineer, on each definition class) + cross-asset rules (`UDwContentValidator : UEditorValidatorBase`): tuning row exists, ability class set, unique `AbilityTag`, pools non-empty, captain has exactly 2 abilities, event has valid choices, deck layout connected, cue tags have notifies, JSON and DataTable in sync, naming/folder conventions | `UDwContentValidator`, `Tools/Validate.ps1` | DW-108 |
| T4 | Tuning import / export | Reimports every `Tuning/*.json` into its DataTable with strict field checking; "Export JSON" writes a DataTable edited in the editor back to its JSON so JSON stays the source of truth; toolbar buttons + commandlet | `UDwImportTuningCommandlet`, `Tools/ImportTuning.ps1` | DW-109 |
| T5 | Deck layout editor | Paint tiles (deck, nest, ladder, cannon, obstacle, water, boarding edge, spawn, tentacle slot, Kraken anchor) on a grid in the details panel of a `UDwDeckLayout`; set blocker height and owner; ASCII import/export; live validation | `FDwDeckLayoutDetails : IDetailCustomization` (Slate grid widget `SDwDeckLayoutGrid`) | DW-110 |
| T6 | Deck alignment preview | Drop `BP_ShipPreview` (an `ADwBoard` in editor preview mode) in `L_Dev_ShipAlign`, assign a layout and ship class; the grid draws over the Synty mesh so `MeshAlignment` can be tuned visually | `ADwBoard::OnConstruction` preview (gameplay-engineer, DW-102) + dev map (tools-engineer) | DW-110 |
| T7 | Map preview and rule validator | Enter a seed (or random), see the generated map graph with node types, the validator result for every rule, and run a 10,000-seed sweep | `EUW_MapPreview` + `UDwMapPreviewLibrary` (BlueprintFunctionLibrary) + `UDwMapSweepCommandlet`, `Tools/MapSweep.ps1` | DW-213 |
| T8 | Play this encounter / event / shop | Right-click an encounter, event or the shop definition -> "Play in Deadwater" -> PIE in `L_Sea` straight into that content with a chosen captain, crew level, seed and gold | Content browser menu extension in `FDwEditorMenus` + `UDwDevSettings : UDeveloperSettings` (per-user config) read by `ADwGameMode` in PIE only | DW-111 |
| T9 | Cheats / console | `Dw.Win`, `Dw.Lose`, `Dw.Kill <unit>`, `Dw.SetHealth <unit> <hp>`, `Dw.AddFear <unit> <n>`, `Dw.Gold <n>`, `Dw.LevelUp <crew>`, `Dw.GotoNode <index>`, `Dw.GiveAbility <crew> <ability>`, `Dw.Instant 0/1`, `Dw.Seed` | `UDwCheatManager` (not created in Shipping) | DW-111 |
| T10 | Headless simulator | Plays thousands of seeded runs with the real systems | `UDwSimulateCommandlet` + `Sim/` runtime classes, `Tools/Simulate.ps1` | DW-212 |
| T11 | Balance dashboard | Pick runs/captains/seed/policy, click Run (spawns the simulator as a child process so the editor stays responsive), see win rate per captain, run length, death-node histogram, outcome mix, gold curve; compare two reports side by side | `EUW_BalanceDashboard` + `UDwSimReportLibrary` (parses `Saved/Sim/*.json`), `FMonitoredProcess` | DW-309 |
| T12 | Ability description preview | In an ability/perk/item details panel, shows the description with `{tokens}` filled from its tuning row, so text and numbers match | Detail customization `FDwDefinitionDescriptionDetails` | DW-213 |
| T13 | Combat replays | `Dw.SaveReplay` writes seed + command list (`FDwCombatRecord`) to `Saved/Replays/`; `-DwReplay=<file>` replays it in PIE or headless - QA attaches replays to bug reports | `FDwCombatRecord` (gameplay-engineer, DW-103) + replay runner (tools-engineer) | DW-407 |
| T14 | GAS debugging | Built-in: `showdebug abilitysystem`, `AbilitySystem.Debug.NextTarget`, Gameplay Tags editor; plus `LogDwCombat` verbose combat log | Engine + gameplay-engineer logging | - |

## How Brandon adds X

### A new crew ability
1. Deadwater menu -> Content Wizard -> **New Ability**. Name it (e.g. `PowderKeg`). The wizard creates `DA_Ability_PowderKeg`, `GA_PowderKeg` (Blueprint subclass of `UDwGA_EffectPayload`) and a `PowderKeg` row in `Tuning/AbilityTuning.json`, then reimports.
2. Open `GA_PowderKeg` (class defaults only, no graph): set Targeting (pattern, side, LOS), Applications (e.g. `GE_Damage`, then `GE_AddFear`), montage, cue tag.
3. Open `DA_Ability_PowderKeg`: name, description with tokens (`Deals {Damage} to all enemies within {Range}`), icon, `AbilityTag` (create `Ability.Crew.PowderKeg` in the tag picker), weapon tags.
4. Set numbers in `Tuning/AbilityTuning.json` (or ask the Balance Designer) and press **Reimport Tuning**.
5. Add it to a pool: open `DA_AbilityPool_Swordsman` and add the entry (it can now appear in shops and loot).
6. Right-click any encounter -> **Play in Deadwater**, `Dw.GiveAbility 1 PowderKeg`, try it.
7. Save -> validation runs; fix anything red.
- Needs code only if it moves units in a new way or needs a new rule (then it's a gameplay-engineer ticket).

### A new item
1. Content Wizard -> **New Item** -> `DA_Item_<Name>` + row in `ItemTuning.json`.
2. Set the effect GE (e.g. `GE_Heal`), targeting (self or adjacent ally), icon, text; set `Price` and `Amount` in JSON; reimport.
3. Add it to `DA_Shop_Default` items and, if desired, the plunder item list.

### A new perk
1. Content Wizard -> **New Perk** -> `DA_Perk_<Name>` + `GE_Perk_<Name>` (infinite GE, asset tag `Effect.Perk`) + row in `PerkTuning.json`.
2. In the GE add modifiers (e.g. `MaxHealth` + SetByCaller `Data.Value`), or set `GrantedAbility` on the perk.
3. Add the perk to a `DA_PerkPool_*`. Set `RequiredUnitTags` if it's archetype-only.

### A new enemy crew (encounter)
1. Content Wizard -> **New Encounter** -> `DA_Encounter_<Name>` + rows in `EncounterTuning.json` and `ShipTuning.json`.
2. Pick or create a deck layout (see below) and ship Blueprint.
3. Add enemy slots: unit definition (existing pirates or a new unit via **New Unit**), level, optional AI profile.
4. Pick an AI profile (`DA_AiProfile_*`) or duplicate one and change weights in `AiTuning.json`.
5. Set `MinLayer`/`MaxLayer` and `Weight` so it appears at the right depth.
6. Right-click -> **Play in Deadwater**.

### A new event
1. Content Wizard -> **New Event** -> `DA_Event_<Name>`.
2. Write `Title` and `Body`, pick `Valence` (Good/Bad), weight, layer range, illustration.
3. Add 1-3 choices: label, result text; in Outcomes click **+** and pick the outcome type from the dropdown (Gold, Ship Health, Crew Health, Give Item, Ability Choice, Grant XP, Recruit, Lose Crew, Random Of) and set its numbers; add requirements if needed (Min Gold, Has Item...).
4. Right-click -> **Play in Deadwater** opens the event screen directly.
- Format reference for writing in a doc first:
```
Title:    <one line>
Valence:  Good | Bad        Layers: 1-9        Weight: 1
Body:     <2-4 sentences>
Choice 1: <label>  -> Outcomes: Gold +60; CrewHealth -10 (Random)  -> Result: <one line>
Choice 2: <label>  -> Requires: MinGold 50 -> Outcomes: GiveItem HealthRestore x1 -> Result: <one line>
```

### A new captain (post-launch; extra captains are cut from v1.0)
1. Content Wizard -> **New Captain** -> `DA_Captain_<Name>` + unit + `CaptainTuning.json` row.
2. Pick the captain unit, starting crew, exactly 2 abilities, rule tags from the existing `Rule.Captain.*` set, captain/crew combat GEs.
- A rule that isn't in the existing set needs a gameplay-engineer ticket (`FDwCaptainRules` hook).

### A new deck layout (ship)
1. Content Wizard -> **New Deck Layout** -> `DA_Deck_<Name>`; set the size.
2. Paint tiles in the details grid (palette on the left; shift-drag to fill); mark the boarding edge, cannon tiles, nest + ladder, spawn tiles; set owner side.
3. Watch the validation strip under the grid (connected, spawns, nest reachable, boarding edge).
4. Open `L_Dev_ShipAlign`, assign the layout and ship mesh to `BP_ShipPreview`, adjust `MeshAlignment` until tiles sit on the deck.
- ASCII export/import buttons let a layout be pasted into a ticket or doc.

### A new ship upgrade
1. Content Wizard -> **New Ship Upgrade** -> `DA_ShipUpgrade_<Name>` + `GE_ShipUpgrade_<Name>` + price row.
2. Set modifiers on the GE (e.g. `MaxHealth +50`) or a granted ship tag; add it to `DA_Shop_Default` upgrades.

### A new Kraken move
1. Content Wizard -> **New Ability** with template "Kraken move" (`UDwGA_EffectPayload`, enemy side).
2. Add it to a phase's move list in `DA_Kraken` with a weight.

### A VFX/sound for a new ability
1. Add a cue tag (`GameplayCue.Ability.<Name>`) and set it on the ability or GE.
2. gameplay-engineer (or Brandon) creates `GC_Ability_<Name>` from the `UDwGameplayCueNotify_Burst` template with a Synty Niagara system.
3. sound-wrangler (or Brandon) adds the tag -> sound entry in `DA_AudioCueMap`.

### Rebalance something
1. Edit the number in `Tuning/*.json` (or the DataTable view, then **Export JSON** from the Deadwater menu to keep JSON as the source of truth).
2. Deadwater menu -> **Balance Dashboard** -> Run 1,000 runs -> compare with the last report.

## Build order (tools-engineer)
- Week 1: T1, T3 (framework), T4, T5, T6 dev map, T8, T9.
- Week 2: T2, T7, T10, T12.
- Week 3: T11.
- Week 4: T13 and packaged autoplay.

## How it's tested
- Each tool has a command-line path or an automation test: validator tests on known-bad assets in `Dev/TestData/`; import tests; wizard test creates and deletes an asset of every type; map sweep exit code; simulator determinism.
- Each tool PR includes a short clip of Brandon's workflow ("how Brandon adds X" steps) done end to end.
