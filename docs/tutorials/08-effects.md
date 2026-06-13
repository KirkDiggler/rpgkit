# Tutorial 08 — Effects

**Goal:** an Effect is an autonomous subscriber. It subscribes when applied,
acts on schedule, and self-removes when done. The game loop goes from
orchestrator to announcer — it publishes `turn.ended` and lets effects respond.

**The finished version:**
[`examples/tutorials/08_effects/main.cpp`](../../examples/tutorials/08_effects/main.cpp).
Play first, read second:

```sh
make build
./build/debug/examples/tutorials/08_effects/tutorial_08_effects
```

```
Hero: 20/20 HP, 0 block   Goblin: 30 HP

  energy: 3
  [1] Strike (cost 1, 6 dmg)
  [2] Rend (cost 2, 3 bleed)
  [3] Defend (cost 1, 5 block)
  [4] Bash (cost 2, 11 dmg)
  [0] end turn
> 2
  Bleed applied to Goblin (3 stacks)
  out of energy.

— Turn 1 complete —
  Shields expire.
  Bleed deals 3 damage to Goblin

--- Goblin's turn (Goblin: 27 HP) ---
```

New card: **Rend** (cost 2, 3 bleed). It doesn't deal damage — it applies a
BleedEffect that ticks 3 damage per turn and fades when its stacks run out. The
game loop doesn't manage Bleed at all. It just publishes `turn.ended`.

## The architectural shift

Before this tutorial, the game loop was the orchestrator:

```
Game loop:  "Goblin takes 6 damage. Tick bleed. Check death."
```

After this tutorial, the game loop is the announcer:

```
Game loop:  "turn.ended happened." (one publish)
BleedEffect: "I heard that — tick, deal damage, maybe remove myself."
```

The loop never checks "is Bleed active?" — it just publishes and lets effects
respond. The `activeEffects` list is held in `main()`, but the loop only prunes
it after each turn. Effect state is internal to the effect.

## What's new

### 1. The Effect base class

`rpg::core::Effect` provides the lifecycle:

```cpp
class Effect {
 public:
  Effect(std::string id, std::string source);

  Status apply(Bus& bus);   // calls onApply, tracks subscriptions, marks active
  Status remove();           // unsubscribes everything tracked, marks inactive

  bool isActive() const;

 protected:
  virtual Status onApply(Bus& bus) = 0;  // you override this
  void track(SubscriptionId id);          // register a subscription for cleanup
};
```

Three things happen when you call `apply(bus)`:

1. `onApply(bus)` runs — your subclass subscribes to topics and calls `track()`
   on every `SubscriptionId` it gets back.
2. If `onApply` fails, every tracked subscription is rolled back and the effect
   stays inactive.
3. If `onApply` succeeds, the effect is marked active. It will stay active
   until `remove()` is called.

`remove()` does the reverse: it unsubscribes every tracked subscription and
marks the effect inactive. You don't pass the bus — it remembers which bus it
was applied on. This means you can never unsubscribe from the wrong bus.

### 2. BleedEffect — an autonomous subscriber

```cpp
class BleedEffect : public rpg::core::Effect {
 public:
  BleedEffect(Fighter& target, int stacks)
      : Effect(makeId(target.name), "rend"), target_(target), stacks_(stacks) {}

 protected:
  Status onApply(Bus& bus) override {
    std::cout << "  Bleed applied to " << target_.name
              << " (" << stacks_ << " stacks)\n";
    track(turnEndedTopic().on(bus).subscribe(
        [this](const int& /*turn*/) { return onTurnEnd(); }));
    return Status::ok();
  }

 private:
  Status onTurnEnd() {
    std::cout << "  Bleed deals " << stacks_ << " damage to "
              << target_.name << '\n';
    target_.hp -= stacks_;
    --stacks_;
    if (stacks_ == 0) {
      std::cout << "  Bleed faded\n";
      return remove();
    }
    return Status::ok();
  }

  Fighter& target_;
  int stacks_;
};
```

BleedEffect subscribes to exactly one topic: `turn.ended`. When a turn ends,
`onTurnEnd()` fires — it deals `stacks` damage, decrements, and if stacks hit
zero, calls `remove()` which unsubscribes itself from the bus. The game loop
never calls `onTurnEnd()` and never checks `stacks`.

The effect ID is `"bleed-Goblin-1"`, `"bleed-Goblin-2"`, etc. — unique per
application. This matters for challenge 4 (Double Rend).

### 3. The Rend card — applying an effect

```cpp
struct Card {
  std::string name;
  int cost = 0;
  int damage = 0;
  int heal = 0;
  int block = 0;
  int bleed = 0;  // new: applies this many Bleed stacks
};
```

When a card has `bleed > 0`, the player phase creates a BleedEffect and
applies it:

```cpp
if (card.bleed > 0) {
  auto effect = std::make_unique<BleedEffect>(goblin, card.bleed);
  mustBeOk(effect->apply(bus));
  activeEffects.push_back(std::move(effect));
  return false;  // no death check — bleed doesn't deal damage immediately
}
```

Bleed doesn't deal damage the turn you play it. It ticks on `turn.ended` — the
first tick happens after the goblin's turn, when the loop publishes the event.

The card pool changes slightly:

```cpp
{.name = "Strike", .cost = 1, .damage = 6},
{.name = "Rend", .cost = 2, .bleed = 3},    // replaces Salve
{.name = "Defend", .cost = 1, .block = 5},
{.name = "Bash", .cost = 2, .damage = 11},
```

Salve (heal) leaves the card pool. Rend takes its slot. This keeps the hand
size at 4 cards and gives the player a reason to think about timing.

### 4. activeEffects — held, not managed

```cpp
std::vector<std::unique_ptr<rpg::core::Effect>> activeEffects;
```

The game loop holds this list but never manages effect state. After publishing
`turn.ended`, it prunes self-removed effects:

```cpp
++turnNumber;
mustBeOk(turnEndedTopic().on(bus).publish(turnNumber));
std::erase_if(activeEffects, [](const auto& e) { return !e->isActive(); });
```

That's the only interaction with the list. The loop doesn't iterate effects,
doesn't call any effect methods, doesn't check stacks. Effects own their
lifecycle.

### 5. The announcer shift — one publish, zero orchestration

In tutorial 07, the game loop published `turn.ended` and subscribers responded
(combat log, shield expiry). But the loop still "orchestrated" some things —
block was cleared inline, damage resolution still called a function.

Now the loop publishes `turn.ended` and *that's it*. Bleed ticks because it
subscribed. Shield expiry happens because it subscribed. The combat log prints
because it subscribed. The loop's only job after the goblin's turn is:

```cpp
++turnNumber;
mustBeOk(turnEndedTopic().on(bus).publish(turnNumber));
std::erase_if(activeEffects, [](const auto& e) { return !e->isActive(); });
```

Three lines. Every effect, every future feature — Regen, Poison, Enrage —
just subscribes. The loop never changes.

## What did NOT change

- **Tough-skin** is still a subscriber on `combat.damage` — added in tutorial
  07, unchanged here.
- **Shield expiry** is still a subscriber on `turn.ended` — added in tutorial
  06, unchanged here.
- **The Bus** from tutorial 06 is the same object. Effects use it to subscribe
  and unsubscribe.
- **Damage resolution** via `DamageAttempt` and the chained topic is unchanged
  from tutorial 07. The `"effects"` stage in `damageStages()` is still empty
  here — that's challenge 1.
- **Healing stays `Chain<int>`** — no effect modifies healing yet.

## Do these before tutorial 09

1. **Damage amp.** Make BleedEffect also subscribe to `combat.damage` — when
   someone hits the Bleeding target, add `+stacks * 2` damage at the "effects"
   stage. This is the "full Bleed" from the original combined tutorial: the
   effect subscribes to two topics (one notification, one chained) and
   contributes to the damage chain. The `"effects"` stage in `damageStages()`
   has been empty since tutorial 07 — this is what it's for.

   ```cpp
   track(damageTopic().onChained(bus).subscribe(
       [id = id(), this](const DamageAttempt& event,
                          rpg::core::Chain<DamageAttempt>& chain) {
         if (event.target != target_.name) {
           return rpg::core::Status::ok();
         }
         return chain.add("effects", id, [bonus = stacks_ * kBleedAmp](
                                              DamageAttempt data) {
           data.baseAmount += bonus;
           return data;
         });
       }));
   ```

   Add it in `onApply()` alongside the `turn.ended` subscription. The receipt
   will now show bleed contributing damage:

   ```
     Strike: 6 damage
       bleed-Goblin-1 (effects): 6 -> 12
       tough-skin (final): 12 -> 11
   ```

   Early hits sting more because the stacks decrease each turn.

2. **Poison (damage-up tick).** Like Bleed, but tick damage *increases* each
   turn (1, 2, 3...) instead of decreasing. One-line change to `onTurnEnd`: a
   `tick_` counter that starts at 1 and increments rather than a `stacks_`
   counter that starts at N and decrements. New card: "Envenom" (cost 2,
   applies Poison). Which feels more satisfying — Bleed or Poison?

3. **Regen.** Heals the hero each turn. Same lifecycle as Bleed (subscribe on
   apply, self-remove at zero), but the effect is positive. New card:
   "Regenerate" (cost 2, applies Regen 3). What does `onTurnEnd` look like
   when it heals instead of harms?

4. **Double Rend.** What happens if you play Rend twice before the first Bleed
   expires? Two BleedEffect instances with different IDs (`bleed-Goblin-1` and
   `bleed-Goblin-2`). The goblin takes double tick damage. Does that feel right?
   Test it and see.

5. **Stacking (replace on re-apply).** Change Rend so playing it again refreshes
   Bleed to full stacks instead of creating a second instance. Search
   `activeEffects` for an existing Bleed on the target and call a `refresh()`
   method on it. What would `refresh()` need to do — just reset `stacks_`, or
   does it need to re-subscribe?

**Next:** tutorial 09 migrates card-playing logic into Action contracts — a
card becomes data, and the game loop becomes "look up the action, feed it
input, fire."