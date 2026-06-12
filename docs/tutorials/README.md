# rpgkit tutorials

A step-by-step path from "I have a computer" to "I built a working combat
game in the terminal." Each tutorial adds one idea to the same growing game.

**You do not need to be a programmer.** If you can type commands into a
terminal and edit a text file, you can follow these. Every step shows the
exact command to run and what you should see. When code appears, the words
around it explain what it means in game-design terms, not programmer terms.

## The series

| # | Tutorial | You end up with |
|---|----------|------------------|
| 01 | [Getting started](01-getting-started.md) | A working setup: rpgkit built, tests passing, an example running |
| 02 | [Your first terminal game](02-your-first-terminal-game.md) | A playable fight: hero vs goblin, damage with receipts, win/lose |
| 03 | [Cards & energy](03-cards-and-energy.md) | A hand of cards and an energy budget — the deck-builder skeleton |
| 04 | [Block](04-block.md) | Defense that absorbs damage — and the first rule that spends a resource |
| 05 | A smarter monster *(coming)* | The goblin gets its own cards and a simple brain |
| 06 | Statuses that stick around *(coming, needs engine v1)* | Bleed, Strength — effects that listen and act on their own |

## How the tutorials work

- The **finished code for each tutorial** lives in
  [`examples/tutorials/`](../../examples/tutorials/) — for example, tutorial
  02's game is `examples/tutorials/02_game_base/`. You can run it
  immediately, or build your own copy alongside it and compare.
- The project's robots rebuild every tutorial's code on every change, so the
  tutorials can't silently go stale.
- Each tutorial **extends the previous one's game**. You can always start
  from the previous checkpoint instead of your own code.

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
