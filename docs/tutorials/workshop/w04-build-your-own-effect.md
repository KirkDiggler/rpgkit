# Workshop W04: Build Your Own Effect

**Goal:** Implement a status effect that lives on the bus, ticks each turn, and
self-removes.

## The skeleton

`skeleton.cpp` is your starting point — it compiles but does nothing.

## The solution

`solution.cpp` implements **Burning**: a status that:

1. Subscribes to `turn.ended` in `onApply()`
2. Deals fire damage equal to remaining stacks each turn
3. Counts down each turn
4. Calls `remove()` at zero to unsubscribe and clean up

## Run the solution

```bash
cmake --build build -j && ./build/examples/workshop/w04_build_effect/workshop_w04_effect_solution
```

## What you should see

```
=== Burning Effect Demo ===

🔥 Burning applied (3 stacks)

--- Turn 1 ---
🔥 Burning deals 3 fire damage

--- Turn 2 ---
🔥 Burning deals 2 fire damage

--- Turn 3 ---
🔥 Burning deals 1 fire damage
🔥 Burning expired

--- Turn 4 ---

--- Turn 5 ---

Done. Effect self-removed at zero stacks.
```

## Your turn

Write your own effect by filling in the skeleton. Three ideas:

1. **Poison** — damage increases by +1 each turn instead of decreasing
2. **Aura** — subscribes to an ally's damage chain, buffs their attacks
3. **Barrier** — absorbs N damage from incoming hits, then breaks

The lifecycle is always the same:

1. `onApply()` — subscribe to topics, `track()` every subscription
2. React to events — publish to chains, modify values
3. `remove()` — cleanup is automatic for tracked subscriptions
