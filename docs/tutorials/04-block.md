# Tutorial 04 — Block

**Goal:** give the hero a defense. A Defend card raises a shield; the
goblin's claws now resolve through a chain where your block absorbs damage
before HP — and the receipt shows exactly what your shield did for you.

**The finished version:**
[`examples/tutorials/04_block/main.cpp`](../../examples/tutorials/04_block/main.cpp).
Play first:

```sh
make build
./build/debug/examples/tutorials/04_block/tutorial_04_block
```

Play a Defend, end your turn, and watch the goblin's attack hit your shield:

```
  Claws: 6 damage
    block-Hero (block): 6 -> 1
```

Six incoming, five absorbed, one got through. The receipt is why block
*feels* good here: the player sees their Defend card working.

## What's new (and the one big lesson)

Three small additions — a `block` stat, a Defend card, a block rule in the
goblin's damage chain — and one genuinely important idea: **our first rule
with a side effect.** Strike code, heal code, the turn loop: unchanged.
Defense arrived without touching offense.

## 1. The character sheet grows a line

```cpp
struct Fighter {
  std::string name;
  int hp = 0;
  int maxHp = 0;
  int block = 0;  // shield points; absorb damage before HP, expire each turn
};
```

And the expiry rule lives at the top of the player phase:

```cpp
  // Shields don't stack across turns (the Slay-the-Spire rule): whatever
  // block survived the goblin's attack expires now, at the START of your
  // turn — so a fresh Defend still protects you through the goblin's reply.
  hero.block = 0;
```

Why start-of-*your*-turn and not end? Trace a turn: you Defend, the goblin
attacks, your shield absorbs. If block expired at your turn's *end*, Defend
would never protect you from anything. Timing rules like this are where
deck-builders live or die — and it's one line, placed deliberately.

## 2. Defend is a data row (plus one new field)

```cpp
      {.name = "Defend", .cost = 1, .block = kDefendBlock},
```

The `Card` struct gained `int block = 0;`, the play logic gained one
`if (card.block > 0)` branch, and that's the whole card. This is the
promise from tutorial 03 holding up: a card with genuinely new *behavior*
earns a new field, and one place learns to read it.

## 3. The block rule — a modifier that spends a resource

```cpp
int goblinClaws(Fighter& hero) {
  rpg::core::Chain<int> damage(std::vector<std::string>{"base", "block"});
  if (hero.block > 0) {
    mustBeOk(damage.add("block", "block-" + hero.name, [&hero](int dmg) {
      const int absorbed = std::min(hero.block, dmg);
      hero.block -= absorbed;
      return dmg - absorbed;
    }));
  }
  return resolveDamage("Claws", kClawsDamage, damage);
}
```

Look closely: this rule does **two** things. It transforms the value
(damage minus absorbed — the honest, pure part every rule does), and it
**spends the shield** (`hero.block -= absorbed`) — a side effect.

Earlier docs told you modifiers shouldn't touch shared state. Here's the
honest, more precise version of that rule:

- A modifier's *transform* must stay honest — never edit the value other
  rules are folding except by returning your result.
- A modifier *may* own and spend a resource as it applies — block is
  exactly that. It's safe **because a chain lives for one resolution and is
  executed exactly once**, so the spend happens exactly once. (This is why
  "one chain per resolution, build it fresh" has been drilled since the
  first recipe — a chain you kept around and executed twice would
  double-spend the shield.)

Also note the rule only joins the chain when there's block to spend —
`if (hero.block > 0)` — so the receipt only mentions your shield when it
actually did something.

## Do these before tutorial 05

1. **Overshield:** add `Barricade` — cost 2, 9 block. One data row, zero new
   code. Is it better than two Defends? (It isn't — why? Think about hands
   and energy, like a designer.)
2. **Thorns:** when block absorbs damage, deal 1 damage back to the goblin.
   The block rule already has the perfect place for it — you'll need to
   capture the goblin too: `[&hero, &goblin]`.
3. **Break the timing on purpose:** move `hero.block = 0;` to the *end* of
   the player phase, play a few turns, and feel why Defend became a dead
   card. Put it back. (Timing bugs in deck-builders are usually this exact
   mistake in some disguise.)

**Next:** tutorial 05 gives the goblin its own hand of cards and a simple
brain — and the fight becomes a duel.
