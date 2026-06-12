# Tutorial 06 — Statuses that Stick Around

**Goal:** the game loop stops orchestrating and starts announcing. A Bus
connects everything; Bleed is a real `rpg::core::Effect` that subscribes to
turn events, amplifies damage, ticks down, and removes itself — all without
the game loop managing it.

**The finished version:**
[`examples/tutorials/06_statuses/main.cpp`](../../examples/tutorials/06_statuses/main.cpp).
Play first, read second:

```sh
make build
./build/debug/examples/tutorials/06_statuses/tutorial_06_statuses
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

  --- Goblin's turn (Goblin: 30 HP) ---
  ... (goblin plays cards) ...

Hero: 14/20 HP, 0 block   Goblin: 30 HP
  Bleed deals 3 damage to Goblin
  Bleed faded
```

Rend doesn't deal direct damage — it applies a status. Three turns later,
Bleed ticks for 3, then 2, then 1, then fades. Each tick also makes any
hit against the goblin hurt more. The game loop never once checked "is
Bleed active?" — it just published `turn.ended` and let the Effect respond.

## The architectural shift

This is the biggest conceptual step in the series. Before tutorial 06:

```
Game loop:      "Goblin takes 6 damage. Tick bleed. Check death."
                "Hero takes 9 damage. Tick bleed. Check death."
```

After tutorial 06:

```
Game loop:      "turn.ended happened." (one publish)
Bleed effect:   "I heard that — tick, deal damage, maybe remove myself."
```

The game loop goes from *orchestrator* to *announcer*. Announcements are
topics on a Bus. Effects subscribe to the topics they care about. The loop
never manages effect state.

## What's new

### 1. The Bus — everything meets here

```cpp
rpg::core::Bus bus;
```

One line. Every subscriber, every topic, every effect connects through this
object. It's passed to `playerPhase`, `goblinPhase`, effect constructors —
everything that needs to publish or listen.

Topics are static facts, defined once:

```cpp
const rpg::core::TopicDef<DamageAttempt>& damageTopic() {
  static const rpg::core::TopicDef<DamageAttempt> kDef{"combat.damage"};
  return kDef;
}

const rpg::core::TopicDef<int>& turnEndedTopic() {
  static const rpg::core::TopicDef<int> kDef{"turn.ended"};
  return kDef;
}
```

Two topics, two types:
- `turn.ended` — a *notification* topic (`Topic<int>`). Subscribe to
  observe, publish to announce. No return value, no transformation.
- `combat.damage` — a *chained* topic (`ChainedTopic<DamageAttempt>`).
  Subscribe to contribute modifiers, publish to collect them.

### 2. DamageAttempt — events that carry context

Before tutorial 06, damage was just a number (`Chain<int>`):

```cpp
int cardDamage(const Card& card) {
  Chain<int> damage({"base", "final"});
  damage.add("final", "tough-skin", [](int dmg) { return dmg - 1; });
  return resolveDamage(card.name, card.damage, damage);
}
```

Now damage is an event that says *who* is being hit, so any subscriber can
decide "do I modify this?":

```cpp
struct DamageAttempt {
  std::string target;  // "Goblin" or "Hero"
  int baseAmount;
};
```

The chain stages are declared once, shared across every resolution:

```cpp
std::vector<std::string> damageStages() {
  return {"base", "effects", "final"};
}
```

### 3. The resolution flow — publish, collect, execute

Instead of building a chain entirely in one function, the resolution now has
three phases:

```cpp
// 1. Create a fresh chain and add rules that always apply (tough-skin).
Chain<DamageAttempt> chain(damageStages());
chain.add("final", "tough-skin", [](DamageAttempt data) {
  data.baseAmount = std::max(0, data.baseAmount - 1);
  return data;
});

// 2. Publish to the chained topic — active effects contribute modifiers.
DamageAttempt attempt{.target = "Goblin", .baseAmount = card.damage};
damageTopic().onChained(bus).publish(attempt, chain);

// 3. Execute: fold through all stages, get the receipt.
auto result = chain.execute(attempt);
```

Step 2 is the new piece. When Bleed is active, its subscriber runs during
`publish` and adds a +6 modifier at the "effects" stage. The chain doesn't
know Bleed exists — it just runs whatever modifiers it has.

### 4. BleedEffect — autonomous, self-cleaning

```cpp
class BleedEffect : public rpg::core::Effect {
 public:
  BleedEffect(Fighter& target, int stacks)
      : Effect("bleed-" + target.name, "rend"), target_(target), stacks_(stacks) {}

 protected:
  rpg::core::Status onApply(rpg::core::Bus& bus) override {
    // Subscribe to turn.ended — tick down and deal DoT damage.
    track(turnEndedTopic().on(bus).subscribe(
        [this](const int&) { return onTurnEnd(); }));

    // Subscribe to the chained damage topic — amplify hits.
    track(damageTopic().onChained(bus).subscribe(
        [id = id(), this](const DamageAttempt& event,
                          rpg::core::Chain<DamageAttempt>& chain) {
          if (event.target != target_.name) return rpg::core::Status::ok();
          return chain.add("effects", id, [bonus = stacks_ * kBleedAmp](DamageAttempt data) {
            data.baseAmount += bonus;
            return data;
          });
        }));
    return rpg::core::Status::ok();
  }
```

Two subscriptions, two concerns:

**Turn subscriber:** on every `turn.ended`, Bleed deals `stacks_` damage to
its target (the DoT), decrements, and at zero calls `remove()` — which
unsubscribes everything automatically.

**Damage subscriber:** when anyone publishes to `combat.damage` targeting
the Bleeding creature, this handler registers a `+stacks_ * kBleedAmp`
modifier at the "effects" stage. The chain handles stage ordering; the
effect just hands the modifier over.

The `Effect` base class manages lifecycle:
- `apply(bus)` → calls `onApply`, sets `isActive`, remembers the bus
- `remove()` → unsubscribes every tracked subscription, clears state
- Subclasses call `track()` in `onApply` to register subscriptions for
  automatic cleanup

This means **no game loop code ever touches Bleed's state.** The loop just
calls `turnEndedTopic().publish(turnNumber)` and lets effects respond.

### 5. The Rend card — applies status instead of damage

```cpp
{.name = "Rend", .cost = 2, .bleed = kBleedStacks},
```

Plus a new field on `Card`:
```cpp
int bleed = 0;  // applies this many Bleed stacks
```

And `playCard` handles it separately from damage/heal/block:

```cpp
bool playCard(const Card& card, Fighter& hero, Fighter& goblin,
              std::vector<std::unique_ptr<Effect>>& activeEffects, Bus& bus) {
  if (card.bleed > 0) {
    auto effect = std::make_unique<BleedEffect>(goblin, card.bleed);
    mustBeOk(effect->apply(bus));
    activeEffects.push_back(std::move(effect));
    return false;
  }
  // damage, heal, block — unchanged
}
```

Rend makes a new Effect and pushes it onto the encounter's active-effects
list. From there, the Bus drives it — the game loop never touches it again.

### 6. What the loop looks like now

```cpp
int turnNumber = 0;
while (true) {
  hero.block = 0;
  // Player phase (may apply Bleed, deal damage, etc.)
  const PhaseResult result = playerPhase(hero, goblin, activeEffects, rng, bus);

  // Announce — the only game-loop interaction with effects.
  ++turnNumber;
  mustBeOk(turnEndedTopic().on(bus).publish(turnNumber));

  // Check death from Bleed ticks
  if (goblin.hp <= 0) { /* victory */ }

  // Goblin phase
  goblinPhase(goblin, hero, rng, bus);
}
```

One publish. That's the entire game loop's responsibility toward effects.
The tick, the amp, the self-removal — all happen inside `BleedEffect`'s
subscribers.

## Do these before tutorial 07

The exercises this time are about exploring what effects make possible:

1. **Strength buff.** Add a Strength effect: +3 to all outgoing damage, lasts
   2 turns. New card: "Focus" (cost 1, applies Strength 2 to the hero). The
   effect subscribes to `combat.damage` and checks `event.attacker == hero`
   (you'll need to add an `attacker` field to `DamageAttempt`).

2. **Poison (damage-up tick).** Like Bleed, but the tick damage *increases*
   each turn (1, 2, 3...) instead of decreasing. This is a one-line change
   to `onTurnEnd`. Play with both versions — which feels more satisfying?

3. **Regen.** Heals the hero each turn. New card: "Regenerate" (cost 2,
   applies Regen 3). The effect subscribes to `turn.ended` and heals HP.
   Same lifecycle as Bleed, different effect.

4. **Double Rend.** What happens if you play Rend twice before the first
   Bleed expires? The `Effect` base class allows multiple instances with
   different ids (`bleed-Goblin` vs `bleed-Goblin-2`). Does the goblin take
   double tick damage? Does the amp stack? Test it and see.

5. **Stacking (replace on re-apply).** Change Rend so playing it again
   refreshes Bleed to full stacks instead of creating a second instance.
   You'll need to search `activeEffects` for an existing Bleed and call
   `refresh()` on it.

**Next:** tutorial 07 migrates the remaining card-playing logic into proper
`Action` contracts — making cards fully data-driven. After that, adding a
new card never means new code.
