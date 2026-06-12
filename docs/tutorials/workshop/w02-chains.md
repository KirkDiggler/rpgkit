# Workshop W02: Chains & Stages

**Goal:** See how a Chain folds a value through ordered stages, with every
contributor named in the breakdown.

## The primitive

A `Chain<T>` is a staged pipeline. You declare the stages (the rulebook's
ordering authority), contributors add modifiers at named stages, and
`execute()` runs every modifier in stage order, returning the final value
and a full breakdown of who contributed what.

This is the pattern that lets Rage add +2 damage at Features and resistance
at Final — two contributions from one effect, both named in the receipt.

## Run it

```bash
cmake -B build && cmake --build build -j
./build/examples/workshop/w02_chains/workshop_w02_chains
```

## What you should see

```
Base damage: 10
  rage [features]: 10 -> 12
  magic_sword [equipment]: 12 -> 13
  resistance [final]: 13 -> 6
Final: 6
```

## How it works

1. `Chain<int>{{"base", "features", ..., "final"}}` — declare stages in order
2. `.add("stage", "unique_id", modifier)` — register a named modifier at a stage
3. `.execute(base_value)` — folds through all stages, returns `Result{value, breakdown}`
4. `result.breakdown` — each `Step` has `id`, `stage`, `before`, `after`

## Key insight

Two modifiers can target the same stage (or different stages) and neither
knows the other exists. The chain orders them by stage, then insertion order.
The breakdown tells you exactly what happened — no debugger needed.
