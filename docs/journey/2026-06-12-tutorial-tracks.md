# Tutorial Tracks — Choose Your Path

**Date:** 2026-06-12
**Status:** Design (not yet implemented)

## The Problem

rpgkit has two kinds of early users:

1. **The game builder** who wants to make something playable fast — cards, energy, block, a boss fight. Visual polish matters. The architecture is infrastructure, not the point.
2. **The engineer** who wants to understand what makes this toolkit different — how a Barbarian's Rage is just a subscriber to an attack chain, how decoupling effects from game logic simplifies everything, how to build their own rules.

One linear tutorial series serves neither well. The builder wades through architecture they don't care about; the engineer waits too long for the "aha" moment.

## The Solution: Tracks

After the common foundation (tutorials 01–02), the docs fork. Two paths, one optional appendix, one rejoin point.

```
┌─────────────────────────────────────────────────────┐
│                 01: Setup                            │
│                 02: Your First Terminal Game         │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ┌─ "The Campaign" ──────────────────────┐          │
│  │  03: Cards & Energy                   │          │
│  │  04: Block as a Chain Rule            │  ┌─────┐ │
│  │  05: Monster Brain & AI               │  │Juice│ │
│  │  06: Statuses on the Bus (v0.2.0)     │  │App. │ │
│  │  07: Cards Become Actions             │  │     │ │
│  │  08: The Town Crier                   │  │any  │ │
│  │  09: Boss Fight Capstone              │  │time │ │
│  └───────────────────────────────────────┘  └─────┘ │
│                                                     │
│  ┌─ "The Workshop" ────────────────────────┐        │
│  │  W01: The Bus — events flow through you │        │
│  │  W02: Chains & Stages — the pipeline    │        │
│  │  W03: Rage — a real effect dissected    │        │
│  │  W04: Build Your Own Effect             │        │
│  │        ↓ rejoins Campaign at 06         │        │
│  └─────────────────────────────────────────┘        │
└─────────────────────────────────────────────────────┘
```

## The Campaign (existing, untouched)

The linear game-building series. Tutorials 03–09 as they are, building the deck-builder terminal game step by step. No changes needed — the track is already written.

## The Workshop (new)

Four modules under `examples/workshop/`, CI-compiled so they never rot.

### W01: The Bus

The primitive that makes everything else work. Minimal example: create a `Bus`, define a `Topic<int>`, subscribe with a lambda, publish a value. Show that publishing reaches all subscribers without either side knowing about each other.

The narrative: "This is what lets Rage just *listen* for damage events without the game loop knowing it exists."

### W02: Chains & Stages

A damage number flows through ordered stages (Base → Features → Conditions → Equipment → Final). Build a `Chain<int>` with named stages, add modifiers at different stages, call `execute()` and inspect the receipt/ledger.

The narrative: "Rage adds +2 damage at the Features stage and resistance at the Final stage — two contributions from one source, both named in the receipt."

### W03: Rage Dissected

A guided reading session. Points at the canonical rpg-toolkit implementation:
- `rage.go` (182 lines) — the Feature that manages the resource
- `raging.go` (297 lines) — the Condition that subscribes to 5 event topics
- The single `onDamageChain` handler that contributes both outgoing bonus and incoming resistance

No new code. Narrative with citations. The point: see how a complex game rule becomes "subscribe → check → contribute → receipt."

### W04: Build Your Own Effect

A skeleton they fill in. rpgkit's `Action` and `Effect` contracts provide the lifecycle:

```
class Burning : public rpg::core::Effect {
    // subscribe to turn.ended → publish damage to chain
    // subscribe to damage chain → contribute +1 per stack
    // count down → remove at zero
};
```

The tutorial provides the 15-line skeleton and three challenge variants: a poison that stacks, an aura that buffs allies, a shield that absorbs then breaks. The reader picks one.

This module rejoins the Campaign at tutorial 06 (Statuses on the Bus).

## The Juice Appendix (new, optional)

One standalone doc with five checkpoints, adoptable by any tutorial at any time:

| # | Name | What it adds |
|---|------|-------------|
| JUICE-01 | Color | ANSI codes for HP, damage, card names |
| JUICE-02 | HP bar | Unicode block characters — `██████░░` |
| JUICE-03 | Clear on turn | Screen reset between turns |
| JUICE-04 | Pacing | Small delay after receipt printout |
| JUICE-05 | FTXUI signpost | "When escape codes aren't enough, here's where to go" |

Each checkpoint says "works with Campaign tutorial N, Workshop module M."

## README Changes

`docs/tutorials/README.md` gets a fork after "02: Your First Terminal Game":

> ### Pick your path
>
> | The Campaign | The Workshop |
> |---|---|
> | Build a game, feature by feature | Understand the engine |
> | 03 → 04 → 05 → 06 → ... | W01 → W02 → W03 → W04 → 06 |
> | Start here to make something fast | Start here to see how it works |
>
> *The Juice appendix is available from either path, at any checkpoint.*

## Scope

This design is about *organization and narrative,* not new engine features. The Workshop teaches the existing contracts (Bus, Chain, Topic, Action, Effect) that already ship in v0.1.1. The only new content is code examples under `examples/workshop/` and a guided-reading doc for W03.

## Future

Once all tracks are built and stable, the module grid (Approach C from the design discussion) becomes the natural next layer — a dependency-graph TOC that lets readers jump to any module by prerequisite, treating both tracks as interchangeable units. But that's after the narrative is proven.
