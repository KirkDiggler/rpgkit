# rpgkit tutorials

A step-by-step path from "I have a computer" to "I built a working combat
game in the terminal." Each tutorial adds one idea to the same growing game.

**You do not need to be a programmer.** If you can type commands into a
terminal and edit a text file, you can follow these. Every step shows the
exact command to run and what you should see. When code appears, the words
around it explain what it means in game-design terms, not programmer terms.

## Getting started

| # | Tutorial | You end up with |
|---|----------|------------------|
| 01 | [Getting started](01-getting-started.md) | A working setup: rpgkit built, tests passing, an example running |
| 02 | [Your first terminal game](02-your-first-terminal-game.md) | A playable fight: hero vs goblin, damage with receipts, win/lose |

## Pick your path

| The Campaign | The Workshop |
|---|---|
| Build a game, feature by feature | Understand the engine |
| [03: Cards & Energy](03-cards-and-energy.md) → ... | [W01: The Bus](workshop/w01-bus.md) → ... |
| Start here to make something fast | Start here to see how it works |

> **Juice appendix** ([juice.md](juice.md)) — terminal polish available from
> either path at any checkpoint.

---

### The Campaign

Build a complete deck-builder game, one feature at a time. Starts where
tutorial 02 left off; no engine internals required.

| # | Tutorial | You end up with |
|---|----------|------------------|
| 03 | [Cards & energy](03-cards-and-energy.md) | A hand of cards and an energy budget — the deck-builder skeleton |
| 04 | [Block](04-block.md) | Defense that absorbs damage — the first rule that spends a resource |
| 05 | [Monster brain](05-monster-brain.md) | The goblin gets its own cards and a simple brain |
| 06 | [Statuses that stick around](06-statuses.md) | Bleed — an Effect that listens, ticks, and removes itself |
| 07 | Cards become Actions *(coming)* | Card-playing logic migrates into Action contracts |
| 08 | The town crier *(coming)* | A combat announcer — UI is just another subscriber |
| 09 | Boss fight *(coming)* | A phase-based boss using the full nervous system |

---

### The Workshop

Architecture deep-dives for the engineer who wants to understand how rpgkit
works and how to build custom rules on top of it. Rejoins the Campaign at
tutorial 06.

| # | Module | What you learn |
|---|--------|----------------|
| W01 | [The Bus](workshop/w01-bus.md) | Events flow through a bus — publishers and subscribers never meet |
| W02 | [Chains & Stages](workshop/w02-chains.md) | Staged pipelines with a named breakdown of every contributor |
| W03 | [Rage Dissected](workshop/w03-rage-dissected.md) | A guided reading of D&D 5e Rage in rpg-toolkit |
| W04 | [Build Your Own Effect](workshop/w04-build-your-own-effect.md) | Implement a Burning status using the Effect contract |

---

## How the tutorials work

- The **finished code for each tutorial** lives in
  [`examples/tutorials/`](../../examples/tutorials/) — for example, tutorial
  02's game is `examples/tutorials/02_game_base/`. You can run it
  immediately, or build your own copy alongside it and compare.
- The Workshop examples live in [`examples/workshop/`](../../examples/workshop/).
  CI rebuilds everything on every change, so nothing goes stale.
- Each Campaign tutorial **extends the previous one's game**. You can always
  start from the previous checkpoint instead of your own code.

## The one idea underneath everything

In rpgkit, damage (or healing, or anything numeric) is never just
subtracted. It travels through a **chain** — a pipeline of rules — and comes
out with a **receipt** showing what every rule contributed:

```
  Strike: 6 damage
    tough-skin (final): 6 -> 5
```

Rules never know about each other; the pipeline composes them. When you add
Block in tutorial 04, the Strike code from tutorial 02 doesn't change at
all. That's the entire design philosophy, and you'll feel it by tutorial 03.
