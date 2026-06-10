# Status

_Last updated: 2026-06-10 (PR 1)_

## Current health

**Scaffold only.** The build system, lint pipeline, CI matrix (Linux clang/gcc,
Windows MSVC), and docs skeleton exist and are green. `rpg::core` contains a
single `version.hpp` — there is no behavior yet.

## Active work

- **core v1** — the smallest slice that proves the nervous system: `Status`,
  type-erased `Bus`, staged `Chain<T>` with breakdown (PR 2); typed topic veneer
  (PR 3); `Action`, `Effect`, and the Strike example (PR 4). Plan:
  [rpg-project/ideas/portable-combat-core/plan.md](https://github.com/KirkDiggler/rpg-project/blob/docs/portable-combat-core/ideas/portable-combat-core/plan.md)

## Paused / not started

- `mechanics/`, `rulebooks/` modules — deferred until core v1 lands
- C ABI layer (Unity), Unreal plugin wrapper — out of scope for v1

## Known rough edges

- None yet — nothing exists to be rough.
