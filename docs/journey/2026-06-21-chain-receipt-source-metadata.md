# Chain Receipt Source Metadata — Design Note

**Issue:** https://github.com/KirkDiggler/rpgkit/issues/35
**Date:** 2026-06-21
**Status:** Design (no public API yet). Follow-up implementation issue cites this note.
**Implements:** implementation slice 1 of the
[minimum observation API](./2026-06-21-minimum-observation-api.md).

**Boundary row:** State mutation / Observations — see
[`docs/architecture/host-boundary.md`](../architecture/host-boundary.md).

## Goal

Pin down the exact metadata a `Chain<T>::Step` receipt carries so a host can
explain "base damage, then Vulnerable doubled it, then block absorbed 3"
without recreating explanation data outside core. Answer the open questions
the observation API note left for slice 1.

## Source pressure

| rpgkit-ue use case | What it needs from the chain receipt |
|---|---|
| KirkDiggler/rpgkit-ue#9 | Base, each modifier **with source label**, final, block/HP distinction — no per-card hardcoded string. |
| KirkDiggler/rpgkit-ue#14 | Effect identity on the modifier so apply/tick/expire line up with the damage step that caused them. |
| KirkDiggler/rpgkit-ue#6 | Authoring a card that applies a status — the status's effect must be traceable back to the card that granted it. |

## What `Step` carries today

```cpp
struct Step {
  std::string id;     // modifier id, e.g. "vulnerable" or "block-hero"
  std::string stage;  // stage name, e.g. "final"
  T before;
  T after;
};
```

`id` is the dedup key and the breakdown label. It's already stable across
executes (the same effect re-adding on every publish uses the same id). What
it does **not** carry: who granted the modifier, what kind of thing it is,
or any host-facing label.

## The decision: add `source`, nothing else

One new field. No label, no category, no opaque host blob.

```cpp
struct Step {
  std::string id;      // modifier id (unchanged)
  std::string stage;   // stage name (unchanged)
  std::string source;  // NEW — effect source that added this modifier, "" if none
  T before;
  T after;
};
```

`Chain::add` moves to a **params struct** so the signature stays stable as
later slices add fields (binding decision 9). `source` is a defaulted member
— callers that don't have one omit the field and the receipt row gets an
empty source, which the host renders as "innate" or omits.

```cpp
struct AddParams {
  std::string_view stage;
  std::string_view id;
  Modifier modifier;
  std::string_view source = "";  // defaulted member, not a positional default
};
Status add(AddParams params);
```

Call sites use C++20 designated initializers, so a caller with an effect in
hand names every field and a caller with no effect simply omits `source`:

```cpp
chain.add({.stage = "final", .id = "rage", .modifier = [](int v){ return v + 2; }});
chain.add({.stage = "final", .id = "bleed", .modifier = bleedMod,
           .source = effect.source()});
```

A future receipt field (slice 2/3 style) is another defaulted member on
`AddParams` — the signature never reshapes and call sites that don't name
the new field still compile. Contrast with the positional form
`(stage, id, source, modifier)`: inserting `source` before `modifier` breaks
the C++ default-argument rule (defaults only skip *trailing* params), so
every existing `chain.add(...)` call site would need `""` inserted before
the lambda. The params struct sidesteps that entirely.

## Why only `source`, not label/category/opaque metadata

The #35 design questions list four candidates. Each rejected here:

| Candidate | Verdict | Why |
|---|---|---|
| **Stable id** | Already owned | `id` is already the stable dedup key. Adding a second id splits identity and breaks dedup. |
| **Human label** | Host-owned | "Vulnerability" vs `"vulnerable"` is a presentation choice. Core carrying a label means core localizes, core formats, core decides label collisions. The host already maps `id`/`source` to a display string for its asset table — that's one lookup, not a core field. |
| **Category** | Host-owned | "buff vs debuff", "status vs equipment", "magic vs physical" — every category scheme is rulebook-specific. Core picking one bakes in a rulebook; core picking none means the field is unused. The host has this from its authored asset and can join on `source`. |
| **Opaque host metadata** | Rejected | A `std::any`/`void*` on `Step` invites hosts to shove engine state through core and back out. It also makes `Step` non-copyable in practice (any_cast lifetime) and breaks the "core is system-agnostic" boundary. If a host needs more than `id`+`source`, it joins on `source` against its own asset table — that's what UE does today. |

The test for each candidate: *would a second host (Unity via C ABI, headless
test harness) carry the same field for the same reason?* `source` passes
(every host wants "what granted this"). Label/category/opaque fail — they
exist because UE wants them, and UE already has them outside core.

## How this interacts with future observation events

The [observation API note](./2026-06-21-minimum-observation-api.md) defines
receipts for Action and Effect that also carry `correlationId` and `source`.
This keeps `Step.source` consistent with them:

- `ActionReceipt.correlationId` — caller-supplied, threaded by the host.
- `EffectReceipt.source` — the effect's own `source()` (already a core field).
- `Step.source` — the `source` the modifier was added with, which for an
  effect-subscribed modifier is the effect's `source()`. The host can join
  a chain step to the effect that caused it by string equality on `source`.

No new join keys. No core-side correlation registry. The host holds the
`EffectReceipt` from apply, the `ActionReceipt` from activate, and the
`Chain::Result` from execute, and joins them on `source` + `correlationId`
it supplied.

## Answers to the #34 open questions

Both open questions from the observation API note get resolved here so #36
and #37 inherit the answer:

1. **`(Status, Receipt)` vs `StatusOr<T>`.** `Chain::execute` already returns
   `Result` by value (it can't fail). `add`/`remove` return `Status` only.
   The pattern that fits core's no-exception style: **operations that can
   fail return `Status`; operations that can't fail return their receipt by
   value.** Action/Effect receipts (which come from operations that *can*
   fail) return `(Status, Receipt)` — `Status` first to match the existing
   `activate`/`apply`/`remove` signatures, receipt populated even on failure
   so the host can log "action X failed at gate Y" with X's identity. No
   `StatusOr<T>` template is introduced; it would be one shape used three
   times, and `(Status, Receipt)` keeps the failure path identical to today.
2. **`correlationId` as `std::string` vs opaque wrapper.** `std::string`.
   It's caller-supplied and host-defined; a wrapper would reserve room for a
   future non-string id but no host has asked for one, and a string already
   serializes, logs, and compares trivially. If a second host needs a
   non-string id, that's a new design note — not a retrofit onto `Step`.

## Smallest implementation slice

One rpgkit issue, verified back in rpgkit-ue:

1. **Extend `Chain<T>::Step` with `source`; thread it through `add`/`execute`.**
   - Test plan (TDD, per AGENTS.md):
     - New `chain_test.cpp` case: `add` with a `source` populates
       `Step.source`; `add` without keeps `Step.source == ""` (defaulted
       member).
     - New `chain_test.cpp` case: two modifiers with the same `id` from
       different sources still reject (dedup is by `id`, not `source`).
      - Existing `chain_test.cpp` cases and every `chain.add(...)` call site
        across `examples/` and `tests/` already use designated initializers
        (the params-struct reshape landed in #47). This slice only adds
        `.source = ...` to the call sites that have one.
      - `strike_integration_test.cpp` updated to pass `effect.source()` from
        the bleed subscriber; assertion that the breakdown step's `source`
        matches the effect's source.
   - Verification in rpgkit-ue#9: the damage breakdown widget renders
     "Vulnerability" as the modifier source by reading `Step.source` and
     joining on it — no per-card hardcoded string, no GameMode branch.

No issue is opened for label, category, or opaque metadata. If a second host
joins on `source` and still can't explain a result, that's a new design note
with a concrete second-host use case — not an extension of this one.

## Non-goals

- No label, category, or opaque host metadata on `Step`.
- No formatting hooks in core. The host formats the breakdown.
- No new join key besides `source` (and the caller-supplied `correlationId`
  on Action/Effect receipts).
- No `StatusOr<T>` template.
- No change to dedup semantics — `id` remains the dedup key; `source` is
  metadata only.
