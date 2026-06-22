# Minimum Observation API — Design Note

**Issue:** https://github.com/KirkDiggler/rpgkit/issues/34
**Date:** 2026-06-21
**Status:** Design (no public API yet). Follow-up implementation issues cite this note.

**Boundary row:** Observations — see
[`docs/architecture/host-boundary.md`](../architecture/host-boundary.md).

## Goal

Define the smallest portable observation surface rpgkit should own, grounded in
the rpgkit-ue use cases that proved the need, with explicit non-goals so core
doesn't grow a sink/callback framework it doesn't need.

## Source pressure

| rpgkit-ue use case | What it wants to observe |
|---|---|
| KirkDiggler/rpgkit-ue#8 | Structured facts: ActionStarted, TopicPublished, SubscriberInvoked, ModifierAdded, ChainExecuted, EffectApplied, EffectExpired, StateChanged |
| KirkDiggler/rpgkit-ue#9 | Damage breakdown: base, each modifier **with source**, final, block/HP distinction |
| KirkDiggler/rpgkit-ue#14 | Effect apply / tick / expire with target + source + remaining duration |
| KirkDiggler/rpgkit-ue#15 | Action lifecycle output for HUD/log/debug |
| KirkDiggler/rpgkit-ue#16 | Typed request seam for state mutations (host convention unless multi-host) |

## What core owns today

- `Chain<T>::execute` already returns `Result{value, breakdown}` where each
  `Step{id, stage, before, after}` names the contributing modifier. This is
  the **prototype receipt** — the only existing observation surface, and a
  first-class output by design (binding decision 6), not a debug afterthought.
- `Bus::publish` is synchronous, fail-fast, returns `Status`. It exposes
  nothing about which subscribers ran.
- `Effect::apply`/`remove` return `Status`. Subscription tracking is internal.
- `Action::activate` returns `Status`. No receipt.

## The design decision: receipts, not a sink

rpgkit owns **receipts** — structured records returned from its operations. It
does **not** own a sink, observer interface, callback registration, or
correlation-id generator. The host pulls receipts out of return values and
routes them to its own sinks (UE logs, HUD, debug tools).

This follows the
[promotion rule](../architecture/host-boundary.md#promotion-rule-host-friction--rpgkit-issue):

- **Concrete.** UE#8/#9/#14/#15 all fail today because return values are
  `Status`-only; the host can't explain what happened without re-reading code.
- **Portable.** A receipt shape is engine-agnostic. A sink/callback would
  carry threading, lifetime, and ordering policy that belongs to the host
  (UE's game thread, Unity's main loop, a headless test harness).
- **Repeatable.** Five UE use cases hit the same gap.

A sink would force core to answer "who registers, when, in what order, on what
thread, with what lifetime?" — all host questions. A receipt answers none of
them and gives the host a structured fact to route however it wants.

## Minimum vocabulary

Mapping UE#8's wishlist to core-owned vs host-owned:

| Event UE#8 wants | Core-owned? | Why |
|---|---|---|
| ActionStarted / ActionFinished | **Yes — as a receipt** from `Action::activate` | Core owns the action lifecycle; today it returns `Status`, losing the fact. |
| ChainExecuted | **Yes — already owned** | `Chain<T>::Result` is the receipt. |
| ModifierAdded | **Yes — already owned** | `Chain<T>::Result::breakdown` steps. Gaps: no source label (see UE#9). |
| EffectApplied / EffectExpired | **Yes — as a receipt** from `Effect::apply`/`remove` | Core owns the lifecycle; today it returns `Status`. |
| TopicPublished | **No — host-owned** | The host called `publish`; it already has the topic + payload. Core adds nothing. |
| SubscriberInvoked | **No — host-owned** | Fail-fast delivery is deterministic; a host that wants this subscribes a tap handler itself. Exposing it would leak subscription identity into core. |
| EffectExpired (duration) | **No — host-owned** | Core has no "tick" or duration concept. Host drives `turn.ended`; effect removal is host-initiated. Core only receipts the removal when asked. |
| StateChanged | **No — host-owned** | Core has no state types (HP/block/energy). UE#16 explicitly says request seams stay adapter convention unless multi-host. |

So the **minimum** is three receipt extensions to existing return values — no
new top-level type, no sink, no registration:

### 1. `Chain<T>::Step` gains optional source metadata (UE#9)

Today a `Step` names the modifier `id` (e.g. `"vulnerable"`) but not what
granted it. UE#9 needs "Vulnerability status" as the source label.

```cpp
struct Step {
  std::string id;       // modifier id, already present
  std::string stage;    // already present
  std::string source;   // NEW: effect source that added this modifier ("" if none)
  T before;
  T after;
};
```

`Chain::add` already takes `id`; the caller (the Effect subscribing into the
chain) knows its `source` and passes it. Core just carries it through.

### 2. `Action::activate` returns a receipt (UE#15)

`activate` moves to a params struct (binding decision 9) so the signature
stays stable as later fields land:

```cpp
struct ActivateParams {
  const EntityRef& owner;
  const TInput& input;
  std::string correlationId = "";  // caller-supplied; defaulted member
};
struct ActionReceipt {
  std::string id;            // action id
  std::string type;          // action type
  std::string correlationId; // echoed from ActivateParams
};
// activate returns (Status, ActionReceipt) — receipt populated even on
// failure so the host can log "action X failed at gate Y" with X's identity.
std::pair<Status, ActionReceipt> activate(ActivateParams params);
```

Call sites use designated initializers; callers that don't correlate omit
`correlationId`:

```cpp
// no correlation — host just wants the receipt
auto [st, receipt] = action->activate({.owner = goblin, .input = strikeInput});

// host correlates this activation with the card play that caused it
auto [st2, correlated] = action->activate(
    {.owner = goblin, .input = strikeInput, .correlationId = "card-play-7"});
```

`correlationId` is **caller-supplied**, not core-generated. The host sets it
when it kicks an action (e.g. from the card-play or enemy-intent entry point)
and **passes the same id explicitly to every core operation it wants
correlated** — the `activate` call, the `publish` whose chain it wants
explained, the `Effect::apply` the action triggered. Core only copies the id
it is handed onto that operation's receipt; it never propagates an id across
operations on its own. The host reconstructs
`action → topic → chain → effect → state` from the receipts it holds, all
tagged with the id it chose. Core never allocates ids, never tracks them
across calls, and has no thread-local "current action" context.

### 3. `Effect::apply`/`remove` returns a receipt (UE#14)

Both move to params structs (binding decision 9). `apply` keeps its `Bus&`
argument as a struct field; `remove` gains a struct solely because
`correlationId` is coming and a zero-arg method can't grow a parameter
without reshaping its signature:

```cpp
struct ApplyParams {
  Bus& bus;
  std::string correlationId = "";
};
struct RemoveParams {
  std::string correlationId = "";
};
struct EffectReceipt {
  std::string id;            // effect id
  std::string source;        // effect source
  std::vector<SubscriptionId> subscriptions;  // what was tracked
  std::string correlationId; // echoed from ApplyParams/RemoveParams
};
std::pair<Status, EffectReceipt> apply(ApplyParams params);
std::pair<Status, EffectReceipt> remove(RemoveParams params = {});
```

Call sites:

```cpp
auto [applied, applyReceipt] = effect->apply({.bus = bus});
auto [removed, removeReceipt] = effect->remove({.correlationId = "card-play-7"});
```

The host gets "Bleed applied by card Strike-1, subscribed to turn.ended" as
a fact, not by re-reading `effect.hpp`.

## Correlation model

- **No global correlation id generator in core.** The host supplies an opaque
  string at action-invocation time and **passes the same id explicitly to each
  core operation it wants correlated** — the `activate` call, any `publish`
  whose chain it wants explained, any `Effect::apply`/`remove` the action
  triggered. Core copies the id onto the receipt of the operation it was
  handed; it does not propagate an id across operations, has no registry, and
  no thread-local "current action" context.
- Core does **not** correlate across actions. "This tick of Bleed came from
  the card that applied it three turns ago" is a host reconstruction job —
  the host has the apply receipt with the correlation id it set; the tick is a
  `turn.ended` publish the host also initiated.
- `correlationId` defaults to `""`. Hosts that don't care pay zero cost.

This keeps core free of: a registry, a thread-local "current action" context,
an id allocator, and any cross-call state. Every correlation fact is either on
the receipt the host just received, or derivable from two receipts the host
already holds.

## Explicit non-goals

- **No sink, observer interface, or callback registration.** Hosts route
  receipts themselves.
- **No `StateChanged` event.** State types are host-owned (UE#16). Core
  receipts describe *core* operations, not host state.
- **No `SubscriberInvoked` exposure.** A host that wants it subscribes a tap.
- **No `TopicPublished` receipt.** The host called `publish`; it has the fact.
- **No duration/tick concept in `Effect`.** Time is host-driven; core only
  receipts the apply/remove the host asked for.
- **No global correlation id.** Caller-supplied, default empty.
- **No new top-level `Observation` type.** Receipts are operation-specific
  (`Chain::Result`, `ActionReceipt`, `EffectReceipt`). A single union type
  would invite speculative events; per-operation receipts keep the surface
  honest.

## Smallest implementation slice

Three follow-up rpgkit issues, each verified back in rpgkit-ue:

1. **Chain source metadata** — extend `Chain<T>::Step` with `source`, thread
   it through `add`/`execute`. The params-struct reshape (`AddParams`,
   `ActivateParams`, `ApplyParams`, `RemoveParams`) landed in #47; this
   slice only adds the `source` defaulted member and the `Step.source`
   field. Verify in UE#9: the damage breakdown widget names "Vulnerability"
   as the modifier source without a hardcoded string.
2. **Action receipt** — `Action::activate` returns `(Status, ActionReceipt)`.
   The `ActivateParams` struct already exists (#47); this slice adds the
   `correlationId` defaulted member and changes the return type. Verify in
   UE#15: a new action type produces HUD/log output from the receipt, no
   GameMode branch added.
3. **Effect receipt** — `Effect::apply`/`remove` return
   `(Status, EffectReceipt)`. The `ApplyParams`/`RemoveParams` structs
   already exist (#47); this slice adds `correlationId` defaulted members
   and changes the return types. Verify in UE#14: apply/tick/expire display
   reads from receipts, not `effect.hpp` internals.

No issue is opened for a sink, observer, or `StateChanged`. If a second host
(Unity via C ABI) later needs the same receipts, that confirms the shape; if
it needs a sink, that's a new design note, not an extension of this one.

## Resolved questions

Both were implementation-slice questions, not design-note blockers. They are
answered in the [chain receipt source metadata note](./2026-06-21-chain-receipt-source-metadata.md#answers-to-the-34-open-questions)
and applied consistently across slices 1-3:

1. **`(Status, Receipt)` vs `StatusOr<T>`.** `(Status, Receipt)` — `Status`
   first to match the existing `activate`/`apply`/`remove` signatures, receipt
   populated even on failure so the host can log "action X failed at gate Y"
   with X's identity. No `StatusOr<T>` template is introduced.
2. **`correlationId` as `std::string` vs opaque wrapper.** `std::string` —
   caller-supplied and host-defined; serializes, logs, and compares trivially.
   If a second host needs a non-string id, that's a new design note.
