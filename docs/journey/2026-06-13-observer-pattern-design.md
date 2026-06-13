# Observer Pattern Design

_Date: 2026-06-13_
_Status: Design approved, awaiting implementation plan_

## Why

Game designers spend hours playtesting to find out if a card is too strong,
a rule is broken, or a matchup is lopsided. They shouldn't have to. rpgkit's
observer pattern lets you attach listeners to every event, chain execution,
and effect lifecycle — no redirects, no special logging calls, no interrupting
your game flow. Subscribe to what you care about, get structured data out, run
your numbers. Your game defines the rules. rpgkit makes them observable.

## Architecture

An `Observer` interface hooks into Bus, Chain, and Effect lifecycles. While the
game runs, those mechanisms are already producing data — they fire events,
execute chains with breakdowns, subscribe and expire effects. An observer
doesn't change any flow; it just receives a copy of data that's already flowing.

### Three observation points

1. **Bus events** — any event published on any topic. "Turn ended", "damage
   attempted", "card played" — whatever the game defines.
2. **Chain results** — every time a chain executes, the observer gets the
   breakdown (base value, which modifiers fired, what they changed, the final
   value).
3. **Effect transitions** — when an effect subscribes, expires, or is removed.

### Principles

- **Observer is a pure receiver.** It never changes the outcome. Watch, don't
  interfere.
- **Opt-in, not all-or-nothing.** A game can hook the whole observer or just
  the parts it cares about. Virtual methods have default empty implementations
  — override only what you need.
- **Zero overhead when not attached.** The Bus, Chain, and Effect don't know
  about specific observers. They call through if one is wired up. No observer,
  no cost.
- **Games provide their own implementations.** A JSON logger, a metrics
  aggregator, a debug stepper, or a terminal that counts win rates — the engine
  doesn't care. The game owns what it does with the data.

### Interface sketch

```cpp
class Observer {
public:
    virtual ~Observer() = default;

    virtual void onBusPublish(const std::string& topic, const std::any& event) {}
    virtual void onChainExecute(const std::string& chain, /* breakdown type TBD */) {}
    virtual void onEffectLifecycle(const Effect& effect, Lifecycle transition) {}
};
```

- `Lifecycle` is an enum: `Subscribed`, `Expired`, `Removed`.
- Default empty implementations mean games override only what they care about.
- The `breakdown` parameter type is TBD — it needs to be generic enough for
  `Chain<T>` across all `T`, but rich enough to be useful. This is an
  implementation detail to resolve during planning.

## Scope

### In scope

- `Observer` interface in `rpg::core` with the three hooks
- Wiring points in Bus, Chain, and Effect to call through to an observer when
  one is attached
- A tutorial that adds observability to the demo game with JSON output
- Zero-overhead guarantee when no observer is attached

### Out of scope

- Built-in Observer implementations (JSON logger, CSV exporter, metrics
  aggregator) — these come later from watching real usage
- Visualization / dashboards — deferred until summaries become walls of text
- Simulation harness or `Steppable` interface — the Bus drives flow; no
  artificial loop needed
- Data schema for exported metrics — emerges from what the demo game actually
  produces
- Any game-specific concepts (cards, decks, turns) in core — the boundary rule
  holds

## Incremental path

1. **Observer interface + wiring** → structured data starts flowing
2. **Game devs explore their data** → patterns emerge, summary questions arise
3. **Built-in aggregators or exporters** → when walls of text need compression
4. **Visualization concepts package** → when text summaries aren't enough

We do not rush stages. Each stage's output tells us what the next stage needs.

## Tutorial integration

After the Effects tutorial lands (tutorial 08), a future tutorial adds an
Observer to the demo game that logs every bus event and chain breakdown to JSON.
By the end, the dev has a playable game that also produces structured data.

The north star: a game designer defines cards, turn flow, and win conditions
using rpgkit primitives. They attach an Observer. They run their game. They get
data. They iterate on game design, not plumbing.

## Decisions made

| # | Decision | Rationale |
|---|----------|-----------|
| 1 | Observer pattern, not Steppable | Bus already ticks; flow is event-driven. Artificial step() fights the grain. |
| 2 | Wide Observer, not specific Metrics | Too early to pick what to measure. Let game devs explore first. |
| 3 | No built-in implementations yet | YAGNI. Ship the hook, watch what devs build, then extract common patterns. |
| 4 | Virtual interface with empty defaults | Opt-in, not all-or-nothing. Override only what you need. |
| 5 | Zero overhead when unattached | Observability should never cost anything if you don't use it. |