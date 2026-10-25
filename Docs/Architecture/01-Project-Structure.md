# 01 - Project Structure, Modules and Template Cleanup

## Problem
- `Source/` is UE template code (top-down character, Strategy and TwinStick variants). None of it fits a grid-based, turn-based game.
- Agents work in parallel worktrees. Without a fixed folder and module layout they collide or build private copies.
- Editor-only tools must never ship in the runtime module.

## Decision

| Option | Pros | Cons | Verdict |
|---|---|---|---|
| A. One runtime module only | Simplest | Editor tools would leak into the game or need `WITH_EDITOR` everywhere | Rejected |
| **B. Runtime `DeadwaterRun` + editor `DeadwaterRunEditor`** | Epic's standard split, tools stay out of Shipping, one place for game code | Pure-rule separation relies on folder discipline | **Chosen** |
| C. `DeadwaterRunCore` (pure rules) + game + editor | Compiler-enforced purity | Export macros, extra Build.cs, slower iteration; no payoff in 6 weeks | Rejected |

- Pure-rule purity is enforced by review: files in `*/Rules/` and pure structs include only `CoreMinimal`, `GameplayTags` and other pure headers, never `GameFramework/*`, `Engine/World.h` or actor headers.

## Module dependencies

| Module | Public deps | Private deps |
|---|---|---|
| `DeadwaterRun` | `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `GameplayAbilities`, `GameplayTags`, `GameplayTasks`, `UMG`, `CommonUI`, `CommonInput` | `Slate`, `SlateCore`, `Niagara`, `DeveloperSettings`, `Json`, `JsonUtilities` |
| `DeadwaterRunEditor` | `Core`, `CoreUObject`, `Engine`, `DeadwaterRun` | `UnrealEd`, `Slate`, `SlateCore`, `PropertyEditor`, `ToolMenus`, `DataValidation`, `AssetTools`, `ContentBrowser`, `Blutility`, `UMGEditor`, `EditorScriptingUtilities`, `GameplayAbilities`, `GameplayTags`, `Json`, `JsonUtilities` |

- Remove from `DeadwaterRun.Build.cs`: `AIModule`, `NavigationSystem`, `StateTreeModule`, `GameplayStateTreeModule` (no NavMesh, no AIController, no StateTree - see `07-Enemy-AI.md`), and every `Variant_*` public include path.
- `.uproject`: add `DeadwaterRunEditor` (Type `Editor`, LoadingPhase `PostEngineInit`); remove the `AdditionalDependencies` list; enable `CommonUI`, `DataValidation`, `EditorScriptingUtilities`; disable `StateTree` and `GameplayStateTree`; set `TargetAllowList: ["Editor"]` on `ModelContextProtocol`, `AllToolsets`, `Terminal` and `ModelingToolsEditorMode` so they never ship (release-engineer verifies in the first Shipping build).
- `DeadwaterRunEditor.Target.cs`: `ExtraModuleNames.AddRange({"DeadwaterRun", "DeadwaterRunEditor"})`.

## Folder layout

```
Source/DeadwaterRun/
  Core/          tags, logs, shared types, asset manager, game data, game modes, player controller
  Abilities/     ASC, attribute sets, abilities, execs, GE components, target data, Tasks/
  Board/         grid model, layouts, pathfinding, LOS, board builder, ADwBoard
  Combat/        combat subsystem, turn manager, commands, snapshot, outcome, units, ship, interaction
  Run/           run state, UDwRun, run subsystem, Map/, Economy/, Events/, progression, captain rules
  Data/          definition DataAsset classes, Tuning/ row structs
  AI/            side controller, brains, profiles, scoring          (ai-engineer)
  UI/            view models, widget bases, UI manager               (ui-engineer)
  Presentation/  cue notify bases, presentation subsystem, music, camera
  Shell/         game user settings
  Sim/           headless run driver and policies                    (tools-engineer)
  Dev/           dev settings, cheat manager                         (tools-engineer)
  Tests/         harness + tests, one subfolder per system           (tools-engineer harness)
Source/DeadwaterRunEditor/                                          (tools-engineer)
Content/DeadwaterRun/
  Data/          Captains/ Units/ Abilities/ Items/ Perks/ Encounters/ Events/ Boards/ Kraken/ Ship/ Rules/ Tuning/ AI/
  Abilities/     GA_* and GE_* Blueprint subclasses
  Characters/    unit Blueprints (BP_Unit_*), animation
  Ships/         BP_Ship_*, ship meshes setup
  VFX/           Niagara (NS_*), cue notifies (GC_*)
  Audio/         sounds, MetaSounds, classes, mixes, DA_AudioCueMap, DA_MusicSet   (sound-wrangler)
  UI/            WBP_*, fonts, textures, DA_UISoundMap                            (ui-engineer, ui-juice-engineer)
  Maps/          L_MainMenu, L_Sea
  Dev/           test maps, tool EUWs, templates - never cooked                    (tools-engineer)
Tuning/          *.json tuning sources (repo root, outside Content)                (balance-designer)
Tools/           Build.ps1, Test.ps1, Validate.ps1, Simulate.ps1, ImportTuning.ps1  (tools-engineer)
Build/           packaging and Steam scripts                                       (release-engineer)
```

## Naming

| Kind | Prefix | Example |
|---|---|---|
| C++ types | `Dw` + Epic prefix | `UDwCombatSubsystem`, `FDwGridModel`, `ADwShip`, `EDwSide`, `IDwCombatSideController` |
| Definition DataAsset | `DA_<Type>_<Name>` | `DA_Ability_RallyCry`, `DA_Captain_TeamLeader` |
| DataTable | `DT_<Name>Tuning` | `DT_UnitTuning` |
| Ability / effect / cue Blueprint | `GA_`, `GE_`, `GC_` | `GA_SixPistols`, `GE_Damage`, `GC_Hit_Melee` |
| Widgets, editor widgets | `WBP_`, `EUW_` | `WBP_CombatHud`, `EUW_BalanceDashboard` |
| Niagara, sounds | `NS_`, `SFX_`, `MS_` (MetaSound), `MUS_` | `NS_CannonImpact`, `MS_CannonFire` |
| Levels | `L_` | `L_Sea` |
| Tags | dotted, namespaced | `Status.Incapacitated.Frozen` (see `02-GAS-Foundation.md`) |

- Epic coding standard: `TObjectPtr` for UPROPERTY object pointers, no raw `new`, `check`/`ensure` for invariants, `UE_LOG(LogDw*, ...)`, `const` correctness, no `using namespace` in headers.
- Every UPROPERTY that holds tuning is `EditDefaultsOnly` (definitions) or comes from a DataTable row; nothing tunable is `EditAnywhere` on placed actors.

## Template cleanup (gameplay-engineer, ticket DW-002)

| Item | Action | Why |
|---|---|---|
| `Source/DeadwaterRun/Variant_Strategy/**`, `Variant_TwinStick/**` | Delete | Template samples, not our game |
| `DeadwaterRunCharacter.h/.cpp` | Delete | We use `ADwCombatUnit` (an actor with an ASC; no CharacterMovement) |
| `DeadwaterRunGameMode.h/.cpp` | Delete, replace with `Core/DwGameMode` | Naming and responsibilities change |
| `DeadwaterRunPlayerController.h/.cpp` | Delete, replace with `Core/DwPlayerController` | NavMesh click-to-move does not apply. **Ask Brandon first**: the 2026-10-23 status notes a WIP "first-move message" of his in this file |
| `DeadwaterRun.h/.cpp` | Keep (module definition) | |
| `Content/Variant_Strategy`, `Content/Variant_TwinStick`, `Content/TopDown`, `Content/Cursor` | Delete | Template content |
| `Content/Splash` | Keep | Project splash |
| `Config/DefaultInput.ini` template mappings | Remove; inputs live in Enhanced Input assets | |
| `Config/DefaultEngine.ini` default maps and game mode | Point to `L_MainMenu` and `BP_DwMenuGameMode` | |

- Remove any `ActiveClassRedirects` that point at template classes. Build `DeadwaterRunEditor` after the delete and confirm zero warnings and that `GameplayAbilities` loads (Week 0 exit criterion).

## How it's tested
- `Tools/Build.ps1` builds `DeadwaterRunEditor` (Development, Win64) with zero new warnings.
- `Tools/Test.ps1 -Filter DeadwaterRun` runs one placeholder pure test (DW-003) so the harness is proven from day one.
- release-engineer's first Shipping package (DW-006) proves the MCP plugins and the editor module are excluded.

## What the player sees
- Nothing yet. A clean tree means every later feature lands in the right place and the build stays fast.
