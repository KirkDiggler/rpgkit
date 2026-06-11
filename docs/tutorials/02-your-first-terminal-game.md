# Tutorial 02 — Your first terminal game

**Goal:** a fight you can actually play — a hero, a goblin, choices, damage
receipts, and a winner. This game is the base every later tutorial extends.

**The finished version** lives at
[`examples/tutorials/02_game_base/main.cpp`](../../examples/tutorials/02_game_base/main.cpp).
Play it right now:

```sh
make build
./build/debug/examples/tutorials/02_game_base/tutorial_02_game_base
```

```
A goblin blocks your path!

Hero: 20 HP   Goblin: 18 HP
[1] Strike   [0] Run away
>
```

Type `1`, press enter, and watch the receipt. Win (or lose). Then come back
and we'll walk through how it works, piece by piece. ~140 lines, and you
will understand all of them.

## The pieces of every game ever made

The whole file is five small pieces. In game terms:

1. **Character sheets** — who's fighting and their numbers
2. **A safe way to listen to the player** — input that survives nonsense
3. **The damage pipeline** — hits go through rules, receipts come out
4. **The moves** — what Strike and Claws actually do
5. **The turn loop** — show, ask, resolve, check for an ending

## 1. The tuning table and the character sheets

```cpp
constexpr int kHeroHp = 20;
constexpr int kGoblinHp = 18;
constexpr int kStrikeDamage = 6;
constexpr int kClawsDamage = 4;
```

Every game number, named, in one place at the top — `constexpr int` just
means "a number with a name that never changes while the game runs."
Balancing the fight means editing these four lines. (The `k` prefix is a
C++ convention for "constant.")

```cpp
struct Fighter {
  std::string name;
  int hp = 0;
};
```

A `struct` is a character sheet: named values that travel together. Making
two fighters reads exactly like you'd hope:

```cpp
Fighter hero{.name = "Hero", .hp = kHeroHp};
Fighter goblin{.name = "Goblin", .hp = kGoblinHp};
```

Later tutorials add `energy` and `block` lines to this sheet. That's all
growth ever looks like here.

## 2. Listening to the player (without trusting them)

```cpp
int readChoice() {
  int choice = 0;
  while (!(std::cin >> choice)) {
    if (std::cin.eof()) {
      return 0;
    }
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cout << "  numbers only — try again: ";
  }
  return choice;
}
```

`std::cin >> choice` means "read a number the player typed." The rest is
armor: if they type `potato`, the read fails, and without the cleanup lines
the program would re-fail on the same `potato` forever. So: clear the error,
throw the bad line away, ask again. Every interactive program you write will
have a cousin of this function. Steal it freely.

## 3. The damage pipeline (the rpgkit part)

```cpp
int resolveDamage(const std::string& attackName, int baseDamage,
                  const rpg::core::Chain<int>& chain) {
  const rpg::core::Chain<int>::Result result = chain.execute(baseDamage);
  std::cout << "  " << attackName << ": " << baseDamage << " damage\n";
  for (const rpg::core::Chain<int>::Step& step : result.breakdown) {
    std::cout << "    " << step.id << " (" << step.stage << "): " << step.before << " -> "
              << step.after << '\n';
  }
  return result.value;
}
```

A `Chain` is a pipeline of rules. `execute(6)` pushes a 6 in one end; every
rule in the pipeline adjusts it; and the result comes out with a
**breakdown** — a list of steps, each saying *which rule* (`step.id`),
*when* (`step.stage`), and *what it did* (`before -> after`). The `for`
line just reads that list out loud to the player.

## 4. The moves

```cpp
int heroStrikes() {
  rpg::core::Chain<int> damage(std::vector<std::string>{"base", "final"});
  mustBeOk(damage.add("final", "tough-skin", [](int dmg) { return dmg - 1; }));
  return resolveDamage("Strike", kStrikeDamage, damage);
}
```

Read it as a sentence: *build a fresh pipeline with two phases ("base" then
"final"); install the goblin's tough-skin rule into the "final" phase; push
a Strike (6 damage, from the tuning table) through and narrate it.*

The `[](int dmg) { return dmg - 1; }` part is the rule itself — a tiny
machine that takes the damage and gives back damage-minus-one. The name for
this is a *lambda*; think "a rule written inline."

Why a **fresh** pipeline per swing? Because rules come and go — a Bless
lasts three turns, a Block lasts one. Building the pipeline at the moment of
each hit, from whatever rules apply *right now*, is what keeps that simple
forever. (The engine takes care of making this cheap.)

The goblin's `goblinClaws()` is the same shape with no rules installed —
the hero has no defenses. Yet. Tutorial 04 installs Block here, and
*nothing else in the game changes*. That's the payoff of the pipeline.

## 5. The turn loop

```cpp
while (true) {
  // show the world
  std::cout << hero.name << ": " << hero.hp << " HP   ...";
  std::cout << "[1] Strike   [0] Run away\n> ";

  // ask the player
  const int choice = readChoice();
  if (choice == 0) { /* flee ending */ }
  if (choice != 1) { /* "no such move", ask again */ }

  // resolve the turn
  goblin.hp -= heroStrikes();
  if (goblin.hp <= 0) { /* victory ending */ }

  hero.hp -= goblinClaws();
  if (hero.hp <= 0) { /* defeat ending */ }
}
```

`while (true)` means "repeat forever — until something inside says stop."
Show, ask, resolve, check. Chess is this loop. Slay the Spire is this loop.
This goblin fight is this loop with small numbers.

## Do these before tutorial 03

No checking the answers in the finished file — change things and rebuild
(`make build`, then run; the loop from tutorial 01):

1. **Balance pass:** the goblin always loses (do the math on the receipts —
   why?). Change one number in the tuning table so the fight gets dangerous.
2. **New move:** add `[2] Heavy Strike` — 10 damage. (Three places need a
   change: the menu text, the `choice` handling, and a new move function.
   Copying `heroStrikes` is the right instinct, not cheating.)
3. **Crueler goblin:** give the goblin's claws a rule like tough-skin —
   maybe `"frenzy"` that *adds* 2 in the `"final"` stage. Watch it appear
   on the receipt without touching any other code.

If all three work, you've already done real game programming: state,
rules, and a loop. **Tutorial 03** turns the menu into a hand of cards with
an energy budget — the deck-builder begins.
