# Tutorial 07 — Rich Events

**Goal:** events carry context. A chained topic lets subscribers contribute
modifiers before resolution completes. This is the plumbing that makes Effects
possible — but we build it without any Effects yet.

**The finished version:**
[`examples/tutorials/07_rich_events/main.cpp`](../../examples/tutorials/07_rich_events/main.cpp).
Play first, read second:

```sh
make build
./build/debug/examples/tutorials/07_rich_events/tutorial_07_rich_events
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
```

The game still plays identically to tutorial 06. But the receipts now show
`baseAmount` instead of bare `int` — and tough-skin is a subscriber on the
Bus, not an inline lambda. This is the stepping stone: if tough-skin can
subscribe, so can Bleed.

## The architectural shift

Before tutorial 07, damage was a bare number:

```cpp
Chain<int> damage({"base", "final"});
damage.add("final", "tough-skin", [](int dmg) { return dmg - 1; });
```

The chain didn't know *who* was being hit. Tough-skin reduced damage by 1,
but only when it was in the chain — player card damage had it, monster damage
didn't. The difference was implicit (which resolution function you called),
not explicit (a field on the event).

After tutorial 07:

```cpp
DamageAttempt attempt{.target = "Goblin", .baseAmount = 6};
Chain<DamageAttempt> chain(damageStages());
damageTopic().onChained(bus).publish(attempt, chain);
auto result = chain.execute(attempt);
```

Damage is now an event — a `DamageAttempt` that carries who's being hit.
Subscribers see the target and decide whether to modify the amount. The chain
folds through stages, and the receipt shows the full breakdown.

This is the same pattern tutorial 08 uses for Bleed — the only difference is
that Bleed is a subscriber registered by an `Effect`, not a static lambda in
`main()`.

## What's new

### 1. DamageAttempt — damage carries context

```cpp
struct DamageAttempt {
  std::string target;
  int baseAmount;
};
```

Two fields. `target` is who's being hit ("Goblin" or "Hero"). `baseAmount` is
the raw damage before any modifiers. Every damage resolution creates one and
pushes it through a chain — the same way `Chain<int>` worked before, but now
subscribers can read the target.

### 2. combat.damage — a chained topic

```cpp
const rpg::core::TopicDef<DamageAttempt>& damageTopic() {
  static const rpg::core::TopicDef<DamageAttempt> kDef{"combat.damage"};
  return kDef;
}
```

Two kinds of topics, two purposes:

- **Notification topics** (`Topic<T>`) — like `turn.ended` from tutorial 06.
  Publish to announce, subscribe to observe. No return value, no
  transformation.
- **Chained topics** (`ChainedTopic<T>`) — like `combat.damage`. Publish to
  collect modifiers, subscribe to contribute. The subscriber gets the event
  *and* a reference to the chain, and it calls `chain.add()` to register a
  modifier.

The `on()` method binds a `TopicDef` as a notification topic; `onChained()`
binds it as a chained topic. Same `TopicDef`, different access patterns.

### 3. damageStages() — shared order of operations

```cpp
std::vector<std::string> damageStages() {
  return {"base", "effects", "final"};
}
```

Three stages. The "effects" stage is empty in this tutorial — no active
effects yet. But it's there so Bleed (tutorial 08) has a place to contribute
`+stacks * 2` damage. The stages are declared once and shared across every
damage resolution.

### 4. Tough-skin — subscriber, targeted by context

In tutorial 06, tough-skin was:

```cpp
damage.add("final", "tough-skin", [](int dmg) { return dmg - 1; });
```

It was baked into the player's damage chain and only affected strikes against
the goblin. Now it's a subscriber that checks the target:

```cpp
damageTopic().onChained(bus).subscribe(
    [&goblin](const DamageAttempt& event,
       rpg::core::Chain<DamageAttempt>& chain) {
      if (event.target != goblin.name) {
        return rpg::core::Status::ok();
      }
      mustBeOk(chain.add("final", "tough-skin", [](DamageAttempt data) {
        data.baseAmount = std::max(0, data.baseAmount - 1);
        return data;
      }));
      return rpg::core::Status::ok();
    });
```

Same rule (reduce damage by 1), same outcome (only the goblin has tough skin),
but now the subscriber *reads the target* and decides whether to contribute.
This is the key advantage over `Chain<int>`: subscribers can make targeted
decisions based on who's being hit.

### 5. The resolution flow — publish, collect, execute

```cpp
int resolveCardDamage(const Card& card, const std::string& targetName,
                      rpg::core::Bus& bus) {
  rpg::core::Chain<DamageAttempt> chain(damageStages());
  const DamageAttempt attempt{.target = targetName, .baseAmount = card.damage};
  mustBeOk(damageTopic().onChained(bus).publish(attempt, chain));
  const auto result = chain.execute(attempt);
  printDamageReceipt(card.name, card.damage, result);
  return result.value.baseAmount;
}
```

Three steps:

1. **Create** a fresh chain with the shared stages.
2. **Publish** to the chained topic — subscribers add their modifiers.
3. **Execute** the chain — fold through stages, get the receipt.

Step 2 is the new piece. When `publish()` is called, every subscriber on
`combat.damage` gets the `DamageAttempt` event and a reference to the chain.
Each subscriber decides whether to contribute — tough-skin checks `event.target`
and only activates for the goblin. When `publish()` returns, the chain has all
contributions. Then `execute()` folds through the stages and produces the
receipt.

### 6. Block — still inline, but in the new shape

Block is a per-resolution modifier that needs to read and mutate the hero's
current shield state. It's added to the chain *before* `publish()`:

```cpp
if (hero.block > 0) {
  mustBeOk(chain.add("final", "block-" + hero.name, [&hero](DamageAttempt data) {
    const int absorbed = std::min(hero.block, data.baseAmount);
    hero.block -= absorbed;
    data.baseAmount -= absorbed;
    return data;
  }));
}
```

This is inline because block is tied to the hero's mutable state — it
absorbs damage and reduces the shield in one step. Tough-skin doesn't need
mutable state, so it subscribes. Block does, so it's inline. The boundary
between "subscriber" and "inline" is whether you need mutable state that
changes per-resolution.

### 7. Receipt format — baseAmount instead of bare int

The receipt now shows `baseAmount` transformations:

```
  Strike: 6 damage
    tough-skin (final): 6 -> 5
```

Same numbers as tutorial 06, but the receipt type changed from
`Chain<int>::Step` (which showed bare ints) to `Chain<DamageAttempt>::Step`
(which shows `baseAmount`). The `target` field is available in every step
for subscribers that need it.

## What did NOT change

- **No Effects, no Bleed, no Rend.** This tutorial moves plumbing; it doesn't
  add gameplay.
- **Healing stays `Chain<int>`.** No effect modifies healing yet, so it doesn't
  need the richer event type.
- **The Bus from tutorial 06 is unchanged.** `turn.ended` still works the
  same way. Shield expiry and combat log are still subscribers.
- **Cards and goblin AI are the same.** The player's hand and the goblin's
  behavior haven't changed.

## Do these before tutorial 08

1. **Targeted tough-skin.** Add an `attacker` field to `DamageAttempt`:
   ```cpp
   struct DamageAttempt {
     std::string attacker;
     std::string target;
     int baseAmount;
   };
   ```
   Change tough-skin to only reduce damage against the goblin — the goblin has
   thick skin, not the hero. The subscriber reads `event.target`:
   ```cpp
   if (event.target != "Goblin") return rpg::core::Status::ok();
   ```
   This demonstrates that rich events let subscribers make targeted decisions.

2. **Critical hit.** Subscribe to `combat.damage` and add a 10% chance to
   double damage at the "effects" stage. The receipt now shows a "critical"
   step. Proof that any subscriber can inject modifiers — not just
   status effects.

3. **Healing as a topic.** Migrate `resolveHeal` from `Chain<int>` to a
   chained topic `combat.heal` with a `HealAttempt` struct. Same pattern as
   damage — once you've done it once, the second time is familiar.

**Next:** tutorial 08 introduces the Effect base class and BleedEffect — an
autonomous subscriber that subscribes when applied, ticks on `turn.ended`, and
self-removes when its stacks hit zero. The game loop goes from orchestrator to
announcer.