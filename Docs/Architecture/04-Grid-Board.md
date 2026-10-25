# 04 - Grid Board, Movement and Line of Sight

## Problem
- Fixed decision: square tile grids. Each ship deck is a grid; a boarding seam joins the two decks; the sniper's nest is an elevated tile that matters for line of sight and range. The Kraken fight uses its own arena (player deck plus tentacle targets).
- Movement (5 tiles default), ranges (Grapple range 10 "within viewing distance"), and AI scoring all need fast, pure grid queries.
- Deck layouts must match Synty ship meshes and be editable by Brandon without code.

## Decisions

### D1 - Board composition
| Option | Verdict |
|---|---|
| **A. One `UDwDeckLayout` per ship (player ship, each enemy ship, the Kraken arena); `FDwBoardBuilder` composes two layouts plus seam rules into one `FDwGridModel` per fight** | **Chosen.** The player deck is defined once; enemy ships vary; Kraken arena is just another "deck" on the enemy side |
| B. One hand-painted layout per encounter | Rejected: the player deck gets copied 5+ times and drifts |
| C. Procedural decks | Rejected: unnecessary, hurts readability |

### D2 - Navigation
| Option | Verdict |
|---|---|
| **A. Pure grid Dijkstra/BFS on `FDwGridModel`, visuals interpolate along the tile path** | **Chosen.** Deterministic, world-free, cheap |
| B. NavMesh + `AIController` MoveTo | Rejected: not tile-exact, needs a world, non-deterministic |

### D3 - Line of sight
| Option | Verdict |
|---|---|
| **A. Grid supercover line with per-tile blocker heights versus viewer elevation** | **Chosen.** Pure, matches the visible grid, makes the nest meaningful |
| B. Physics line traces against meshes | Rejected: needs a world, art changes change rules |

## Data model (Board/, pure)

`FDwTile` (USTRUCT)
| Field | Meaning |
|---|---|
| `EDwTileKind Kind` | `None` (not on board), `Deck`, `Nest`, `Ladder`, `Seam`, `Water`, `Obstacle` |
| `EDwSide Owner` | `Player`, `Enemy`, `Neutral` - which ship the tile belongs to (sinking kills units on that side's tiles) |
| `uint8 Elevation` | 0 deck, 1 nest |
| `uint8 BlockerHeight` | 0 none, 1 low (crates, rails), 2 high (mast, cabin) - LOS blocks if `BlockerHeight > viewer Elevation` |
| `bool bWalkable` | Can a unit stand/path here |
| `int32 MoveCost` | Default 1 (seam/ladder cost from grid rules) |
| `FGameplayTagContainer Tags` | `Tile.Cannon`, `Tile.BoardingEdge`, `Tile.Spawn.Player`, `Tile.Spawn.Enemy`, `Tile.Kraken.Body`, `Tile.Kraken.Tentacle`, ... |

`FDwGridModel` (USTRUCT): `FIntPoint Size`, `TArray<FDwTile> Tiles`, `TArray<FDwUnitHandle> Occupants`; `IsInBounds`, `GetTile`, `GetOccupant`, `SetOccupant`, `GetNeighbors(Tile, Rules)`.

`FDwGridRules` (from `DT_CombatTuning`, row `Default`): `EDwAdjacency Adjacency` (`Four`, `Eight`), `EDwRangeMetric RangeMetric` (`Manhattan`, `Chebyshev`), `bool bAllowPassThroughAllies`, `bool bAllowSplitMovement`, `int32 SeamMoveCost`, `int32 ClimbMoveCost`, `int32 RangeBonusPerElevation`, `int32 SeamWidth`, `bool bSeamStandable`. Every unanswered board question (Open-Questions C1-C6) is one of these fields.

## Pure queries

| Function | Returns | Used by |
|---|---|---|
| `FDwPathfinder::ComputeReachable(Grid, Rules, Start, Budget, MoverSide)` | `TMap<FIntPoint, int32>` cost per reachable tile | Move highlight, AI candidates |
| `FDwPathfinder::FindPath(Grid, Rules, Start, Goal, MoverSide)` | `TArray<FIntPoint>` or empty | `UDwGA_Move`, path preview |
| `FDwLineOfSight::HasLineOfSight(Grid, From, To)` | bool | Targeting, Grapple "within viewing distance" |
| `FDwLineOfSight::GetDistance(Rules, From, To)` | int32 | Range checks |
| `FDwTargetingQuery::IsInRange(Grid, Rules, From, To, Range)` | bool | Adds elevation range bonus |
| `FDwTargetingQuery::FirstUnitAlongLine(Grid, From, Direction, MaxSteps)` | handle + stop tile | Charge |
| `FDwTargetingQuery::FreeTileAdjacentTo(Grid, Tile, TowardFrom)` | tile or none | Grapple landing tile |

Movement rules (in `FDwPathfinder`):
- Elevation can change only between a `Ladder` tile and an adjacent `Nest` tile (cost `ClimbMoveCost`).
- Enemies block movement; allies block or pass through per `bAllowPassThroughAllies`; nobody ends on an occupied tile.
- Crossing between decks only through `Seam` tiles (or directly between facing `Tile.BoardingEdge` tiles when `SeamWidth == 0`).
- `Water` and `Obstacle` are never walkable.

## Layout assets and the builder

`UDwDeckLayout : UDwDefinitionBase` (Board/)
- `FIntPoint Size`, `TArray<FDwTile> Tiles` (painted with the layout editor, see `Editor-Tools.md`).
- `int32 BoardingRowOffset` - aligns this deck's boarding edge with the other ship's.
- `FTransform MeshAlignment` - grid origin relative to the ship mesh pivot, so tiles sit on the Synty deck.
- `float TileSize` (default from `UDwGameData`, overridable).
- `IsDataValid`: at least N spawn tiles for its side, every walkable tile connected, nest reachable via a ladder, cannon tiles walkable or adjacent to walkable tiles, boarding edge present (ship decks only).

`FDwBoardBuilder` (pure)
- `BuildShipBattle(PlayerDeck, EnemyDeck, Rules) -> FDwGridModel`: player deck on the left, enemy deck mirrored on the right, `SeamWidth` columns of `Seam` tiles (Neutral) between facing boarding edges, rows aligned by `BoardingRowOffset`.
- `BuildKrakenArena(PlayerDeck, ArenaLayout, Rules) -> FDwGridModel`: same placement; the arena is `Water` tiles with `Tile.Kraken.Tentacle` slots next to the rail and one `Tile.Kraken.Body` anchor. No seam; nothing to board.

```
Ship battle (example, 4-way, SeamWidth 1)        Kraken arena
  P P P P N P | S | E E E E E                    P P P P N P ~ T ~ ~
  P C P P L P | S | E E L E C E                  P C P P L P T ~ ~ ~
  P P P P P P | S | E E E E E E                  P P P P P P ~ ~ B ~
  P C P P P P |   | E C E E E E                  P C P P P P T ~ ~ ~
P/E deck  N nest  L ladder  C cannon  S seam     ~ water  T tentacle slot  B Kraken body
```

## Presentation: `ADwBoard : AActor` (Board/)
- Spawned by `UDwCombatSubsystem::StartEncounter`; places both `ADwShip` actors using each layout's `MeshAlignment`.
- `TileToWorld(FIntPoint)`, `WorldToTile(FVector)` (cursor picking against the board plane, elevation-aware for the nest).
- Highlights: one `UInstancedStaticMeshComponent` per `EDwHighlightLayer` (`Move`, `Path`, `Target`, `AreaOfEffect`, `Danger` for Kraken intents, `Hover`). `ShowHighlights(Layer, Tiles)`, `ClearHighlights(Layer)`. Materials and colours are UI-art decisions (ui-engineer picks, Brandon signs off the palette).
- Editor preview: in `OnConstruction`, if a preview layout is assigned, draws the tile grid so a designer can align a deck to a ship mesh in a dev map.

## Connections
- `02-GAS-Foundation.md`: targeting rules call these queries; `UDwGA_Move`, Grapple and Charge use the path and line helpers.
- `03-Combat-Turns.md`: the subsystem owns the grid and occupancy; snapshots copy it.
- `05-Crew-Captains.md`: synergy adjacency and defend placement use `GetNeighbors`.
- `06-Ship-Cannons.md`: cannon tiles and deck ownership.
- `07-Enemy-AI.md`: reachable sets and LOS for candidate generation.
- `08-Kraken-Boss.md`: arena layout.
- `14-UI.md`: highlights and path preview are world-space; HUD reads hover/selection.
- `Editor-Tools.md`: deck layout painter, ASCII import/export.

## Build order
1. DW-102 (gameplay-engineer): `FDwTile`, `FDwGridModel`, `FDwGridRules`, pathfinder, LOS, targeting queries, `UDwDeckLayout`, `FDwBoardBuilder::BuildShipBattle`, `ADwBoard` highlights; two placeholder layouts made from ASCII.
2. DW-110 (tools-engineer): deck layout painter in the details panel + ASCII import/export.
3. DW-301 (gameplay-engineer): `BuildKrakenArena` and the arena layout.
4. DW-307 (gameplay-engineer): align layouts to Synty ship meshes.

## How it's tested (all pure, `DeadwaterRun.Rules.Grid.*`)
- Reachable set with budget 5 on an open deck matches the expected diamond (4-way) or square (8-way).
- Enemies block, allies follow the rule flag, occupied tiles are never destinations.
- Nest reachable only via a ladder; climb cost applied.
- LOS: low blockers block deck-to-deck but not nest-to-deck; high blockers block both.
- Range bonus from elevation applied once.
- Board builder: seam joins facing boarding edges; rows align by offset; ownership correct on both sides.
- Every shipped `UDwDeckLayout` passes `IsDataValid` (data validation commandlet).

## What the player sees
- A clean tile grid laid over two Synty ships side by side, a path preview that bends around masts, a sniper perched in the nest who can see over the crates, and boarders leaping across the gap onto the enemy deck.
