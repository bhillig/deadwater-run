# N1 - Crew ability drafts (DW-304)

**DRAFT for Brandon to pick from. Nothing here is in data yet.** Every number is a placeholder for the DW-401 simulator pass. Owner: balance-designer.

## Constraints every set obeys (read from Source/, 2026-11-17)

- **Classes available:** `UDwGA_EffectPayload` (the workhorse), `UDwGA_Charge`, `UDwGA_Grapple`. `Move` / `FireCannon` / `UseItem` are not content classes.
- **Target patterns that hit units:** `Self`, `SingleUnit`, `SingleUnitOrShip`, `EnemyShip`, `AllEnemies` / `AllAllies` (every one **within tuning Range**, so Range 1 = "all adjacent"), `RandomEnemies` (Min/MaxHits, within Range). `Tile` and `Line` return tile targets with no unit, so the payload hits nothing - **no line or area-on-tile abilities**.
- **GEs an ability can tune from its row:** `GE_Damage` (`Damage`), `GE_Heal` (`Heal`), `GE_AddFear` (`Fear`), `GE_Frozen` / `GE_Rooted` (`DurationTurns`), `GE_Cleanse`. `MakeTunedSpec` fills only Damage / DamageScale / Heal / Fear / Turns.
  - Not usable from an ability today: `GE_RestoreMana` (reads `Data.Value`), `GE_DamageBonus` (reads `Data.DamageBonus`), any timed damage or armour buff. No ship heal (`UDwShipAttributeSet` has no `IncomingHeal`). No damage over time (turn durations only count down).
- **`DamageScale` is useless:** every unit has `AttackDamage` 0, so all damage is flat `Damage`.
- **No cooldown field.** MP is the only limiter. 20 MP per fight, refilled each fight, no regen (C9), `ManaPerLevel` 0. Mana Restore item = +10 MP for 30 gold.
- **One row per ability:** every Application in a payload uses the same `Damage` / `Heal` / `Turns`. "Hit, then hit again if X" = the same number twice.
- **Previews:** base `PreviewOutcome` shows one `Damage` and `Heal` per target. It ignores Application conditions, repeats and `bApplyToSelf`. A conditional, repeated or self-heal ability **previews wrong** (breaks "previews never lie", and the AI scores it wrong). Set A avoids this entirely. Sets B and C mark each ability that hits it.
- **Fear for every captain:** cowering at 3 Fear runs in the turn manager for every captain. Surrender needs `Rule.Captain.Surrender` (Fearmongerer only).
- **Kraken (K10):** immune to Fear and Grapple; freeze works on tentacles only; the body never moves, so Rooted does nothing to it.
- **Slots:** 3 per crew member + the basic attack (P4). Price: `ShopPrice` 50 on every row (GDD). Weapon tags: `Ability.Weapon.Sword` for Swordsman, `Ability.Weapon.Firearm` for Sniper (the Conqueror blocks Firearm if captains can learn from a pool).

Baseline these numbers are measured against: cutlass 25 (range 1), musket 20 (range 6, +2 from the nest), pirate 100 HP (+10/level), ship 250 HP, cannon 70.
Rough value rule: an ability is about **+2 damage per MP** above the free basic attack for that turn. Control and heals are priced as damage avoided.

---

## Set A - "Cutlass and Musket" (straightforward)

- **Theme:** clear, readable upgrades to what each archetype already does: big hits, cleaves, pins, one heal, one ship shot. Every ability is one `EffectPayload` with no conditions, so previews and AI scoring are exact.
- **New classes:** none. **Code changes:** none.

| # | Name | Arch | Effect | MP | Numbers | Range / area | Duration | Class / setup | Price |
|---|---|---|---|---|---|---|---|---|---|
| A1 | Heavy Blow | Swordsman | One big cutlass hit | 10 | 45 dmg | 1, no nest | - | EffectPayload `SingleUnit`; `GE_Damage`; `bCanTargetElevated` false | 50 |
| A2 | Cleave | Swordsman | Hits every adjacent enemy | 8 | 20 dmg each | `AllEnemies`, range 1 | - | EffectPayload `AllEnemies`; `GE_Damage` | 50 |
| A3 | Hamstring | Swordsman | Cut + target can't move | 6 | 15 dmg | 1 | Rooted 1 turn | EffectPayload `SingleUnit`; `GE_Damage`, `GE_Rooted` | 50 |
| A4 | Pommel Strike | Swordsman | Stun in melee | 12 | 10 dmg | 1 | Frozen 1 turn | EffectPayload `SingleUnit`; `GE_Damage`, `GE_Frozen` | 50 |
| A5 | Boarding Cry | Swordsman | Frighten one enemy | 6 | 2 Fear | 3, LOS | - | EffectPayload `SingleUnit`; `GE_AddFear` | 50 |
| A6 | Aimed Shot | Sniper | Long, heavy shot | 10 | 40 dmg | 8, LOS | - | EffectPayload `SingleUnit`; `GE_Damage` | 50 |
| A7 | Crow's Nest Shot | Sniper | Huge shot, only from the nest | 14 | 55 dmg | 10 (+2 nest), LOS | - | EffectPayload `SingleUnit`; `RequiredSourceTileTags` = `Tile.Nest`; `GE_Damage` | 50 |
| A8 | Pinning Shot | Sniper | Pin a boarder or runner | 6 | 15 dmg | 6, LOS | Rooted 1 turn | EffectPayload `SingleUnit`; `GE_Damage`, `GE_Rooted` | 50 |
| A9 | Scattershot | Sniper | Spray at random enemies | 10 | 3 hits x 12 | `RandomEnemies`, range 5 | - | EffectPayload `RandomEnemies`; Min/MaxHits 3/3; `GE_Damage` | 50 |
| A10 | Chain Shot | Sniper | Shred the enemy ship's hull | 10 | 50 ship dmg | `EnemyShip` | - | EffectPayload `EnemyShip`; `GE_Damage` | 50 |
| A11 | Field Dressing | Any | Patch up yourself or an adjacent ally | 8 | +30 HP | 1, ally | - | EffectPayload `SingleUnit` Side=Ally; `GE_Heal` | 50 |
| A12 | Grog Courage | Any | Shake off a root (a frozen unit cannot act, so not a freeze), small heal | 5 | +10 HP, cleanse | `Self` | - | EffectPayload `Self`; `GE_Cleanse`, `GE_Heal` | 50 |

**MP economy (20 MP):** average cost 8.75. That is about 2 casts per fight (10+10, 8+12, 6+6+8). With 3 slots, a crew member can't cast all of them in one fight, so Mana Restore (30 g) is a real buy.

**Captains:**
- **Team Leader:** synergy +20% applies to A1/A2/A6/A9 (K4a), so Cleave next to an ally does 24 x N. Grapple pulls a target into Cleave range or out of the nest for Hamstring. A12 overlaps Rally Cry's cleanse (smaller, single).
- **Fearmongerer:** A5 is the only Fear source (2 per cast, single target, 3 range). It speeds up surrender without making it automatic. Simulator watch: surrender rate per fight.
- **Conqueror:** his crew can move OR act, so ranged and no-move abilities (A6, A8, A9, A10, A11) are worth more to him than melee. That is a natural pull toward Snipers. A4 + Eye of the Tiger = two frozen enemies in one turn.

**Kraken:** A5 is dead (Fear immune). A8/A3 are dead on the body (live on tentacles if they move). A10 is dead (no enemy ship). A4 freezes tentacles only. Everything else works.

**Strengths / risks:** + exact previews, zero code, easiest AI. - least "build-around" depth. Chain Shot vs cannon (70, free, needs a cannon tile) needs a sim check so sinking doesn't become the default.

---

## Set B - "Dirty Fighting" (combos and statuses)

- **Theme:** status setups and payoffs (Rooted, Frozen/Cowering, Afraid, boarders on your deck). Players pick a combo plan per crew, and each captain's status tool becomes a payoff enabler.
- **New classes:** none.
- **Code change needed (not a class):** `UDwGA_EffectPayload::PreviewOutcome` must walk `Applications` (conditions, repeats, `bApplyToSelf`). Abilities that need it are marked **[P]**. Without it their previews and AI scores are wrong.

| # | Name | Arch | Effect | MP | Numbers | Range / area | Duration | Class / setup | Price |
|---|---|---|---|---|---|---|---|---|---|
| B1 | Dirty Kick | Swordsman | Setup: root in melee | 4 | 10 dmg | 1 | Rooted 1 turn | EffectPayload `SingleUnit`; `GE_Damage`, `GE_Rooted` | 50 |
| B2 | Kick 'Em While Down **[P]** | Swordsman | Double damage vs a helpless target | 8 | 20, +20 if `Status.Incapacitated` (frozen or cowering) | 1 | - | EffectPayload; `GE_Damage`; `GE_Damage` with `RequiredTargetTags` = `Status.Incapacitated` | 50 |
| B3 | Menace | Swordsman | Setup: cut + Fear | 5 | 15 dmg, 1 Fear | 1 | - | EffectPayload `SingleUnit`; `GE_Damage`, `GE_AddFear` | 50 |
| B4 | Terror Strike **[P]** | Swordsman | Payoff vs the afraid | 8 | 20, +20 if `Status.Afraid` | 1 | - | EffectPayload; `GE_Damage` x2 (2nd requires `Status.Afraid`) | 50 |
| B5 | Repel Boarders **[P]** | Swordsman | Payoff vs enemies on your deck | 6 | 20, +20 if target has `State.OnEnemyDeck` | 1 | - | EffectPayload; `GE_Damage` x2 (2nd requires `State.OnEnemyDeck`) | 50 |
| B6 | Kneecap | Sniper | Setup: root at range | 5 | 10 dmg | 6, LOS | Rooted 1 turn | EffectPayload `SingleUnit`; `GE_Damage`, `GE_Rooted` | 50 |
| B7 | Picked Off **[P]** | Sniper | Payoff vs the rooted | 8 | 20, +20 if `Status.Rooted` | 8, LOS | - | EffectPayload; `GE_Damage` x2 (2nd requires `Status.Rooted`) | 50 |
| B8 | Sitting Duck **[P]** | Sniper | Big payoff vs a helpless target | 10 | 15, x3 (45) if `Status.Incapacitated` | 10, LOS | - | EffectPayload; `GE_Damage` x3 (2nd and 3rd require `Status.Incapacitated`) | 50 |
| B9 | Warning Shot | Sniper | Pure Fear at range | 6 | 2 Fear | 8, LOS | - | EffectPayload `SingleUnit`; `GE_AddFear` | 50 |
| B10 | Powder Shot | Sniper | Hits a pirate or the hull | 10 | 35 dmg (unit or ship) | 6, LOS | - | EffectPayload `SingleUnitOrShip`; `GE_Damage` | 50 |
| B11 | Jeer | Any | Fear to every enemy nearby | 8 | 1 Fear each | `AllEnemies`, range 3 | - | EffectPayload `AllEnemies`; `GE_AddFear` | 50 |
| B12 | Smelling Salts | Any | Free an ally from root/freeze, small heal | 6 | +15 HP, cleanse | 1, ally | - | EffectPayload `SingleUnit` Side=Ally; `GE_Cleanse`, `GE_Heal` | 50 |

**MP economy (20 MP):** average 7. That is about 3 casts per fight, and a setup + payoff pair costs 12-15, so one full combo per crew member per fight. A 4-MP Dirty Kick plus two payoffs is the efficient line.

**Captains:**
- **Fearmongerer:** biggest winner. B3/B9/B11 + Smoke and Fire reach 3 Fear fast, giving surrender, 1.5x loot and no deaths. B4 turns Afraid into damage. **Parity risk #1**: the sim must cap his surrender rate (for example `Fear` 1 on B9, or B11 range 2).
- **Conqueror:** Eye of the Tiger = `Status.Incapacitated.Frozen` sets up B2/B8 (45 from range). His crew's move-or-act rule favours B6 + B7 from the back. Second-strongest here.
- **Team Leader:** fewest native setups (Grapple, Defend). Synergy +20% multiplies each damage application, so payoffs scale. Rally Cry + B12 makes him immune to enemy roots and freezes. Likely the weakest captain in this set, so the sim must check his win rate.

**Kraken:** weakest set at the boss. Fear payoffs (B3/B4/B9/B11) and root payoffs (B6/B7) are dead on the body. B2/B8 only fire on frozen tentacles. B5 depends on the arena. B12 counters Tentacle Grab. Expect a win-rate dip at the Kraken: decide if that is "the build has a weakness" or a problem.

**Strengths / risks:** + most real decisions and build identity, strong "one more run". - needs the preview code change, the AI needs DW-214 to value roots, freezes and conditions, and the Fearmongerer surrender risk.

---

## Set C - "Broadsides and Boarders" (risk, position, the ship)

- **Theme:** abilities that push the sink-vs-plunder choice and positioning (nest, boarding, your own deck), plus high-risk hits with a self-drawback.
- **New classes:** at most 1. **C5 Brace needs new class: `UDwGE_TurnDamageReduction`** (DamageReduction += value, turn duration) **plus** a SetByCaller for that value filled from the tuning row (one line in `ApplyTuningToSpec`). Fallback with no new class: swap C5 for A3 Hamstring.
- C1 reuses `UDwGA_Charge` (the Conqueror's class), with shorter range and less damage. Drop it if you count that as duplicating Charge (fallback: A2 Cleave).

| # | Name | Arch | Effect | MP | Numbers | Range / area | Duration | Class / setup | Price |
|---|---|---|---|---|---|---|---|---|---|
| C1 | Boarding Rush | Swordsman | Short dash, strike the first enemy | 8 | 30 dmg | Straight line, 3 | - | `UDwGA_Charge` BP; `GE_Damage` | 50 |
| C2 | Reckless Swing **[P]** | Swordsman | Huge hit, then you're spent | 8 | 50 dmg | 1 | Self Frozen 1 turn | EffectPayload `SingleUnit`; `GE_Damage`; `GE_Frozen` `bApplyToSelf` | 50 |
| C3 | Sweep the Deck | Swordsman | Hit every adjacent enemy | 10 | 20 dmg each | `AllEnemies`, range 1 | - | EffectPayload `AllEnemies`; `GE_Damage` | 50 |
| C4 | Scuttle | Swordsman | Hack the enemy hull | 12 | 60 ship dmg | `EnemyShip` | - | EffectPayload `EnemyShip`; `GE_Damage` | 50 |
| C5 | Brace | Swordsman | Take less damage until your next turn | 6 | -50% damage taken | `Self` | 1 turn | **needs new class: `UDwGE_TurnDamageReduction`** | 50 |
| C6 | Chain Shot | Sniper | Shred the hull from anywhere | 8 | 40 ship dmg | `EnemyShip` | - | EffectPayload `EnemyShip`; `GE_Damage` | 50 |
| C7 | Crow's Nest Volley | Sniper | Rain shots, only from the nest | 12 | 4 hits x 12 | `RandomEnemies`, range 8 (+2 nest) | - | EffectPayload `RandomEnemies`; `RequiredSourceTileTags` = `Tile.Nest`; Min/MaxHits 4/4 | 50 |
| C8 | Long Shot | Sniper | Reach across both decks | 8 | 30 dmg | 10, LOS | - | EffectPayload `SingleUnit`; `GE_Damage` | 50 |
| C9 | Pin Down | Sniper | Stop a boarder on your deck | 5 | 10 dmg | 6, LOS | Rooted 1 turn | EffectPayload `SingleUnit`; `GE_Damage`, `GE_Rooted` | 50 |
| C10 | Powder Shot | Sniper | Hits a pirate or the hull | 10 | 35 dmg (unit or ship) | 6, LOS | - | EffectPayload `SingleUnitOrShip`; `GE_Damage` | 50 |
| C11 | Ship's Surgeon | Any | Bigger heal, adjacent ally only | 10 | +40 HP | 1, ally (not self) | - | EffectPayload `SingleUnit` Side=Ally; `GE_Heal` | 50 |
| C12 | Bellow | Any | Fear to every enemy nearby | 6 | 1 Fear each | `AllEnemies`, range 2 | - | EffectPayload `AllEnemies`; `GE_AddFear` | 50 |

(C11 "not self" needs a targeting check that excludes the caster. If `SingleUnit` Side=Ally can't exclude self, allow self, which is fine for balance.)

**MP economy (20 MP):** average 8.6, about 2 casts per fight. Ship abilities stack with free cannons: C4 + C6 + 2 cannons = 240 in one turn, enough to sink a 250 hull. **Sink speed is the #1 sim check**, because sinking skips plunder and that is the core risk/reward choice.

**Captains:**
- **Conqueror:** C1 Boarding Rush is move + attack in one action, which breaks his crew's move-or-act rule in their favour. Ship abilities need no move. **Parity risk #1**: likely strongest here.
- **Team Leader:** C3 + synergy, C5 + Defend makes a tanky front line, and Rally Cry cleanses C2's self-freeze (a real combo).
- **Fearmongerer:** C12 adds Fear but less than Set B. Sinking competes with his surrender route (both end the fight, and only surrender gets plunder), so expect him to sink less.

**Kraken:** ship abilities (C4/C6, the ship half of C10) are dead. That is 2.5 of 12, so a ship-heavy crew enters the boss weak. C12 is dead. C2 / C3 / C7 / C8 are strong vs the 500 HP body.

**Strengths / risks:** + leans hardest into the game's unique hook (two health bars), strong positional play. - one new class, one [P] preview gap (C2's self-freeze is not shown), fast sinks and Conqueror dominance to tune.

---

## Cross-set notes (all sets)

- **Gold:** about 100 + 5 fights x 150-225 = about 850-1,225 gold per run, plus 1 free ability per fight win (5). Ability slots: 3 x 3-4 crew = 9-12. Expect about 5 from rewards + 2-4 bought (100-200 g), competing with repair 100, recruit 80, upgrade 150 and items 30. That is the "heal or upgrade" decision. Alternative if the sim shows abilities are always or never bought: tier 40 (5-6 MP utility) / 50 / 60 (12-14 MP).
- **Run length:** abilities shorten fights (bigger hits) but add a decision per turn. The sim should report turns per fight against the ~30-minute target (DW-401).
- **Enemy crews (N2):** if enemy crews use this pool, Rooted, Frozen and Fear on the player matter. Set B's B12 and Set A's A12 are the counters, and Team Leader's Rally Cry gets stronger.
- **AI:** `FDwAbilityAiHints` is empty. Roots, freezes and self-drawbacks have no AI value until DW-214. AI `Weights.Fear` 10 already exists.
- **Perks (P2):** Cutthroat +10% applies to every ability's damage. Hardened stacks with C5 Brace (DamageReduction is clamped at 0.9).

## Recommendation

**Set A, with up to 2 Set B swaps once the preview code change lands.**
- Zero new classes and zero code. Previews and AI are exact. Buildable this week (content week, Nov 16-22).
- Smallest captain-parity risk: one modest Fear source, no move-and-attack for the Conqueror's crew, nothing that only works vs the Kraken.
- Its weakness is depth. If the Gameplay Engineer adds `Applications`-aware previews, swap A5 Boarding Cry for **B4 Terror Strike** and A9 Scattershot for **B8 Sitting Duck**. Each captain then gets one payoff (Afraid / Frozen), which adds build decisions without Set B's surrender spike.
- Pick B if depth matters more than the schedule. Pick C if you want the sink-vs-plunder choice front and centre and accept one new class.

## What I need from Brandon

1. Pick A, B, C or the A + 2-swap hybrid.
2. If B or the hybrid: approve the `PreviewOutcome` change (Gameplay Engineer).
3. If C: approve `UDwGE_TurnDamageReduction` (or take the Hamstring fallback), and say whether a shorter Charge counts as duplicating the Conqueror.
4. Can captains learn crew abilities (a captain `LearnablePool`)? The drafts assume crew only. "Any" rows would also go to captains if yes.
5. Ask the Tools Engineer for simulator support: win rate per captain, turns per fight, sink/plunder/surrender split and death node, for the chosen set (DW-401).
