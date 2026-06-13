# Tutorial 06 — The Bus

**Goal:** the game loop stops being the only place that knows what happened. A
Bus connects everything; the loop announces `turn.ended` and subscribers
respond — without the loop managing them.

**The finished version:**
[`examples/tutorials/06_bus/main.cpp`](../../examples/tutorials/06_bus/main.cpp).
Play first, read second:

```sh
make build
./build/debug/examples/tutorials/06_bus/tutorial_06_bus
```

```
Hero: 20/20 HP, 0 block   Goblin: 30 HP

  energy: 3
  [1] Strike (cost 1, 6 dmg)
  [2] Bash (cost 2, 11 dmg)
  [3] Salve (cost 1, 4 heal)
  [4] Defend (cost 1, 5 block)
  [0] end turn
> 1
  Strike: 6 damage
    tough-skin (final): 6 -> 5

  --- Goblin's turn (Goblin: 25 HP) ---
  Goblin plays Claws (cost 0, 6 dmg)
  Claws: 6 damage
  (Goblin out of energy)

— Turn 1 complete —
  Shields expire.

Hero: 14/20 HP, 0 block   Goblin: 25 HP

  energy: 3
  ...
```

New since tutorial 05: the turn-end announcement ("— Turn 1 complete —") and
the shield-expiry message ("Shields expire.") both come from **subscribers**,
not from inline code in the game loop. The loop just publishes one event;
everything else responds.

## The architectural shift

Before the Bus, the game loop managed everything inline:

```cpp
hero.block = 0;    // the loop knew about shields
goblin.block = 0;  // and reset them explicitly
```

After the Bus:

```cpp
turnEndedTopic().on(bus).publish(turnNumber);  // one announcement
```

The loop doesn't reset shields. It doesn't print turn numbers. It publishes
one event, and subscribers handle the rest. The loop goes from **orchestrator**
to **announcer**.

This is the same shift that makes Effects possible — in tutorial 08, Bleed
will subscribe to `turn.ended` and manage its own lifecycle. But we're not there
yet. This tutorial just teaches the Bus and notification topics. The chain is
still `Chain<int>`. Damage resolution hasn't changed. The game plays identically
to tutorial 05, plus turn announcements.

## What's new

### 1. The Bus — everything meets here

```cpp
rpg::core::Bus bus;
```

One line. Every subscriber, every topic, every announcement connects through
this object. It's passed to `playerPhase` and `goblinPhase` — everything that
needs to publish or listen gets the same Bus.

The Bus is opaque on purpose. You don't iterate over its subscribers. You don't
ask it what topics exist. You publish and subscribe through typed topic
definitions, and the Bus routes messages between publishers and subscribers
that never meet.

### 2. Topics — named, typed channels

```cpp
const rpg::core::TopicDef<int>& turnEndedTopic() {
  static const rpg::core::TopicDef<int> kDef{"turn.ended"};
  return kDef;
}
```

A topic definition is a static fact about your game. It has:

- **A name** (`"turn.ended"`) — human-readable, appears in receipts and logs
- **A type** (`int`) — the payload subscribers receive

You define topics once, at namespace scope, with the `static const` pattern.
Two flavors:

- **Notification topics** (`Topic<T>`) — publish to announce, subscribe to
  observe. No return value, no transformation. This is what `turn.ended` is.
- **Chained topics** (`ChainedTopic<T>`) — publish to collect modifiers,
  subscribe to contribute. These come in tutorial 07.

### 3. Subscribing — respond without the loop knowing

The combat log is the simplest subscriber:

```cpp
turnEndedTopic().on(bus).subscribe([](const int& turn) {
  std::cout << "— Turn " << turn << " complete —\n";
  return rpg::core::Status::ok();
});
```

Two things to notice:

1. **The handler returns `Status::ok()`.** Every handler returns a Status. If
   something goes wrong, return an error Status. If everything's fine, return
   `ok()`. The Bus checks for errors during publish and propagates the first
   one.
2. **It prints a message.** Subscribers can do anything — modify state, print
   output, trigger other events. The game loop doesn't know this subscriber
   exists.

The shield-expiry subscriber goes further — it modifies game state:

```cpp
turnEndedTopic().on(bus).subscribe([&hero](const int& /*turn*/) {
  hero.block = 0;
  std::cout << "  Shields expire.\n";
  return rpg::core::Status::ok();
});
```

This resets the hero's block. It captures `hero` by reference — subscribers
are lambdas, they can capture anything in scope. They're just code, not a
special class.

Note: the goblin's block is still cleared at the start of `goblinPhase()`,
same as tutorial 05. `turn.ended` fires after the goblin phase, so clearing
goblin block there would make it expire before the player's next turn. Goblin
block carries through the player phase and resets when the goblin's turn
begins — same timing as before.

Pure flavor, zero mechanics. But it proves the Bus is for more than just combat
logic — anything that needs to respond to "a turn ended" can subscribe.

### 4. Publishing — one announcement, many responses

The game loop's entire interaction with the Bus is one line:

```cpp
++turnNumber;
mustBeOk(turnEndedTopic().on(bus).publish(turnNumber));
```

That's it. The loop doesn't reset hero shields. It doesn't print turn numbers.
It publishes one event and lets subscribers handle the rest. If you add a new
subsystem — enrage timers, regen ticks, UI updates — you add another subscriber.
The loop never changes.

Note the `mustBeOk()` wrapper. `publish()` returns a Status because any
subscriber could return an error. If one does, the Bus stops and the error
propagates up. Fail-fast, same as everywhere else in rpgkit.

### 5. What the loop used to do

In tutorial 05, the main loop reset hero shields directly:

```cpp
hero.block = 0;    // explicit, inline
```

Now that line is gone. A subscriber handles it — the game loop doesn't even
know shields exist, it just announces turns.

The goblin's block is still cleared explicitly at the start of `goblinPhase()`
(the same timing as tutorial 05 — goblin block persists through the player
phase and resets when the goblin's turn begins). Only hero block moved to a
subscriber, because `turn.ended` fires after the goblin phase. Moving goblin
block there would make it expire before the player's next turn, which changes
the timing.

### 6. Bus passed through, not yet used for damage

`playerPhase` and `goblinPhase` both take a `Bus&` parameter now. In this
tutorial they don't use it — the `/*bus*/` parameter name has a comment
instead. That's intentional. The Bus is threaded through so it's available in
tutorial 07, when damage resolution publishes to it.

## Do these before tutorial 07

1. **Enrage.** Subscribe to `turn.ended` — after turn 5, the goblin gets +1
   energy per turn. The subscriber modifies the goblin's state directly; the
   game loop doesn't change.

2. **First blood.** Define a new topic `combat.hit` (`TopicDef<int>`,
   name `"combat.hit"`). Publish it whenever damage resolves (in
   `cardDamage` and `monsterDamage`). Subscribe and print "First blood!" the
   first time either side takes damage, then unsubscribe. (Hint: capture a
   `bool` flag in the lambda and use `SubscriptionId` to unsubscribe.)

3. **Goblin taunts.** Subscribe to `turn.ended` and print a random goblin
   taunt each turn. Use a `std::vector<std::string>` of taunts and
   `std::uniform_int_distribution` to pick one. Pure flavor, zero mechanics
   — but proves the Bus is for more than just combat logic.

**Next:** tutorial 07 migrates damage from `Chain<int>` to
`Chain<DamageAttempt>` — events that carry context (who's being hit) — and
introduces chained topics where subscribers contribute modifiers to a
resolution. The game plays identically; this is a plumbing refactor that makes
Effects possible.