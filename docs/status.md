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

- **Tutorial 08 — Effects** (#29): BleedEffect subscribes to `turn.ended`,
  ticks damage, self-removes at zero. The game loop goes from orchestrator
  to announcer. This is the v0.2.0 milestone — Effects own their lifecycle.
  PR open, pending review.
- **Tutorial tracks** — two-path structure in `docs/tutorials/`:
  - **Campaign** (03–08, expanding): game-building series, feature by feature
  - **Workshop** (W01–W04): architecture deep-dives (Bus, Chains, Rage,
     Build Your Own Effect)
  - **Juice appendix**: optional terminal polish at any checkpoint
  - **Demo-game experiment** — local qwen model builds against the tutorials/
    cookbook; findings become doc/API issues. Design:
    [rpg-project/ideas/rpgkit-demo-game](https://github.com/KirkDiggler/rpg-project/tree/main/ideas/rpgkit-demo-game)

## Completed (recent)

- **Tutorial 08 — Effects** (in progress): BleedEffect, Rend card,
  activeEffects list, announcer shift.
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
