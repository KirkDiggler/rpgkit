# Host-Created Runtime Effect Lifecycle — Design Note

**Issue:** https://github.com/KirkDiggler/rpgkit/issues/36
**Date:** 2026-06-21
**Status:** Design / pattern doc. No public API yet — any API work split into
follow-up issues.
**Implements:** implementation slice 3 (effect receipt) of the
[minimum observation API](./2026-06-21-minimum-observation-api.md).

**Boundary row:** State mutation / Observations — see
[`docs/architecture/host-boundary.md`](../architecture/host-boundary.md).

## Goal

Give a host adapter developer one repeatable pattern for turning authored
effect data into a runtime `Effect`, subscribing it, observing its lifecycle,
and removing it safely — without copying one-off demo glue. Separate what
rpgkit `Effect` owns from what the host owns, and name the rough edges that
should become future core issues.

## Source pressure

| rpgkit-ue use case | What it needs |
|---|---|
| KirkDiggler/rpgkit-ue#6 | Author a card that applies Vulnerable/Bleed from a DataAsset — no card-specific GameMode branch. |
| KirkDiggler/rpgkit-ue#7 | Add a new effect by following the same pattern as existing effects — localized code, no copied GameMode logic. |
| KirkDiggler/rpgkit-ue#14 | HUD/log shows effect apply, tick, expire with target + source + duration. |
| KirkDiggler/rpgkit-ue#19 | A fresh workshop user follows docs to create/configure a status and verify its lifecycle. |

## The pattern

A status effect has two lives that must stay separate:

```
authored spec (host-owned data)   →   runtime Effect instance (rpgkit-owned contract)
        │                                    │
   DataAsset / JSON                     subclass of rpg::core::Effect
   magnitude, duration,                 id, source, onApply subscribes,
   stacks, target rule,                 remove() unsubscribes everything tracked
   display text, icon                   apply()/remove() return (Status, EffectReceipt)*
```

\* Receipt shape is the one defined in the
[observation API note](./2026-06-21-minimum-observation-api.md); see
"Lifecycle visibility" below for what exists *today* before that lands.

### 1. The authored spec is host-owned

The spec is whatever the host's authoring format produces — a UE `DataAsset`,
a JSON blob, a Lua table. rpgkit never sees it. It carries:

| Field | Owner | Why |
|---|---|---|
| `magnitude`, `duration`, `stacks` | Host | Game-specific numbers; core has no "magnitude" or "duration" type. |
| `targetRule` | Host | Who the effect lands on — see the targeting row of [`host-boundary.md`](../architecture/host-boundary.md). |
| `displayText`, `icon`, `category` | Host | Presentation. Core never localizes or renders. |
| `sourceId` | Host | The authored id of whatever granted this (card id, enemy id). Becomes `Effect.source`. |

The host turns the spec into constructor arguments for the runtime `Effect`
instance. UE#6 and UE#7 both flag "Amount and DurationTurns carry too many
meanings" — that's a host authoring fix (split the spec fields), not a core
change. Core's `Effect(id, source)` takes only identity; everything else is
subclass constructor parameters the host defines.

### 2. The runtime instance is rpgkit-owned

The subclass of `rpg::core::Effect` is where the host's authored values become
runtime behavior. The pattern, matching `examples/strike/main.cpp:74` and
`core/tests/strike_integration_test.cpp:32`:

```cpp
class VulnerableEffect : public rpg::core::Effect {
 public:
  // Constructor takes the authored values the host extracted from the spec.
  VulnerableEffect(std::string source, int multiplier, std::string targetId)
      : Effect("vulnerable-" + targetId, std::move(source)),
        multiplier_(multiplier), targetId_(std::move(targetId)) {}

 protected:
  Status onApply(Bus& bus) override {
    // Subscribe to the chained topic this effect modifies; track() every
    // subscription so remove() cleans them all up.
    track(damageTopic().onChained(bus).subscribe(
        [id = id(), source = source(), mult = multiplier_](
            const DamageEvent& evt, Chain<DamageEvent>& chain) {
          if (evt.target == targetId_) {
            return chain.add("final", id, source, [mult](DamageEvent d) {
              d.amount *= mult;
              return d;
            });
          }
          return Status::ok();
        }));
    return Status::ok();
  }

 private:
  int multiplier_;
  std::string targetId_;
};
```

Two things to notice, both already core-owned and already in the examples:

- **`track()` makes cleanup automatic.** `remove()` unsubscribes everything
  `track()` recorded — the wrong-bus and forgotten-subscription bugs are
  unrepresentable. The host does not write its own unsubscribe loop.
- **`id` includes the target.** `"vulnerable-goblin-1"` dedupes correctly:
  applying Vulnerable to the same target twice doesn't stack (rejected by
  `Chain::add`'s duplicate-id check), and two different targets get two
  different effects. The id scheme is the subclass's job — core only
  guarantees dedup is by `id`.

### 3. Apply, observe, remove

The host's action code does three things, in order:

```cpp
auto effect = std::make_unique<VulnerableEffect>(cardId, mult, targetId);
auto [applied, receipt] = effect->apply(bus);   // (Status, EffectReceipt)*
if (!applied.isOk()) { /* log, undo, etc. */ }
hostEffectRegistry.add(targetId, std::move(effect), receipt);  // host owns lifetime
// ...later, when duration expires or the encounter ends:
auto& effect = hostEffectRegistry.lookup(targetId, "vulnerable");
auto [removed, removeReceipt] = effect.remove();
hostEffectRegistry.remove(targetId, "vulnerable", removeReceipt);
```

\* Today `apply`/`remove` return only `Status`. The `(Status, EffectReceipt)`
shape is implementation slice 3 from the observation API note; the host
follows the same pattern now, just ignoring the missing receipt.

The host owns the **registry** — the map from `(target, effect-id)` to the
runtime instance. Core does not track "which effects are active on which
entity"; that's host state (the host owns game state per the boundary doc).
Core owns only the apply/remove contract and the subscription cleanup.

### 4. Duration and tick are host-driven

`Effect` has no `tick()`, no `duration`, no turn counter. UE#14 wants
"remaining duration where useful" — the host tracks that:

- The host subscribes to `turn.ended` (a topic it defined).
- On each tick, the host decrements its own duration counter for that effect.
- When duration hits zero, the host calls `effect.remove()`.

Core's role is only: `remove()` actually unsubscribes everything, returns
`Status`, and the effect no longer modifies future publishes. The "when to
remove" is host policy.

This is deliberate — it keeps core free of time. A real-time combat game
would tick on a frame timer, a card game on turn end, a play-by-email game on
a server tick. All three use the same `Effect::remove()`.

## What rpgkit owns vs host owns

| Concern | rpgkit `Effect` | Host |
|---|---|---|
| Subscription tracking + cleanup | **owns** (`track()`/`remove()`) | — |
| apply/remove contract + `Status` | **owns** | — |
| Effect identity (`id`, `source`) | **owns** (constructor) | supplies the values |
| Authored spec data | — | **owns** (DataAsset/JSON) |
| Duration, stacks, magnitude | — | **owns** (subclass ctor params) |
| Tick / when to remove | — | **owns** (subscribes `turn.ended`) |
| Effect registry (who's active on whom) | — | **owns** |
| Display text, icon, category | — | **owns** |
| Lifecycle visibility today | — | **owns** (logs from host call sites) |
| Lifecycle receipts (future) | **owns** (`EffectReceipt` per slice 3) | consumes |

## Lifecycle visibility — today vs after slice 3

UE#14 and UE#19 want structured apply/tick/expire visibility. Today, before
the effect receipt lands:

- **Apply:** the host's action code knows it called `effect.apply(bus)` and
  got `Status` back. Log "Vulnerable applied to goblin-1 by card strike-1"
  from the call site — the host has `source`, `id`, and the target it chose.
- **Tick:** the host owns the `turn.ended` handler that decrements duration.
  Log "Bleed ticked goblin-1 for 2 (2 turns left)" from that handler — the
  host has the magnitude and remaining duration because it owns them.
- **Expire:** the host's duration handler calls `effect.remove()`. Log
  "Vulnerable expired on goblin-1" from that call site.

After slice 3 (`EffectReceipt`), the same three facts come from receipts the
host routes to its sinks instead of ad-hoc logs. **The pattern doesn't change
— only where the host reads the facts.** This is why UE#14 and UE#19 can ship
today on manual logs and upgrade to receipts without rewriting the effect
subclass or the registry.

## Rough edges that should become future issues

| Friction | Where it belongs | Why |
|---|---|---|
| `Amount` / `DurationTurns` overloaded across effect specs | **rpgkit-ue** (host authoring) | Core has no spec types; the host splits its own fields. |
| No receipt on apply/remove today | **rpgkit#34 slice 3** (already seeded) | The observation API note already names this as follow-up #3. |
| Effect id scheme is per-subclass | **rpgkit docs only** — not API | A naming guide (include target for per-target dedup, include source for debug) is enough; core picking an id format would bake in a rulebook. |
| No "is this effect already on this target?" helper in core | **Host-owned** | Core's `Chain::add` already rejects duplicates by id. A registry lookup is host state, not core. |
| Stacking rules (refresh duration vs add stacks vs reject) | **Host-owned** | Game-specific policy. Core's dedup-by-id prevents accidental double-application; the host decides what a re-apply *means*. |

None of these justify core API today. Each gets revisited if a second host
hits the same friction.

## Non-goals

- No `Effect::tick` or `Effect::duration` in core. Time is host-driven.
- No core effect registry. "Who's active on whom" is host state.
- No core spec type. The authored data format is host-owned.
- No stacking/refresh policy in core. Game-specific.
- No id-format mandate. The subclass picks its id; core only dedups by it.
- This note does not add `EffectReceipt` — that's slice 3, its own issue.

## Verification against #36

- [x] A docs/journey note explains the host-created runtime effect pattern.
- [x] Authored spec data vs runtime instance state are explicitly separated (§1 vs §2).
- [x] The rpgkit-ue use cases that exposed the need are cited (UE#6/#7/#14/#19).
- [x] rpgkit-ue can use this to validate/adjust its effect creation pattern.
- [x] Rough edges are named with where each one belongs (core / host / already-seeded).
