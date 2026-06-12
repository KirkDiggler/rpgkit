# Architecture

rpgkit is a C++20 monorepo implementing the **portable combat core** designed in
[rpg-project/ideas/portable-combat-core/design.md](https://github.com/KirkDiggler/rpg-project/blob/docs/portable-combat-core/ideas/portable-combat-core/design.md)
— the source design; this page summarizes, it doesn't redefine.

## Monorepo layout

Modules are CMake targets under the `rpg::` namespace (Abseil/EnTT-family layout).
Consumers link only the targets they want.

```
rpgkit/
  core/        rpg::core — the nervous system (Bus, Chain, Action, Effect). Header-only.
  mechanics/   (later) reusable mechanics built on core
  rulebooks/   (later) game systems built on mechanics — D&D 5e, deck-builders
```

The boundary: **core is system-agnostic**. It never knows what "rage" or "bleed"
means — D&D 5e and an Across-the-Obelisk-style deck-builder are *rulebooks* layered
on top of the same four contracts.

## The four contracts

- **Bus** — routes events by string topic id over a type-erased payload.
  Subscribe returns an opaque `SubscriptionId`; delivery is synchronous, in
  subscription order, fail-fast on the first error.
- **Chain\<T\>** — a transient, per-resolution collector of modifiers. Built with an
  ordered **stage list**; each modifier joins a named stage with a stable unique id.
  `execute` folds the value through the stages and returns the result **with a
  per-modifier breakdown** ("rage: +2, bless: +4") — the breakdown is a core selling
  point, not an extra.
- **Action** — the verb that fires once. `canActivate` gates (cost, target,
  cooldown); `activate` spends, publishes, applies effects.
- **Effect** — the persistent subscriber. `apply(bus)` subscribes its handlers and
  tracks every subscription; `remove()` auto-unsubscribes everything tracked.

Two verbs, deliberately separate: **publish collects** contributions into the
chain; **execute applies** them. Publishing never transforms.

## Binding decisions

These were settled in the design and the bootstrap plan; implementers don't
relitigate them.

| Decision | Consequence |
|---|---|
| No exceptions in the core API | Fallible operations return `rpg::core::Status`; never throw across the API surface; never return success on failure |
| Header-only `rpg::core` | `INTERFACE` CMake target; trivial FetchContent / Unreal-plugin vendoring |
| Type erasure via `std::any` | Bus routes `string id -> std::any`; typed `Topic<T>` veneer casts in/out |
| Stages only — no integer priorities | `Chain<T>` takes an ordered stage list; modifiers join a named stage |
| Opaque `SubscriptionId` (wrapped `uint64_t`) | A future C ABI can pass handles across the wire |
| Breakdown in scope for v1 | `Chain<T>::execute` returns the folded value plus per-modifier records |
| Naming | Types `PascalCase`, methods `lowerCamelCase`, headers `snake_case.hpp` under `include/rpg/core/` |
| Synchronous, ordered, fail-fast delivery | Deterministic and debuggable; async is a policy a port may revisit later |

## Per-module docs

- `core/` — all four contracts implemented (v1 complete). Headers:
  `status.hpp`, `bus.hpp`, `chain.hpp`, `topic.hpp` (TopicDef/Topic/ChainedTopic
  — the typed veneer; pointer-form `any_cast` turns type mismatches into
  `Status` errors), `action.hpp` + `entity.hpp`, `effect.hpp`. The composed
  proof: `examples/strike` and its machine-checked twin
  `core/tests/strike_integration_test.cpp` — an Action fires, an Effect
  modifies it, the breakdown names the contributor, removal reverts.
