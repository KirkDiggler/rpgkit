# Status

_Last updated: 2026-06-11 (core v1 complete)_

## Current health

**Core v1 is complete.** All four contracts live: `Status`, the type-erased
`Bus`, staged `Chain<T>` with receipts, the typed `Topic<T>`/`ChainedTopic<T>`
veneer, `Action<TInput>`, and `Effect` with tracked lifecycle — 40 tests
including the v1 acceptance integration (`examples/strike`: an Action fires,
an Effect modifies it, breakdown out, removal reverts). CI green on Linux
clang/gcc + Windows MSVC. Released through v0.1.0; v0.2.0 tag pending with
topics + Action/Effect.

## Active work

- **Tutorials** (`docs/tutorials/`) — progressive series building a terminal
  deck-builder; written for non-programmers and small local models alike.
  01 (setup) and 02 (game base, `examples/tutorials/02_game_base`) landing
  now; 03+ (cards/energy, block, monster AI) follow.
- **v0.2.0 release** — tag once Action/Effect merges so consumers (the demo
  game) can pin the full nervous system.
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
