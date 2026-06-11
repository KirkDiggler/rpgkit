# Recipe: build a damage chain with a breakdown receipt

**Goal:** several game effects each adjust one damage number, in a guaranteed
order, without knowing about each other — and you get a receipt showing who
did what.

**Working example:** [`examples/chain_basics/`](../../examples/chain_basics/main.cpp)
— complete and runnable; new chain code starts as a copy of it.

## The pattern, in five moves

```cpp
#include "rpg/core/chain.hpp"
#include "rpg/core/status.hpp"
```

**1. Declare the stage order once.** This list is the only place ordering
lives. Modifiers in earlier stages always apply before later ones.

```cpp
rpg::core::Chain<int> damage(std::vector<std::string>{"base", "effects", "final"});
```

**2. Add modifiers: (stage, unique id, function).** The function takes the
value and returns the new value. The id must be unique in the chain — adding
the same id twice returns an error (that's how "can't rage twice" works).

```cpp
rpg::core::Status s = damage.add("effects", "rage", [](int dmg) { return dmg + 2; });
if (!s.isOk()) { /* handle s.message() */ }
```

Every `add`/`remove` returns a `Status`. You cannot silently ignore it — the
compiler diagnoses a dropped Status (an error under -Werror builds).

**3. Dice and other "fresh every time" values go INSIDE the function.** The
chain stores the function and calls it on every execute, so this re-rolls
per execution:

```cpp
std::mt19937 rng{std::random_device{}()};
damage.add("effects", "bless", [&rng](int dmg) {
  std::uniform_int_distribution<int> d4{1, 4};
  return dmg + d4(rng);
});
```

**4. Execute with the starting value; read value + receipt.**

```cpp
const rpg::core::Chain<int>::Result result = damage.execute(5);
// result.value           -> final number
// result.breakdown       -> vector of Steps: {id, stage, before, after}
```

**5. Remove by id when an effect ends.**

```cpp
damage.remove("rage");  // Status; error if "rage" isn't in the chain
```

## Expected output (from chain_basics)

```
Strike! base damage 5
  rage (effects): 5 -> 7
  bless (effects): 7 -> 10
  vulnerable (final): 8 -> 16     <- bless line varies: it's a real d4
final damage: 16
```

## Rules of thumb

- `T` can be any copyable type: `Chain<int>` for a bare number,
  `Chain<Damage>` for a struct with fields. Modifiers receive a copy and
  return the new value — never try to mutate shared state from inside one.
- One chain per resolution: build it, fill it, execute, let it go out of
  scope. Don't keep a chain across turns; effects re-register on the next
  resolution (the event/topic layer automates this — coming in the typed
  topic recipes).
- Stage names are your rulebook's vocabulary. Pick readable ones
  (`"block-absorption"` beats `priority: 75`).

## Gotchas

- A lambda that captures locals by reference (`[&rng]`) must not outlive
  them. Inside one function like these examples: always fine. Storing chains
  or modifiers longer-term: capture by value (`[=]` or `[rng]`) or think
  about lifetimes.
- `add` to a stage that isn't in the stage list is an error (typo guard) —
  check the Status.
