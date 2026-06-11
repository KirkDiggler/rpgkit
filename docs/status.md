# Status

_Last updated: 2026-06-11 (tutorials foundation PR)_

## Current health

**First third of core v1 is live and green.** `rpg::core` has `Status`
(no-exceptions error type), the type-erased `Bus` (snapshot delivery,
fail-fast), and staged `Chain<T>` with the breakdown receipt — 22 tests, CI
green on Linux clang/gcc + Windows MSVC. First cookbook example
(`examples/chain_basics`) and how-to recipe are merged.

## Active work

- **Tutorials** (`docs/tutorials/`) — progressive series building a terminal
  deck-builder; written for non-programmers and small local models alike.
  01 (setup) and 02 (game base, `examples/tutorials/02_game_base`) landing
  now; 03+ (cards/energy, block, monster AI) follow.
- **core v1 remainder** — typed topic veneer (`Topic<T>`/`ChainedTopic<T>`,
  plan-PR 3); `Action`, `Effect`, Strike example (plan-PR 4). Plan:
  [rpg-project/ideas/portable-combat-core/plan.md](https://github.com/KirkDiggler/rpg-project/blob/main/ideas/portable-combat-core/plan.md)
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
- Game code currently touches `Chain` directly; the typed topic veneer that
  hides bus-level `std::any` from rulebooks doesn't exist yet (plan-PR 3).
