# Tutorial 05 — Monster Brain

**Goal:** the goblin gets its own hand, its own energy, and a 3-line brain
that picks its best card. The fight becomes a duel — same `Fighter`, same
chains, same rules on both sides. The only difference is the card pool each
draws from and who makes the choices.

**The finished version:**
[`examples/tutorials/05_monster_brain/main.cpp`](../../examples/tutorials/05_monster_brain/main.cpp).
Play first, read second:

```sh
make build
./build/debug/examples/tutorials/05_monster_brain/tutorial_05_monster_brain
```

```
Hero: 20/20 HP, 0 block   Goblin: 30 HP

  energy: 3
  [1] Bash (cost 2, 11 dmg)
  [2] Strike (cost 1, 6 dmg)
  [3] Defend (cost 1, 5 block)
  [4] Salve (cost 1, 4 heal)
  [0] end turn
> 2

  --- Goblin's turn (Goblin: 30 HP) ---
  Goblin plays Claws (cost 0, 6 dmg)
  Claws: 6 damage
  Goblin plays Bite (cost 1, 9 dmg)
  Bite: 9 damage
    block-Hero (block): 9 -> 4
  (Goblin out of energy)
```

Sixteen damage in one turn, your shield absorbed five — the fight is real now.

## What's new (and the one big lesson)

Three additions — a monster card pool, an AI pick function, and a goblin
phase that loops like the player's — and one idea: **symmetry**. The player
and the monster use the same `Fighter` struct, the same `Card` struct, the
same chains. There is no special "monster attack" code. The goblin is
indistinguishable from a player running a script.

Before, `goblinClaws()` was a hardcoded function: "deal 6 damage, always."
Now the goblin has energy, a hand, choices, and variance — it can feel
smarter or dumber depending on the draw. But the *machinery* is identical.

### 1. The monster card pool — a second data table

```cpp
std::vector<Card> monsterCardPool() {
  return {
      {.name = "Claws", .cost = 0, .damage = 6},
      {.name = "Bite", .cost = 1, .damage = 9},
      {.name = "Hide", .cost = 1, .block = 4},
  };
}
```

Same `Card` struct as the player's. Monsters just have different numbers.
Claws is free (always playable), Bite is the heavy hitter, Hide is defense.
A designer edits this table — the card pool IS the goblin's identity.

The player draws from `playerCardPool()`; the goblin draws from
`monsterCardPool()`. The deal function is shared:

```cpp
std::vector<Card> dealHand(const std::vector<Card>& pool, std::mt19937& rng) {
  // picks kHandSize random cards from the given pool
}
```

The pool is a parameter, not a global. This is the same pattern as
tutorial 04's `Card` struct: data drives behavior.

### 2. The goblin phase is now a loop

Where tutorial 04 had:

```cpp
hero.hp -= goblinClaws(hero);
```

Now the goblin phase is its own function, mirroring the player phase:

```cpp
bool goblinPhase(Fighter& goblin, Fighter& hero, std::mt19937& rng) {
  goblin.block = 0;
  std::vector<Card> hand = dealHand(monsterCardPool(), rng);
  std::vector<bool> played(hand.size(), false);
  int energy = kGoblinEnergyPerTurn;

  while (energy > 0) {
    const int pick = aiPick(hand, played, energy);
    if (pick == -1) break;

    const Card& card = hand.at(pick);
    energy -= card.cost;
    played.at(pick) = true;

    // resolve the card — same code the player uses
    if (card.damage > 0) {
      hero.hp -= monsterDamage(card, hero);
    }
    if (card.block > 0) {
      goblin.block += card.block;
    }
  }
  return hero.hp <= 0;
}
```

Structure is identical to `playerPhase`: deal, loop over energy, play cards,
resolve through chains. The only difference is the input source — the player
reads from stdin, the goblin calls `aiPick`.

Notice block resets at the start, same as the hero. The timing rule
("shields expire at start of your turn") applies to everyone.

### 3. The AI pick — the only difference between player and monster

```cpp
int aiPick(const std::vector<Card>& hand, const std::vector<bool>& played, int energy) {
  int best = -1;
  for (std::size_t i = 0; i < hand.size(); ++i) {
    if (played.at(i) || hand.at(i).cost > energy) continue;
    if (best == -1 || hand.at(i).damage > hand.at(best).damage ||
        (hand.at(i).damage == hand.at(best).damage &&
         hand.at(i).block > hand.at(best).block)) {
      best = static_cast<int>(i);
    }
  }
  return best;
}
```

Three rules, read top to bottom:

1. **Prefer damage** — an attack is always better than defense.
2. **Among attacks, prefer bigger** — Bite (9) over Claws (6).
3. **Among equals, prefer block** — if two cards deal the same damage, the
   one with block is the better play.

This is a greedy AI — it always plays the single best affordable card.
Against a real player it's predictable, which is exactly what makes it a
good fight: the player can anticipate. ("The goblin will Bite if it can.")

### 4. What it looks like in motion

Each card the goblin plays prints with the attack's full receipt:

```
  --- Goblin's turn (Goblin: 30 HP) ---
  Goblin plays Claws (cost 0, 6 dmg)
  Claws: 6 damage
  Goblin plays Bite (cost 1, 9 dmg)
  Bite: 9 damage
    block-Hero (block): 9 -> 4
  (Goblin out of energy)
```

The player sees exactly which cards the goblin played, how much damage each
dealt, and how their shield absorbed some of it. Transparency is the whole
point — this isn't a black box.

A 300ms pause between cards (`std::this_thread::sleep_for(kCardDelayMs)`)
keeps the goblin's turn readable. Without it, four cards resolve in a single
frame and the player misses what happened.

### 5. The hero's code didn't change

The player's cards, the player phase, `resolveDamage`, `resolveHeal` — all
identical to tutorial 04. This is the architecture's payoff: adding an
AI-controlled opponent didn't require changing the hero's code.

## Do these before tutorial 06

1. **Tune the goblin's energy.** Change `kGoblinEnergyPerTurn` from 2 to 1,
   then to 3. Play ten turns each. How does the fight feel at each setting?
   What does energy budget communicate about the goblin's personality?

2. **Write a smarter AI.** Make the goblin prefer Hide if its HP is below 10
   (it's scared), or always lead with Bite then fill with Claws (it's
   aggressive). The AI function is 10 lines — changing its personality is
   changing the priority order.

3. **Add a monster card.** `Howl` — cost 0, 0 damage, 3 heal. Goblins heal
   now. Do you also want the AI to prioritize healing when low? (That's a
   second condition in `aiPick`.)

4. **Remove the pause.** Comment out the `sleep_for` line and play a few
   turns. Notice the difference. The pause is "juice," not mechanics — and
   the game works without it.

**Next:** tutorial 06 introduces the Bus and `Effect` — statuses like
Burning that listen for events and act on their own. No more game loop
orchestrating every tick.
