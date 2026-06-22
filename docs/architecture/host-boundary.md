# Host-owned vs rpgkit-owned responsibilities

This doc is the practical answer to "does this need belong in rpgkit core,
rpgkit docs, or the host adapter?" It expands the one-paragraph boundary in
[`README.md`](./README.md) into a per-surface table a reviewer can point at.

It exists to keep rpgkit portable. Project 15 — the
[RPGKit Unreal Workshop](https://github.com/users/KirkDiggler/projects/15) board
— is the current customer surface, and `rpgkit-ue` is the adapter that proves
which seams a real engine integration actually needs. The rule is not "core is
minimal" and not "core is generous"; it is **core earns a seam only when host
friction is concrete, portable, and repeatable** — see the three-part
promotion rule below.

## The default split

`rpgkit` owns four portable contracts — **Bus, Chain, Action, Effect** — plus
the breakdown that `Chain<T>::execute` already returns. Everything else is the
host's job by default. The host owns:

- the **encounter loop** (turn order, win/loss, encounter start/stop),
- **authoring assets** (data tables, card definitions, intent data),
- **target selection UI** and any host-side target rule resolution that needs
  engine input,
- **presentation** (HUD, logs, animation, feedback),
- **persistence** (save/load, serialization format),
- and the **mutation entry points** that turn player input into rpgkit calls.

rpgkit owns the *portable part* of a seam only when the host can't write it
without reimplementing the same logic in every engine. The rows below name the
seams Project 15 has put pressure on so far and where each one sits today.

## Per-surface boundary

| Surface | rpgkit owns | host owns by default | proven by |
|---|---|---|---|
| **Targeting** | The authored target-rule *vocabulary* a host maps to ("self", "enemy", "all enemies", "random"). No UI, no actor queries. | Actor queries, selection UI, resolving a rule to concrete actors, multi-actor encounters. | KirkDiggler/rpgkit-ue#11 |
| **Encounter loop** | Nothing yet. Turn order, win/loss, and encounter lifecycle are host policy. | Start/stop encounter, turn advancement, win/loss detection, enemy intent scheduling. | KirkDiggler/rpgkit-ue#13 |
| **Authored assets** | The portable *shape* of action/effect data the host feeds into `Action`/`Effect` construction. No asset format, no editor integration. | Data tables, asset types, editor authoring UX, importing authored data into rpgkit objects. | KirkDiggler/rpgkit-ue#11, #15, #18 |
| **Presentation / UI** | The **breakdown** returned by `Chain<T>::execute` and any future structured observation surface (see KirkDiggler/rpgkit#34). No HUD, no widgets, no rendering. | HUD, combat log, damage breakdown widget, effect tick/expire visuals, animation. | KirkDiggler/rpgkit-ue#8, #9, #14 |
| **Persistence** | Nothing yet. Save/load is host-owned until a portable serialization shape is proven. | Save/load format, slot management, restoring rpgkit objects from host-authored blobs. | (no current pressure) |
| **State mutation** | The **topics** effects subscribe to and the **typed request seam** a host routes mutations through so effects/logs/tools can observe them. No HP, no block, no game state types. | Game state structs, HP/block fields, when to apply a request, validation beyond `Action::canActivate`. | KirkDiggler/rpgkit-ue#16, #18 |
| **Observations** | A *minimum* portable vocabulary + correlation model for what rpgkit did internally (action → topic → chain → effect → state). Design pending in KirkDiggler/rpgkit#34. No logging, no formatting, no engine sinks. | Wiring observations into UE logs, HUD, debug tools; choosing which host-owned events to also emit. | KirkDiggler/rpgkit-ue#8, #14, #15, #16 |

"Nothing yet" is a deliberate answer, not a gap. It means the host owns it
until Project 15 (or a second host) proves the same friction twice.

## Promotion rule: host friction → rpgkit issue

A discovered need becomes rpgkit work only when **all three** are true:

1. **Concrete.** A specific rpgkit-ue issue describes the friction, with a
   workflow that fails or duplicates logic today. No "we might want…".
2. **Portable.** The fix is not Unreal-specific. If the same seam would look
   the same in a Unity or plain-C++ host, it's a candidate. If it only makes
   sense because of UE widgets/assets/editor, it stays host-owned.
3. **Repeatable.** The friction shows up in more than one rpgkit-ue use case,
   *or* a second host would hit it. One-off host convenience does not earn a
   core seam.

When the rule fires, the new rpgkit issue must cite:

- the **rpgkit-ue use case** that proved the need (fully qualified:
  `KirkDiggler/rpgkit-ue#N`), and
- the **boundary row** above it falls under (or a proposed new row if it's a
  new surface).

If a need is concrete but not portable, it's a `rpgkit-ue` issue. If it's
concrete and portable but the friction is one-off, it's host-owned for now and
the rpgkit-ue issue should note the pressure so a future repeat can promote it.

## Verification back into rpgkit-ue

When rpgkit core work lands for a seam, the closing rpgkit PR links the
rpgkit-ue use case, and a follow-up rpgkit-ue change verifies the seam
actually helped — i.e. the original host friction is gone or smaller. This
closes the loop Project 15 is meant to enforce: rpgkit-ue discovers pressure,
rpgkit earns a seam, rpgkit-ue proves the seam worked. No speculative core
API survives without a rpgkit-ue verification step.

## Non-goals

- This doc does not pre-approve any API. Each seam that earns rpgkit work gets
  its own design (rpgkit#34 is the first).
- It does not enumerate every future surface. New rows get added when a
  Project 15 use case lands on a surface not in the table.
- It does not bind a second host. A Unity adapter would re-prove the same rule
  against its own use cases; this table is rpgkit-ue-shaped by design.
