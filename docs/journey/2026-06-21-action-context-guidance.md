# Action Context for Host-Authored Actions — Design Note

**Issue:** https://github.com/KirkDiggler/rpgkit/issues/37
**Date:** 2026-06-21
**Status:** Design / guidance doc. No public API yet — any API work split
into follow-up issues.
**Implements:** implementation slice 2 (action receipt) of the
[minimum observation API](./2026-06-21-minimum-observation-api.md).

**Boundary row:** Authored assets / State mutation / Observations — see
[`docs/architecture/host-boundary.md`](../architecture/host-boundary.md).

## Goal

Define the portable action context — what an `Action` receives when it fires,
what it emits, and what stays host-owned — so a host adapter can resolve
player cards, enemy intents, items, and abilities through one execution path
without turning GameMode into a rules engine. Player cards and enemy intents
are equivalent action sources; the guidance must not bias toward cards.

## Source pressure

| rpgkit-ue use case | What it needs |
|---|---|
| KirkDiggler/rpgkit-ue#10 | Enemy intent data resolves through the same action executor as player cards — no hardcoded goblin attack. |
| KirkDiggler/rpgkit-ue#11 | Authored actions describe target selection rules without embedding targeting in one-off code. |
| KirkDiggler/rpgkit-ue#15 | Add a new action type without a GameMode branch; action works for both a card and an enemy intent. |
| KirkDiggler/rpgkit-ue#17 | A card with multiple ordered actions (damage, block, heal, apply status) resolves each in order. |

## What `Action<TInput>` owns today

```cpp
template <typename TInput>
class Action {
  std::string id_;      // "strike-1", "vulnerable-apply"
  std::string type_;    // "strike", "apply-status"
  Status canActivate(const EntityRef& owner, const TInput& input);
  Status activate(const EntityRef& owner, const TInput& input);
};
```

- **`id` / `type`** — action identity. Already core-owned.
- **`TInput`** — the rulebook's typed input. Core never looks inside it; the
  subclass reads targeting/cost choices from it. Already core-owned as a
  template parameter, host-defined as a concrete type.
- **`EntityRef owner`** — who's acting. Already core-owned.
- **`activate` returns `Status`** — today, that's all. The receipt is
  implementation slice 2 (see "Lifecycle visibility" below).

The contract is deliberately small. The question #37 answers is: **what goes
in `TInput`, what stays on the host, and what's the host's job at the call
site** — not "what new fields does `Action` need."

## The decision: `TInput` is the whole context, and the host owns it

rpgkit does not define an `ActionContext` struct. `TInput` *is* the context,
and the host defines it per rulebook. Core stays out:

| Context concern | Owner | Where it lives |
|---|---|---|
| Actor / owner | **Core** | `EntityRef owner` parameter (already exists) |
| Target(s) | **Host** → passed in `TInput` | The host resolves targeting *before* calling `activate` (see targeting row of [`host-boundary.md`](../architecture/host-boundary.md)). `TInput` carries the resolved `EntityRef`/list, not a target *rule*. |
| Action identity | **Core** | `Action::id()` / `type()` (already exists) |
| Source (what granted this action) | **Host** → in `TInput` | Card id, enemy intent id, item id. The host sets it when constructing the action or the input. |
| Cost / resource state | **Host** → in `TInput` | Energy, mana, card-from-hand. Core's `canActivate` gates on it; the host supplies the current values. |
| Encounter state (turn, phase) | **Host** | Not in `TInput`. The host decides whether to call `activate` at all; core doesn't gate on encounter phase. |
| Actor storage / replication | **Host** | Engine concern. Core sees only the `EntityRef` opaque id. |
| Target selection UI | **Host** | Runs before `activate`; produces the resolved target that goes into `TInput`. |
| Request seams (damage, block, heal) | **Host** | The action calls them during `activate`; they're host-owned per UE#16 and the state-mutation boundary row. |

The test for each row is the same as the host-boundary doc: *would a second
host carry this in the same place?* Actor and identity are core because every
host needs them in the same shape. Everything else is host because its shape
changes per engine or per rulebook.

## Player cards and enemy intents are the same shape

UE#10 and UE#15 both pressure this. The pattern that satisfies both:

```
authored action spec (host DataAsset)        →   Action<TInput> instance
        │                                              │
   type, cost, target rule,                       id, type from spec;
   magnitude, status id, ...                     TInput filled by host
        │                                              │
   ┌────┴────┐                                         │
   │ card    │ enemy intent                            │
   │ source  │ source                                  │
   └────┬────┴────┬───────────────────────────────────┘
        │         │
        ▼         ▼
   shared executor: resolve target → fill TInput → canActivate → activate
```

The **executor** is host code (UE#15 calls it the "shared action executor").
It does, in order:

1. Read the authored action spec (card action or intent action — same shape).
2. Resolve the target rule to concrete `EntityRef`(s) using host actor
   queries + host UI if selection is needed.
3. Build `TInput` with the resolved target, source id, cost values.
4. Call `action.canActivate(owner, input)`; abort on failure.
5. Call `action.activate(owner, input)`; route the receipt (after slice 2).

No branch on "is this a card or an enemy intent." The difference is only
*which authored spec* the host loaded and *who the owner* is — both fed into
the same `TInput`-building step. UE#10's goblin attack becomes an authored
intent spec that goes through the same executor; the hardcoded branch
disappears.

## Composite actions (UE#17)

A card with multiple actions (damage, then block, then apply-status) is
**host-owned sequencing**, not a core `CompositeAction`. The host:

- Holds an ordered list of authored action specs on the card definition.
- Runs the executor for each spec in order, building a fresh `TInput` per
  action (the block action's target is the owner; the status action's target
  may differ).
- Routes each action's receipt (after slice 2) to the HUD/log in order.

Core's `Action` fires once by design ("the transient verb" —
`core/include/rpg/core/action.hpp:13`). A composite is a host loop over
single-fire actions, not a new core type. If a second host also needs
composite actions, the *receipt aggregation* shape might earn a design note —
but the sequencing itself stays host-owned.

## Lifecycle visibility — today vs after slice 2

UE#15 and UE#17 want action lifecycle output for HUD/log/debug. Today, before
the action receipt lands:

- **Started:** the host's executor knows it called `activate`; log
  "strike-1 by hero on goblin-1" from the call site — the host has id, owner,
  and target because it just built `TInput`.
- **Finished:** the host got `Status` back; log success/failure with the
  action id.
- **Per-step (composite):** the host's loop knows which sub-action it's on;
  log each step's id and result.

After slice 2 (`ActionReceipt`), the same facts come from the receipt the
host routes to its sinks. The executor pattern doesn't change — only where
the host reads the facts. This is why UE#15 and UE#17 can ship today on
call-site logs and upgrade to receipts without rewriting the executor or the
action subclass.

## Rough edges that should become future issues

| Friction | Where it belongs | Why |
|---|---|---|
| `TInput` field sprawl as action types grow | **Host-owned** | The host defines `TInput`; splitting it (e.g. `StrikeInput` vs `ApplyStatusInput`) is a host refactor. Core never looks inside. |
| No receipt on `activate` today | **rpgkit#34 slice 2** (already seeded) | The observation API note names this as follow-up #2. |
| Composite action receipt aggregation | **Deferred** — not an issue yet | If a second host needs it, a new design note with that use case. Today the host aggregates by looping. |
| Target rule vocabulary | **Possible future core** — see [`host-boundary.md`](../architecture/host-boundary.md) targeting row | The boundary doc already names this as "core owns the vocabulary, host owns resolution." A follow-up issue opens when UE#11 proves which vocabulary words are portable. |
| Action context observations vs receipts | **Already answered** — receipts, not a sink, per the [observation API note](./2026-06-21-minimum-observation-api.md) | Not a new issue; this note confirms it. |

## Non-goals

- No `ActionContext` struct in core. `TInput` is the context; the host defines it.
- No core `CompositeAction`. Sequencing is a host loop over single-fire actions.
- No core target-rule resolution. The host resolves before `activate`; `TInput`
  carries the resolved target.
- No core encounter-phase gating. The host decides when to call `activate`.
- No core cost/resource types. The host supplies values via `TInput`; core's
  `canActivate` gates on them.
- This note does not add `ActionReceipt` — that's slice 2, its own issue.

## Verification against #37

- [x] A design note explains the host-authored action boundary.
- [x] Player cards and enemy intents are equivalent action sources (§"Player cards and enemy intents are the same shape").
- [x] The guidance calls out what rpgkit deliberately does not own (Non-goals + the context table).
- [x] Any proposed API work is split into follow-up issues (slice 2 already seeded; composite receipts deferred; target vocabulary deferred to its own issue).
