# Status

_Last updated: 2026-06-13 (v0.1.2 — Rich Events)_

## Current health

**Core v1 is complete.** All four contracts live: `Status`, the type-erased
`Bus`, staged `Chain<T>` with receipts, the typed `Topic<T>`/`ChainedTopic<T>`
veneer, `Action<TInput>`, and `Effect` with tracked lifecycle — 40 tests
including the v1 acceptance integration (`examples/strike`: an Action fires,
an Effect modifies it, breakdown out, removal reverts). CI green on Linux
clang/gcc + Windows MSVC. Released through v0.1.2.

**Tutorials 01–08 are live** in `examples/tutorials/`. The tutorial-06 redesign
split "Statuses that Stick Around" into three focused episodes (06: Bus,
07: Rich Events, 08: Effects). Design spec:
`docs/journey/2026-06-13-linear-tutorials-6-8.md`.

## Active work

- **Tutorial tracks** — two-path structure in `docs/tutorials/`:
  - **Campaign** (03–08, expanding): game-building series, feature by feature
  - **Workshop** (W01–W04): architecture deep-dives (Bus, Chains, Rage,
     Build Your Own Effect)
  - **Juice appendix**: optional terminal polish at any checkpoint
  - **Demo-game experiment** — local qwen model builds against the tutorials/
    cookbook; findings become doc/API issues. Design:
    [rpg-project/ideas/rpgkit-demo-game](https://github.com/KirkDiggler/rpg-project/tree/main/ideas/rpgkit-demo-game)

## Customer pressure from rpgkit-ue

Project 15 is the customer/use-case surface for the Unreal integration. Its
capability slices are developer goals; `rpgkit` work should be promoted from
those slices only when `rpgkit-ue` exposes concrete friction, a missing portable
seam, or a documentation gap.

Current pressure clusters:

- **Observability** — structured facts for action start, topic publish,
  subscriber invocation, modifier addition, chain execution, effect lifecycle,
  and state changes.
- **Chain receipts** — source metadata, stable modifier identity, human labels,
  and formatting guidance so hosts can explain why a result changed.
- **Effect lifecycle** — ergonomic patterns for host-created runtime effects,
  subscription ownership, apply/remove visibility, and safe cleanup.
- **Action context** — portable guidance for actor/source/target/action identity
  and lifecycle observations across cards, enemy intents, items, and abilities.
- **Host adapter boundaries** — documentation that separates RPG concepts owned
  by `rpgkit` from Unreal-owned authoring, targeting, encounter orchestration,
  UI, and persistence.

Promotion rule: do not create speculative core extensibility work. A future
`rpgkit` issue should cite the `rpgkit-ue` use case that proved the need and
describe how the result will be verified back in the Unreal integration.

## Completed (recent)

- **Tutorial 08 — Effects**: BleedEffect, Rend card, activeEffects list,
  announcer shift. The game loop publishes `turn.ended` and lets effects
  respond — no effect-specific logic in the loop.
- **Tutorial 07 — Rich Events** (v0.1.2): `DamageAttempt`, `ChainedTopic`,
  tough-skin and block as bus subscribers. Plumbing refactor — behavior
  identical to tutorial 06, but effects can now subscribe too.
- **Tutorial 06 — The Bus** (v0.1.2): `rpg::core::Bus`, `turn.ended`
  notification topic, combat-log subscriber, block reset via subscriber instead
  of inline code.

## Paused / not started

- Tutorials 09+ (old numbering 07+): Cards Become Actions, The Town Crier,
  Boss Fight — renumbered pending after tutorial 08 lands
- `mechanics/`, `rulebooks/` modules — deferred until core v1 lands
- C ABI layer (Unity), Unreal plugin wrapper — out of scope for v1

## Known rough edges

- `Bus::publish` copies the subscriber list (handlers included) per publish —
  correct-by-construction snapshot semantics, unmeasured cost. Revisit if
  profiling ever cares.
- No benchmarks anywhere yet; "cheap enough" is asserted, not measured.
