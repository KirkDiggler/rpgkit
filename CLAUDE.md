# rpgkit

C++20 implementation of the **portable combat core** — the Bus / Chain / Action /
Effect nervous system extracted from [rpg-toolkit](https://github.com/KirkDiggler/rpg-toolkit)'s
design. Sibling project, not a port-in-place: the Go toolkit keeps shipping the
existing game; rpgkit evolves independently for engine embedding (Unreal natively,
Unity via a future C ABI).

**Source design (don't re-derive):**
[rpg-project/ideas/portable-combat-core/design.md](https://github.com/KirkDiggler/rpg-project/blob/docs/portable-combat-core/ideas/portable-combat-core/design.md)
· implementation plan: `plan.md` beside it.

## The boundary

**Core is system-agnostic.** It never knows what "rage", "bleed", or "energy"
means. D&D 5e and co-op deck-builders are *rulebooks* layered on top of the same
four contracts. If game-specific logic shows up in `core/`, say something.

## Binding decisions

Settled in the design + plan. Implementers do not relitigate.

| # | Decision |
|---|---|
| 1 | **No exceptions in the core API.** Fallible operations return `rpg::core::Status`. Never throw across the API surface; never return success on failure (the Go "never return (nil, nil)" rule, translated). |
| 2 | **Header-only `rpg::core`** — `INTERFACE` CMake target. Compiled TUs only if build times demand it later. |
| 3 | **Type erasure via `std::any`** in the Bus; the typed `Topic<T>` veneer casts in/out. |
| 4 | **Stages only — no integer priorities.** `Chain<T>` is constructed with an ordered stage list; modifiers join a named stage. |
| 5 | **`SubscriptionId` is an opaque handle** (wrapped `uint64_t`) so a future C ABI can pass it across the wire. |
| 6 | **The breakdown is in scope for v1.** `Chain<T>::execute` returns the folded value *and* per-modifier records. |
| 7 | **Naming:** types `PascalCase`, methods `lowerCamelCase`, headers `snake_case.hpp` under `include/rpg/core/`. Notification-vs-chained lives in the type (`Topic<T>` vs `ChainedTopic<T>`), both with plain `subscribe`/`publish`. |
| 8 | **Synchronous, ordered, fail-fast delivery.** Publish walks subscribers in insertion order, returns the first error. |

## Workflow

- **Issue-first.** No branch without an issue; one PR per logical unit.
- Branch from fresh main; never reuse old branches; merge, never rebase.
- TDD: test first, implement, `make test` green, commit.
- **`make pre-commit` green before every push** (fmt-check + clang-tidy + tests).
  CI enforces the same. Never `--no-verify`.
- Open the PR, wait for Copilot review, fix or reply on every thread.
- **Never merge without Kirk's explicit approval.**

## Testing

- GoogleTest via FetchContent; tests live in `<module>/tests/`.
- Hand-written fakes preferred for base-class exercises; add `GTest::gmock` to the
  link line only when a test first needs it. Reserve "mock" for generated/gmock
  doubles — name hand-written ones `fakeX`/`testX`.

## Docs

Living docs: `docs/status.md`, `docs/quality.md`, `docs/architecture/`,
`docs/how-to/`. Update them **in** the PR that changes reality, not after.
