# Linear Tutorials 6-8: Splitting Statuses Into Three Focused Steps

## Problem

Tutorial 06 introduces six major concepts simultaneously:

1. Bus (pub/sub event hub)
2. Topics (notification vs chained)
3. DamageAttempt (rich events with context)
4. Effect base class (onApply, track, remove, lifecycle)
5. BleedEffect (concrete: subscribes to two topics, ticks, self-removes)
6. Orchestrator-to-announcer shift (loop publishes, effects respond)

That's too much for one tutorial. Each concept deserves room to breathe.

## Approach

Split tutorial 06 into three sequential tutorials, each teaching one concept.
Renumber existing tutorials 07+ as 09+.

**Option A — Linear Split** was chosen because:
- Minimal disruption (just expands 06 into 06/07/08)
- Tutorial 05 stays unchanged — it's solid
- Each new tutorial adds exactly one concept
- Progressive challenge design builds naturally

New tutorial map:

| # | Title | One Concept |
|---|-------|-------------|
| 06 | The Bus | Publish/subscribe decouples "what happened" from "who responds" |
| 07 | Rich Events | Events carry context; chained topics let subscribers contribute modifiers |
| 08 | Effects | An Effect is an autonomous subscriber that manages its own lifecycle |

Tutorials 09+ shift up by 2 (old 07 → 09, old 08 → 10, etc.)

## Tutorial 06 — The Bus

### Goal

The game loop stops being the only place that knows what's happening. It announces events; subscribers decide what to do.

### What changes from tutorial 05

- `rpg::core::Bus bus` is introduced — one object, passed everywhere
- `turn.ended` — a `Topic<int>` notification topic published after each turn
- A combat-log subscriber that prints "— Turn N complete —"
- Block reset moves off implicit `hero.block = 0` / `goblin.block = 0` and onto a `turn.ended` subscriber that clears both fighters' block (the explicit resets in player/goblin phase are removed)
- The Bus is passed into both phases and the main loop

### What does NOT change

- Damage chains are still `Chain<int>` — no DamageAttempt yet
- No Effects, no Bleed, no Rend
- Cards are the same as tutorial 05
- The goblin AI is unchanged
- Game plays identically, just with turn-end announcements and cleaner block handling

### Key code additions

```cpp
// Bus — one line, passed everywhere
rpg::core::Bus bus;

// Turn-ended topic
const rpg::core::TopicDef<int>& turnEndedTopic() {
  static const rpg::core::TopicDef<int> kDef{"turn.ended"};
  return kDef;
}

// In main loop, after both phases:
++turnNumber;
mustBeOk(turnEndedTopic().on(bus).publish(turnNumber));

// Combat log subscriber
turnEndedTopic().on(bus).subscribe([](const int& turn) {
  std::cout << "— Turn " << turn << " complete —\n";
});

// Block reset subscriber
turnEndedTopic().on(bus).subscribe([](const int&) {
  // shields expire
});
```

### Example output

```
Hero: 14/20 HP, 0 block   Goblin: 12 HP

  energy: 3
  [1] Strike (cost 1, 6 dmg)
  [2] Bash (cost 2, 11 dmg)
  [3] Defend (cost 1, 5 block)
  [4] Salve (cost 1, 4 heal)
  [0] end turn
> 1
  Strike: 6 damage
    tough-skin (final): 6 -> 5

— Turn 1 complete —
  Shields expire.
```

### Challenges

1. **Enrage.** Subscribe to `turn.ended` — after turn 5, the goblin gets +1 energy per turn. No change to the game loop; the subscriber modifies goblin energy state.

2. **First blood.** Publish a `combat.hit` topic whenever damage resolves. Subscribe and print "First blood!" the first time either side takes damage, then unsubscribe. Demonstrates one-shot subscribers.

3. **Goblin taunts.** Subscribe to `turn.ended` and print a random goblin taunt each turn. Pure flavor, zero mechanics — proves the Bus is for more than combat logic.

## Tutorial 07 — Rich Events (Chained Topics + DamageAttempt)

### Goal

Events carry context. A chained topic lets subscribers contribute modifiers before the resolution completes. This is the plumbing that makes Effects possible — but we build it without any Effects yet.

### What changes from tutorial 06

- `DamageAttempt` struct replaces bare `int` in damage resolution — carries `target` and `baseAmount`
- `combat.damage` — a `ChainedTopic<DamageAttempt>` where subscribers register modifiers
- `damageStages()` — `{"base", "effects", "final"}`, shared across every resolution
- Tough-skin and block both subscribe to `combat.damage` via the chained topic (they were local lambdas before)
- `resolveCardDamage()` and `resolveMonsterDamage()` now `publish()` to the chained topic
- The damage receipt shows stage-based breakdowns with `DamageAttempt` values

### What does NOT change

- No Effects yet. No Bleed, no Rend
- Game behavior is identical to tutorial 06 — this is a plumbing refactor
- Healing stays `Chain<int>` (no effect modifies it yet)
- The Bus from tutorial 06 carries the new topic alongside `turn.ended`

### Key insight for the reader

In tutorial 06, tough-skin was a lambda baked into `cardDamage()`. Now it's a subscriber on the Bus. The chain doesn't know about it — it just runs whatever modifiers the topic collected. This is the stepping stone: if tough-skin can subscribe, so can Bleed.

### Key code changes

```cpp
// DamageAttempt replaces bare int
struct DamageAttempt {
  std::string target;
  int baseAmount;
};

// Chained damage topic
const rpg::core::TopicDef<DamageAttempt>& damageTopic() {
  static const rpg::core::TopicDef<DamageAttempt> kDef{"combat.damage"};
  return kDef;
}

// Shared stages — every resolution uses these
std::vector<std::string> damageStages() {
  return {"base", "effects", "final"};
}

// Resolution: create chain, publish, execute
int resolveCardDamage(const Card& card, const std::string& targetName, Bus& bus) {
  Chain<DamageAttempt> chain(damageStages());
  // Tough-skin subscribes here (or via Bus subscriber)
  DamageAttempt attempt{.target = targetName, .baseAmount = card.damage};
  mustBeOk(damageTopic().onChained(bus).publish(attempt, chain));
  auto result = chain.execute(attempt);
  printDamageReceipt(card.name, card.damage, result);
  return result.value.baseAmount;
}
```

### Example output

Damage receipts now show target-aware breakdowns:

```
  Strike: 6 damage
    tough-skin (final): {target: Goblin, amount: 6} -> {target: Goblin, amount: 5}
```

### Challenges

1. **Targeted tough-skin.** Add an `attacker` field to `DamageAttempt`. Make tough-skin only reduce damage against the goblin — the goblin has thick skin, not the hero. Demonstrates that rich events let subscribers make targeted decisions.

2. **Critical hit.** Subscribe a 10% chance to double damage on `combat.damage` (chained). The receipt now shows a "critical" step. Shows that any subscriber can inject modifiers.

3. **Healing as a topic.** Migrate `resolveHeal` from `Chain<int>` to a chained topic `combat.heal` with a `HealAttempt` struct. Same pattern as damage — doing it a second time cements the concept.

## Tutorial 08 — Effects

### Goal

An Effect is an autonomous subscriber. It subscribes when applied, acts on schedule, and self-removes when done. The game loop goes from orchestrator to announcer — it publishes `turn.ended` and lets effects respond.

### What changes from tutorial 07

- `rpg::core::Effect` base class — `onApply`, `track()`, `remove()`, lifecycle management
- `BleedEffect` — subscribes to `turn.ended` only (DoT: deals `stacks` damage, decrements, removes itself at zero)
- The Rend card — `card.bleed > 0` creates a BleedEffect and applies it
- `activeEffects` list in the game loop — held, pruned, but never managing effect state
- The announcer shift: `turnEndedTopic().publish(turnNumber)` drives all effect behavior

### What this tutorial explicitly does NOT include

Bleed amplifying incoming damage. That's challenge 1 — the payoff for understanding everything so far.

### Key code additions

```cpp
// Rend card
{.name = "Rend", .cost = 2, .bleed = kBleedStacks},

// Card struct gains a bleed field
struct Card {
  std::string name;
  int cost = 0;
  int damage = 0;
  int heal = 0;
  int block = 0;
  int bleed = 0;  // applies this many Bleed stacks
};

// BleedEffect — subscribes to turn.ended only
class BleedEffect : public rpg::core::Effect {
 public:
  BleedEffect(Fighter& target, int stacks)
      : Effect("bleed-" + target.name, "rend"), target_(target), stacks_(stacks) {}

 protected:
  rpg::core::Status onApply(rpg::core::Bus& bus) override {
    track(turnEndedTopic().on(bus).subscribe(
        [this](const int&) { return onTurnEnd(); }));
    return rpg::core::Status::ok();
  }

 private:
  rpg::core::Status onTurnEnd() {
    std::cout << "  Bleed deals " << stacks_ << " damage to " << target_.name << '\n';
    target_.hp -= stacks_;
    --stacks_;
    if (stacks_ == 0) {
      std::cout << "  Bleed faded\n";
      return remove();
    }
    return rpg::core::Status::ok();
  }

  Fighter& target_;
  int stacks_;
};

// Game loop — announcer, not orchestrator
++turnNumber;
mustBeOk(turnEndedTopic().on(bus).publish(turnNumber));
// Prune self-removed effects
std::erase_if(activeEffects, [](const auto& e) { return !e->isActive(); });
```

### Example output

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
  Bleed deals 3 damage to Goblin
  Shields expire.

--- Goblin's turn (Goblin: 27 HP) ---
```

### The architectural shift

Before tutorial 08:
```
Game loop:  "Goblin takes 6 damage. Tick bleed. Check death."
```

After tutorial 08:
```
Game loop:  "turn.ended happened." (one publish)
BleedEffect: "I heard that — tick, deal damage, maybe remove myself."
```

The game loop never checks "is Bleed active?" — it just publishes and lets effects respond.

### Challenges

1. **Damage amp.** Make BleedEffect also subscribe to `combat.damage` — when someone hits the Bleeding target, add `+stacks * 2` damage at the "effects" stage. This is the "full Bleed" from the original tutorial 06, and it's the payoff for understanding the entire pipeline.

2. **Poison (damage-up tick).** Like Bleed, but tick damage *increases* each turn (1, 2, 3...) instead of decreasing. One-line change to `onTurnEnd`. Play with both — which feels more satisfying?

3. **Regen.** Heals the hero each turn. New card: "Regenerate" (cost 2, applies Regen 3). Same lifecycle as Bleed, different effect.

4. **Double Rend.** What happens if you play Rend twice before the first Bleed expires? The Effect base class allows multiple instances with different ids (`bleed-Goblin-1` vs `bleed-Goblin-2`). Does the goblin take double tick damage? Does the amp stack? Test it and see.

5. **Stacking (replace on re-apply).** Change Rend so playing it again refreshes Bleed to full stacks instead of creating a second instance. Search `activeEffects` for an existing Bleed and call `refresh()` on it.

## Exercise mapping from old tutorial 06

| Old exercise | New home |
|---|---|
| Strength buff | Tutorial 08 challenge 1 (after amp) or tutorial 07 challenge 1 (needs attacker field) |
| Poison | Tutorial 08 challenge 2 |
| Regen | Tutorial 08 challenge 3 |
| Double Rend | Tutorial 08 challenge 4 |
| Stacking (replace on re-apply) | Tutorial 08 challenge 5 |

## Renumbering impact

| Old # | New # | Title |
|---|---|---|
| 06 | (split into 06, 07, 08) | — |
| 07 | 09 | Cards Become Actions |
| 08 | 10 | The Town Crier |
| 09 | 11 | Boss Fight |

Workshop track rejoins at the new tutorial 08 (was 06).

## What stays unchanged

- Tutorials 01-05: no changes
- Tutorial 05's challenges (goblin energy tuning, smarter AI, monster cards, remove pause) stay as-is
- Workshop track: no changes (W01-W04 still rejoin at tutorial 08)
- The juice appendix: no changes