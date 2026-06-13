# Tutorial Tracks Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restructure rpgkit tutorials into two parallel tracks (Campaign + Workshop) with an optional Juice appendix.

**Architecture:** The Campaign (tutorials 03–09) is existing and untouched. The Workshop (W01–W04) adds new example directories under `examples/workshop/`, CI-compiled. The Juice appendix is a standalone doc under `docs/tutorials/`. The README forks after tutorial 02.

**Tech Stack:** C++20, CMake, rpg::core (Bus, Chain, TopicDef, Topic, ChainedTopic, Effect)

---

### Task 1: Scaffold the Workshop examples tree

**Files:**
- Create: `examples/workshop/CMakeLists.txt`
- Create: `examples/workshop/w01_bus/CMakeLists.txt`
- Create: `examples/workshop/w01_bus/main.cpp`
- Create: `examples/workshop/w02_chains/CMakeLists.txt`
- Create: `examples/workshop/w02_chains/main.cpp`
- Create: `examples/workshop/w04_build_effect/CMakeLists.txt`
- Create: `examples/workshop/w04_build_effect/skeleton.cpp`
- Create: `examples/workshop/w04_build_effect/solution.cpp`
- Modify: `examples/CMakeLists.txt`

- [ ] **Step 1: Update `examples/CMakeLists.txt`**

Edit `/home/kirk/personal/rpgkit/examples/CMakeLists.txt`, insert after `add_subdirectory(tutorials)`:
```
add_subdirectory(workshop)
```

- [ ] **Step 2: Create `examples/workshop/CMakeLists.txt`**

```
# The Workshop — architecture deep-dive modules for the engineering track.
add_subdirectory(w01_bus)
add_subdirectory(w02_chains)
add_subdirectory(w04_build_effect)
```

- [ ] **Step 3: Create each workshop module's CMakeLists.txt**

`w01_bus/CMakeLists.txt`:
```
add_executable(workshop_w01_bus main.cpp)
target_link_libraries(workshop_w01_bus PRIVATE rpg::core rpgkit_warnings)
```

`w02_chains/CMakeLists.txt`:
```
add_executable(workshop_w02_chains main.cpp)
target_link_libraries(workshop_w02_chains PRIVATE rpg::core rpgkit_warnings)
```

`w04_build_effect/CMakeLists.txt`:
```
add_executable(workshop_w04_effect_skeleton skeleton.cpp)
target_link_libraries(workshop_w04_effect_skeleton PRIVATE rpg::core rpgkit_warnings)

add_executable(workshop_w04_effect_solution solution.cpp)
target_link_libraries(workshop_w04_effect_solution PRIVATE rpg::core rpgkit_warnings)
```

- [ ] **Step 4: Create placeholder main.cpp files**

Each gets `int main() { return 0; }` — replaced in Tasks 2, 3, 5.

- [ ] **Step 5: Verify it builds**

```bash
cmake -B build && cmake --build build -j
```

Expected: all workshop targets compile (empty programs).

- [ ] **Step 6: Commit**

```bash
git add examples/workshop/ examples/CMakeLists.txt
git commit -m "feat: scaffold Workshop examples tree"
```

---

### Task 2: W01 — The Bus module

**Files:**
- Modify: `examples/workshop/w01_bus/main.cpp`
- Create: `docs/tutorials/workshop/w01-bus.md`

- [ ] **Step 1: Write `examples/workshop/w01_bus/main.cpp`**

```cpp
#include <cstdio>
#include <rpg/core/bus.hpp>
#include <rpg/core/topic.hpp>

int main() {
    using namespace rpg::core;

    Bus bus;
    int total{0};

    // Define a topic and bind it to this bus
    auto damage_topic = TopicDef<int>{"damage_dealt"}.on(bus);

    // Subscribe — handler runs on every publish
    auto sub = damage_topic.subscribe([&](int dmg) {
        total += dmg;
        std::printf("  subscriber: +%d damage (total: %d)\n", dmg, total);
        return Status::ok();
    });

    // Publish — anyone can, no reference to the subscriber
    std::printf("Publishing 5 damage...\n");
    damage_topic.publish(5);

    std::printf("Publishing 3 damage...\n");
    damage_topic.publish(3);

    std::printf("Publishing 7 damage...\n");
    damage_topic.publish(7);

    std::printf("Final total: %d\n", total);

    // Unsubscribe
    bus.unsubscribe(sub);

    std::printf("After unsubscribe, publishing 10 (should be ignored)...\n");
    damage_topic.publish(10);
    std::printf("Total still: %d (unchanged)\n", total);

    return 0;
}
```

- [ ] **Step 2: Verify it builds and runs**

```bash
cmake --build build -j && ./build/examples/workshop/w01_bus/workshop_w01_bus
```

Expected: three publications accumulate, then the fourth is ignored after unsubscribe.

- [ ] **Step 3: Write `docs/tutorials/workshop/w01-bus.md`**

```markdown
# Workshop W01: The Bus

**Goal:** See how a Bus lets events flow without the sender knowing who's listening.

## The primitive

A `Bus` is a message board. Anyone can pin a message (publish), anyone can
listen for messages matching a topic (subscribe), and neither knows the other
exists. This is the pattern that lets Rage just *listen* for damage events
without the game loop even knowing Rage is installed.

## Run it

```bash
cmake -B build && cmake --build build -j
./build/examples/workshop/w01_bus/workshop_w01_bus
```

## What you should see

```
Publishing 5 damage...
  subscriber: +5 damage (total: 5)
Publishing 3 damage...
  subscriber: +3 damage (total: 8)
Publishing 7 damage...
  subscriber: +7 damage (total: 15)
Final total: 15
After unsubscribe, publishing 10 (should be ignored)...
Total still: 15 (unchanged)
```

## How it works

1. `TopicDef<int>{"damage_dealt"}` defines a typed topic
2. `.on(bus)` binds it to a bus, returning a `Topic<int>`
3. `.subscribe(handler)` registers a callback, returns a `SubscriptionId`
4. `.publish(value)` delivers to every active subscriber
5. `bus.unsubscribe(id)` removes the handler

## Key insight

The publisher and subscriber have no reference to each other. They connect
through the Bus. This is the decoupling that makes effects additive — snap on
a subscriber, snap it off, no other code changes.
```

- [ ] **Step 4: Commit**

```bash
git add examples/workshop/w01_bus/main.cpp docs/tutorials/workshop/w01-bus.md
git commit -m "feat(workshop): W01 — The Bus module"
```

---

### Task 3: W02 — Chains & Stages module

**Files:**
- Modify: `examples/workshop/w02_chains/main.cpp`
- Create: `docs/tutorials/workshop/w02-chains.md`

- [ ] **Step 1: Write `examples/workshop/w02_chains/main.cpp`**

```cpp
#include <cstdio>
#include <rpg/core/chain.hpp>

int main() {
    using namespace rpg::core;

    // Stage list declared once — the rulebook's ordering authority
    Chain<int> chain{{"base", "features", "conditions", "equipment", "final"}};

    // Each contributor names its stage and provides a unique id
    chain.add("features", "rage", [](int dmg) { return dmg + 2; });
    chain.add("equipment", "magic_sword", [](int dmg) { return dmg + 1; });
    chain.add("final", "resistance", [](int dmg) { return dmg / 2; });

    // Execute folds base value through all stages
    auto result = chain.execute(10);

    std::printf("Base damage: 10\n");
    for (const auto& step : result.breakdown) {
        std::printf("  %s [%s]: %d -> %d\n",
            step.id.c_str(), step.stage.c_str(), step.before, step.after);
    }
    std::printf("Final: %d\n\n", result.value);

    // Second example — different modifiers, same stages
    Chain<int> fight2{{"base", "features", "conditions", "equipment", "final"}};
    fight2.add("features", "rage", [](int dmg) { return dmg + 2; });
    fight2.add("final", "resistance", [](int dmg) { return dmg / 2; });

    auto result2 = fight2.execute(10);
    std::printf("Rage only, vs resistant:\n");
    for (const auto& step : result2.breakdown) {
        std::printf("  %s [%s]: %d -> %d\n",
            step.id.c_str(), step.stage.c_str(), step.before, step.after);
    }
    std::printf("Final: %d\n", result2.value);

    return 0;
}
```

- [ ] **Step 2: Verify it builds and runs**

```bash
cmake --build build -j && ./build/examples/workshop/w02_chains/workshop_w02_chains
```

Expected output shows each modifier's contribution and the final value. First example: `10 -> 12 -> 13 -> 6`. Second: `10 -> 12 -> 6`.

- [ ] **Step 3: Write `docs/tutorials/workshop/w02-chains.md`**

Mirror the W01 format. Explain stages as ordered pipeline, modifiers as named contributors, the breakdown as an audit trail. Tie back to Rage: "+2 at Features, resistance at Final — one effect, two contributions, both named in the receipt."

- [ ] **Step 4: Commit**

```bash
git add examples/workshop/w02_chains/main.cpp docs/tutorials/workshop/w02-chains.md
git commit -m "feat(workshop): W02 — Chains & Stages module"
```

---

### Task 4: W03 — Rage Dissected (guided reading)

**Files:**
- Create: `docs/tutorials/workshop/w03-rage-dissected.md`

No code. A guided-reading doc pointing at rpg-toolkit's canonical Rage implementation.

- [ ] **Step 1: Write `docs/tutorials/workshop/w03-rage-dissected.md`**

```markdown
# Workshop W03: Rage Dissected

**Goal:** Read a real D&D 5e Rage implementation to see how the Bus + Chain
pattern makes complex rules simple.

## The source files

The canonical Rage lives in the sibling project
[rpg-toolkit](https://github.com/KirkDiggler/rpg-toolkit):

- `rulebooks/dnd5e/features/rage.go` (182 lines) — the Feature that manages
  the resource (uses per day, active duration)
- `rulebooks/dnd5e/conditions/raging.go` (297 lines) — the Condition that
  subscribes to bus events while Rage is active

## What Rage does

Open both files and trace the lifecycle:

1. Rage is activated → a `Condition` is created
2. The Condition subscribes to **5 event topics** (hit, damage-dealt,
   damage-chain, turn-started, turn-ended)
3. On a damage-chain event, the `onDamageChain` handler runs:
   - **Outgoing:** adds +2 at the `features` stage (bonus damage)
   - **Incoming:** halves physical damage at the `final` stage (resistance)
4. On turn-end, a counter ticks down
5. At zero, the Condition unsubscribes everything and removes itself

## Key insight

**One handler does both outgoing and incoming.** The chain stages separate
them: +2 enters at Features, resistance applies at Final. Same function, two
contributions, one receipt. No if-else branching on "am I attacking or being
attacked" — the stage tuple makes it declarative.

## Questions to answer while reading

1. How does the handler know which damage is outgoing vs incoming?
2. What happens if Rage expires mid-resolution?
3. How does the Condition ensure cleanup even if the game loop forgets?
4. Why are Features and Conditions separate types in rpg-toolkit?
```

- [ ] **Step 2: Commit**

```bash
git add docs/tutorials/workshop/w03-rage-dissected.md
git commit -m "feat(workshop): W03 — Rage Dissected guided reading"
```

---

### Task 5: W04 — Build Your Own Effect module

**Files:**
- Modify: `examples/workshop/w04_build_effect/skeleton.cpp`
- Modify: `examples/workshop/w04_build_effect/solution.cpp`
- Create: `docs/tutorials/workshop/w04-build-your-own-effect.md`

- [ ] **Step 1: Write the skeleton (`skeleton.cpp`)**

A 15-line Effect lifecycle template for the reader to fill in:

```cpp
#include <cstdio>
#include <rpg/core/bus.hpp>
#include <rpg/core/effect.hpp>
#include <rpg/core/topic.hpp>

// TODO: Implement a Burning effect:
//  - onApply: subscribe to turn.ended, track the subscription
//  - on each turn: print fire damage, decrement stacks
//  - at zero stacks: call remove()
// See solution.cpp for a reference implementation.

int main() {
    // Test harness — make the effect tick 4 turns
    std::printf("Your effect will run here once you implement it.\n");
    std::printf("See solution.cpp for one complete implementation.\n");
    return 0;
}
```

- [ ] **Step 2: Write the solution (`solution.cpp`)**

```cpp
#include <cstdio>
#include <rpg/core/bus.hpp>
#include <rpg/core/effect.hpp>
#include <rpg/core/topic.hpp>

// A Burning status that deals fire damage each turn until it expires.
// Demonstrates: Effect subclass, onApply, track(), lifecycle cleanup.
class Burning : public rpg::core::Effect {
public:
    explicit Burning(int stacks)
        : Effect("burning", "fire_spell"), stacks_(stacks) {}

protected:
    rpg::core::Status onApply(rpg::core::Bus& bus) override {
        std::printf("🔥 Burning applied (%d stacks)\n", stacks_);
        track(rpg::core::TopicDef<int>{"turn.ended"}.on(bus).subscribe(
            [this](int turn) {
                return onTurnEnd(turn);
            }));
        return rpg::core::Status::ok();
    }

private:
    int stacks_;

    rpg::core::Status onTurnEnd(int /*turn*/) {
        if (stacks_ <= 0) return rpg::core::Status::ok();
        std::printf("🔥 Burning deals %d fire damage\n", stacks_);
        --stacks_;
        if (stacks_ == 0) {
            std::printf("🔥 Burning expired\n");
            return remove();  // unsubscribe everything
        }
        return rpg::core::Status::ok();
    }
};

int main() {
    rpg::core::Bus bus;

    std::printf("=== Burning Effect Demo ===\n\n");

    Burning burn(3);
    burn.apply(bus);

    for (int turn = 1; turn <= 5; ++turn) {
        std::printf("--- Turn %d ---\n", turn);
        rpg::core::TopicDef<int>{"turn.ended"}.on(bus).publish(turn);
        std::printf("\n");
    }

    std::printf("Done. Effect self-removed at zero stacks.\n");
    return 0;
}
```

- [ ] **Step 3: Verify it builds and runs**

```bash
cmake --build build -j && ./build/examples/workshop/w04_build_effect/workshop_w04_effect_solution
```

Expected: Burning ticks for 3 turns, self-removes at zero, no more output.

- [ ] **Step 4: Write `docs/tutorials/workshop/w04-build-your-own-effect.md`**

Include:
- The skeleton code (15 lines they fill in)
- Challenge variants: Poison (damage increases each turn), Aura (buffs all allies), Barrier (absorbs N damage then breaks)
- Cross-reference to the solution
- How to verify: `cmake --build build -j && ./build/.../workshop_w04_effect_skeleton`

- [ ] **Step 5: Commit**

```bash
git add examples/workshop/w04_build_effect/ docs/tutorials/workshop/w04-build-your-own-effect.md
git commit -m "feat(workshop): W04 — Build Your Own Effect module"
```

---

### Task 6: Juice appendix

**Files:**
- Create: `docs/tutorials/juice.md`

- [ ] **Step 1: Write `docs/tutorials/juice.md`**

```markdown
# Juice Appendix — Polish Your Terminal Game

Adoptable by any Campaign or Workshop checkpoint. Each checkpoint is
self-contained and tells you which tutorials it works with.

## Checkpoints

### JUICE-01: Color
ANSI escape codes for HP (red), damage (yellow), card names (cyan), healing
(green). Works with any tutorial that prints values.

```cpp
// Color helpers — stdlib only
#define RST "\033[0m"
#define RED "\033[31m"
#define GRN "\033[32m"
#define YLW "\033[33m"
#define CYN "\033[36m"

std::printf(RED "%d" RST " HP\n", hp);
std::printf(YLW "+%d" RST " damage\n", dmg);
```

### JUICE-02: HP bar
Unicode block characters. `██████░░` instead of `6/8 HP`.

```cpp
void printHpBar(int current, int max) {
    int filled = (current * 8 + max - 1) / max;  // 0..8
    for (int i = 0; i < 8; ++i) {
        std::printf("%s", i < filled ? "█" : "░");
    }
    std::printf(" %d/%d\n", current, max);
}
```

### JUICE-03: Clear on turn
Screen reset between turns. Uses stdlib escape codes.

```cpp
std::printf("\033[2J\033[H");  // clear screen, move cursor home
```

### JUICE-04: Pacing
Small delay after receipt printout so the player can read it.

```cpp
#include <thread>
#include <chrono>
// ...
std::this_thread::sleep_for(std::chrono::milliseconds(400));
```

### JUICE-05: FTXUI signpost
When escape codes aren't enough, FTXUI is the next step.

```cpp
// https://github.com/ArthurSonzogni/FTXUI
// Add to CMakeLists.txt:
//   FetchContent_Declare(ftxui ...)
//   target_link_libraries(my_game PRIVATE ftxui::screen)
```

## Going further

- **Menus and choice lists** — termios raw mode for arrow-key selection
- **Sprite art** — ANSI art for the goblin, the player, the boss
- **Animation** — damage numbers that scroll, health bars that tween
- **Beyond terminal** — signposts to SDL (graphical), Unreal plugin path
```

- [ ] **Step 2: Commit**

```bash
git add docs/tutorials/juice.md
git commit -m "docs: Juice appendix — terminal polish checkpoints"
```

---

### Task 7: Restructure the tutorials README

**Files:**
- Modify: `docs/tutorials/README.md`

- [ ] **Step 1: Read the existing README**

```bash
cat docs/tutorials/README.md
```

- [ ] **Step 2: Add the track fork**

After the section describing tutorials 01–02, insert:

```markdown
## Pick your path

| The Campaign | The Workshop |
|---|---|
| Build a game, feature by feature | Understand the engine |
| [03: Cards & Energy](03-cards-energy.md) → ... | [W01: The Bus](workshop/w01-bus.md) → ... |
| Start here to make something fast | Start here to see how it works |

> **Juice appendix** ([juice.md](juice.md)) — terminal polish available from
> either path at any checkpoint.
```

Then restructure the existing tutorial listing into two sub-sections:

**Campaign** (03–09) and **Workshop** (W01–W04), with the Workshop listing pointing to the new `workshop/` subdirectory docs.

- [ ] **Step 3: Commit**

```bash
git add docs/tutorials/README.md
git commit -m "docs: restructure tutorials README into Campaign + Workshop tracks"
```

---

### Task 8: Update living docs

**Files:**
- Modify: `docs/status.md`

- [ ] **Step 1: Update `docs/status.md`**

Replace the Active Work section's tutorial and v0.2.0 entries:

```markdown
## Active work

- **Tutorial tracks** — two-path structure in `docs/tutorials/`:
  - **Campaign** (03–09): game-building series, feature by feature
  - **Workshop** (W01–W04): architecture deep-dives (Bus, Chains, Rage,
     Build Your Own Effect)
  - **Juice appendix**: optional terminal polish at any checkpoint
- **v0.2.0 — Effects as autonomous bus citizens** (tutorial 06). Tag once
  Effects own their lifecycle (subscribe/expire/remove) without the game
  loop orchestrating them.
```

- [ ] **Step 2: Commit**

```bash
git add docs/status.md
git commit -m "docs: update status.md with tutorial tracks"
```
