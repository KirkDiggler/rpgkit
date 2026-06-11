# Tutorial 03 — Cards & energy

**Goal:** turn tutorial 02's one-button fight into a deck-builder turn: a
random hand of cards, an energy budget that can't pay for everything, and
real decisions. This is the moment it becomes a *game*.

**The finished version:**
[`examples/tutorials/03_cards_energy/main.cpp`](../../examples/tutorials/03_cards_energy/main.cpp).
Play first, read second:

```sh
make build
./build/debug/examples/tutorials/03_cards_energy/tutorial_03_cards_energy
```

```
  energy: 3
  [1] Strike (cost 1, 6 dmg)
  [2] Bash (cost 2, 11 dmg)
  [3] Salve (cost 1, 4 heal)
  [4] Strike (cost 1, 6 dmg)
  [0] end turn
>
```

Three energy. You can't play everything. Bash now, or double Strike and
keep one energy for a Salve? *That tension is the entire deck-builder
genre*, and it costs about 80 new lines.

## What's new (and what isn't)

Grown from tutorial 02: a `Card` struct, a dealt hand, the energy budget,
and a healing chain. **Unchanged:** `resolveDamage`, `readChoice`, the
goblin's code, the shape of the turn loop. Growing a game should mostly be
*adding* — if every feature rewrites old code, the architecture is fighting
you.

## 1. A card is data, not code

```cpp
struct Card {
  std::string name;
  int cost = 0;
  int damage = 0;
  int heal = 0;
};
```

A card is a row in a table — exactly how a designer thinks about cards:

```cpp
std::vector<Card> cardPool() {
  return {
      {.name = "Strike", .cost = 1, .damage = 6, .heal = 0},
      {.name = "Bash", .cost = 2, .damage = 11, .heal = 0},
      {.name = "Salve", .cost = 1, .damage = 0, .heal = 4},
  };
}
```

Adding a card to the game = adding a line here. No new code. (A card whose
*behavior* is genuinely new earns a new field — and one place in the play
logic learns to read it.)

## 2. Dealing a hand

```cpp
std::vector<Card> dealHand(std::mt19937& rng) {
  const std::vector<Card> pool = cardPool();
  std::uniform_int_distribution<std::size_t> pick{0, pool.size() - 1};
  std::vector<Card> hand;
  hand.reserve(kHandSize);
  for (int i = 0; i < kHandSize; ++i) {
    hand.push_back(pool.at(pick(rng)));
  }
  return hand;
}
```

Four random picks, duplicates welcome. This is the same die-rolling pattern
as chain_basics' bless, pointed at a list instead of a d4. Randomness is
what makes turn three different from turn two — replayability is variance
you chose on purpose.

## 3. The energy budget — the decision engine

The player phase is a loop-within-the-turn:

```cpp
int energy = kEnergyPerTurn;
while (energy > 0) {
  // show hand, read a choice
  // 0 ends the turn early; a card you can afford gets played
  energy -= card.cost;
}
```

Plus three refusals, each with its own message: not a real card, already
played, can't afford it. Notice the game never *crashes* on a bad choice —
it explains and re-asks. Player-proofing is most of what interactive code
does all day.

## 4. Healing is the same machinery as damage

```cpp
rpg::core::Chain<int> heal(std::vector<std::string>{"base", "cap"});
const int room = std::max(0, target.maxHp - target.hp);
mustBeOk(heal.add("cap", "max-hp-cap", [room](int amount) {
  return amount > room ? room : amount;
}));
```

(The `std::max(0, ...)` clamp matters: if hp could ever exceed maxHp, a
negative `room` would turn healing into damage. Defensive rules in copyable
code earn their keep.)

A healing chain with a cap rule: "you can't heal past full" is not an `if`
buried in the play logic — it's a named rule on the receipt, same as
tough-skin:

```
  Salve: 4 healing
    max-hp-cap (cap): 4 -> 1
```

The player *sees* that 3 healing was wasted. Receipts turn invisible math
into information the player can play around — that's why everything goes
through chains.

## 5. The tuning table grew

```cpp
constexpr int kHeroMaxHp = 20;
constexpr int kGoblinHp = 30;
constexpr int kClawsDamage = 6;
constexpr int kEnergyPerTurn = 3;
constexpr int kHandSize = 4;
```

Five numbers now define the fight's feel. Energy 2 is a starvation game;
energy 5 is a power fantasy; hand size 3 makes draws cruel. Balance lives
here, not in the code below it.

## Do these before tutorial 04

1. **Add a card**: `Smite` — cost 3, 16 damage. One line. Is it ever worth
   a whole turn's energy? Play ten turns and decide like a designer.
2. **Vampiric Strike**: cost 2, 5 damage *and* 3 heal. (No new fields
   needed — the play logic already checks `card.damage > 0` and
   `card.heal > 0` independently. Some cards just fill in both.)
3. **Energy carryover**: let unspent energy carry into next turn, capped at
   5. Does the game get better or worse? (Real answer: it gets *slower* —
   players bank instead of acting. Feel it yourself before trusting us.)

**Next:** tutorial 04 gives the goblin's victims a defense — Block — as a
chain rule, and the hero's `resolveDamage` still won't change.
