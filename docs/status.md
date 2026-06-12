# Status

_Last updated: 2026-06-11 (core v1.1 — pattern layer)_

## Current health

**Core v1 is complete.** All four contracts live: `Status`, the type-erased
`Bus`, staged `Chain<T>` with receipts, the typed `Topic<T>`/`ChainedTopic<T>`
veneer, `Action<TInput>`, and `Effect` with tracked lifecycle — 40 tests
including the v1 acceptance integration (`examples/strike`: an Action fires,
an Effect modifies it, breakdown out, removal reverts). CI green on Linux
clang/gcc + Windows MSVC. Released through v0.1.1.

## Active work

- **Tutorial tracks** — two-path structure in `docs/tutorials/`:
  - **Campaign** (03–09): game-building series, feature by feature
  - **Workshop** (W01–W04): architecture deep-dives (Bus, Chains, Rage,
     Build Your Own Effect)
  - **Juice appendix**: optional terminal polish at any checkpoint
- **v0.2.0 — Effects as autonomous bus citizens** (tutorial 06). Tag once
  Effects own their lifecycle (subscribe/expire/remove) without the game
  loop orchestrating them.
- **Demo-game experiment** — local qwen model builds against the tutorials/
  cookbook; findings become doc/API issues. Design:
  [rpg-project/ideas/rpgkit-demo-game](https://github.com/KirkDiggler/rpg-project/tree/main/ideas/rpgkit-demo-game)

## Paused / not started

- `mechanics/`, `rulebooks/` modules — deferred until core v1 lands
- C ABI layer (Unity), Unreal plugin wrapper — out of scope for v1

## Known rough edges

- `Bus::publish` copies the subscriber list (handlers included) per publish —
  correct-by-construction snapshot semantics, unmeasured cost. Revisit if
  profiling ever cares.
- No benchmarks anywhere yet; "cheap enough" is asserted, not measured.
