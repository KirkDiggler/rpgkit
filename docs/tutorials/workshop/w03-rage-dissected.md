# Workshop W03: Rage Dissected

**Goal:** Read a real D&D 5e Rage implementation to see how the Bus + Chain
pattern makes complex rules simple.

## The source files

The canonical Rage lives in the sibling project
[rpg-toolkit](https://github.com/KirkDiggler/rpg-toolkit):

- `rulebooks/dnd5e/features/rage.go` (182 lines) — the Feature that manages
  the resource (uses per day, active duration)
- `rulebooks/dnd5e/conditions/raging.go` (297 lines) — the Condition that
  subscribes to bus events while Rage is active

## What Rage does

Open both files and trace the lifecycle:

1. Rage is activated → a `Condition` is created
2. The Condition subscribes to **5 event topics** (hit, damage-dealt,
   damage-chain, turn-started, turn-ended)
3. On a damage-chain event, the `onDamageChain` handler runs:
   - **Outgoing:** adds +2 at the `features` stage (bonus damage)
   - **Incoming:** halves physical damage at the `final` stage (resistance)
4. On turn-end, a counter ticks down
5. At zero, the Condition unsubscribes everything and removes itself

## Key insight

**One handler does both outgoing and incoming.** The chain stages separate
them: +2 enters at Features, resistance applies at Final. Same function, two
contributions, one receipt. No if-else branching on "am I attacking or being
attacked" — the stage tuple makes it declarative.

## Questions to answer while reading

1. How does the handler know which damage is outgoing vs incoming?
2. What happens if Rage expires mid-resolution?
3. How does the Condition ensure cleanup even if the game loop forgets?
4. Why are Features and Conditions separate types in rpg-toolkit?
