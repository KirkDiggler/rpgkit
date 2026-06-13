# Tutorial 06 — The Bus: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the old tutorial 06 (statuses/effects/Bleed all at once) with a new tutorial 06 that introduces only the Bus and notification topics. The game plays identically to tutorial 05 — this is purely additive plumbing.

**Architecture:** Take the tutorial 05 codebase and add `rpg::core::Bus`, `Topic<int>` for `turn.ended`, a combat-log subscriber, and a block-reset subscriber. Damage chains remain `Chain<int>`. No Effects, no Bleed, no Rend. The Bus is a new dependency that all future tutorials build on.

**Tech Stack:** C++20, rpgkit core headers, GoogleTest (build system only — no new tests needed since tutorials don't have unit tests, they compile-and-run)

**Issue:** https://github.com/KirkDiggler/rpgkit/issues/27

---

## File Structure

| Action | Path | Responsibility |
|--------|------|----------------|
| Create | `examples/tutorials/06_bus/main.cpp` | Runnable tutorial checkpoint |
| Create | `examples/tutorials/06_bus/CMakeLists.txt` | Build target |
| Modify | `examples/tutorials/CMakeLists.txt` | Add `06_bus` subdirectory, remove `06_statuses` |
| Create | `docs/tutorials/06-the-bus.md` | Tutorial documentation |
| Modify | `docs/tutorials/README.md` | Update Campaign table with new numbering (06=Bus, 07=Rich Events, 08=Effects, 09=Actions, 10=Town Crier, 11=Boss Fight) |
| Delete | `examples/tutorials/06_statuses/` | Old tutorial 06 (will be reimplemented as 08) |
| Delete | `docs/tutorials/06-statuses.md` | Old tutorial 06 doc |

The old 06_statuses content (Bus + ChainedTopic + DamageAttempt + Effect + Bleed) is being split across three tutorials (06, 07, 08). It will be reimplemented incrementally. We remove it now to avoid confusion.

---

### Task 1: Create the branch and remove old tutorial 06

**Files:**
- Delete: `examples/tutorials/06_statuses/` (entire directory)
- Delete: `docs/tutorials/06-statuses.md`
- Modify: `examples/tutorials/CMakeLists.txt` — remove `add_subdirectory(06_statuses)`

- [ ] **Step 1: Create feature branch from main**

```bash
cd /home/kirk/personal/rpgkit
git checkout main
git pull
git checkout -b feat/tutorial-06-bus
```

- [ ] **Step 2: Remove old 06_statuses example directory**

```bash
rm -rf examples/tutorials/06_statuses
```

- [ ] **Step 3: Remove old 06-statuses.md doc**

```bash
rm docs/tutorials/06-statuses.md
```

- [ ] **Step 4: Update `examples/tutorials/CMakeLists.txt`** — remove the `06_statuses` line

The file should become:

```cmake
# Tutorial checkpoints — each directory is the complete, runnable state of
# the game at the end of one tutorial in docs/tutorials/. CI builds them so
# the tutorials can never silently rot.
add_subdirectory(02_game_base)
add_subdirectory(03_cards_energy)
add_subdirectory(04_block)
add_subdirectory(05_monster_brain)
```

(Do NOT add `06_bus` yet — that comes in Task 3 after the code compiles.)

- [ ] **Step 5: Verify build still passes**

```bash
make pre-commit
```

Expected: all tests pass, no 06_statuses references remain.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "chore: remove old tutorial 06 (will be reimplemented as three tutorials)"
```

---

### Task 2: Write the tutorial 06 example code

**Files:**
- Create: `examples/tutorials/06_bus/main.cpp`

This is tutorial 05's game with the Bus added. Key changes from 05:
1. `#include "rpg/core/bus.hpp"` and `#include "rpg/core/topic.hpp"` added
2. `rpg::core::Bus bus` created in `main()`, passed through to phases
3. `turnEndedTopic()` defined — a `TopicDef<int>` with key `"turn.ended"`
4. Block reset moves from explicit `hero.block = 0` in main loop and `goblin.block = 0` in `goblinPhase` to a subscriber on `turn.ended`
5. Combat-log subscriber prints `"— Turn N complete —"` on `turn.ended`
6. The main loop publishes `turnEndedTopic().on(bus).publish(turnNumber)` after both phases
7. `playerPhase` and `goblinPhase` signatures add `Bus& bus` parameter (needed for future tutorials, passed through but not yet used for damage)

What does NOT change:
- Damage chains are still `Chain<int>` — no DamageAttempt yet
- No Effects, no Bleed, no Rend
- Cards are the same as tutorial 05
- The goblin AI is unchanged
- `resolveDamage`, `resolveHeal`, `cardDamage`, `monsterDamage` unchanged

- [ ] **Step 1: Create the `06_bus` directory**

```bash
mkdir -p examples/tutorials/06_bus
```

- [ ] **Step 2: Write `examples/tutorials/06_bus/main.cpp`**

Start from a copy of tutorial 05's `main.cpp` and make these specific changes:

1. Add includes at top: `#include "rpg/core/bus.hpp"` and `#include "rpg/core/topic.hpp"` (after existing includes)

2. Add after the `kCardDelayMs` constant:

```cpp
// The Bus — every subscriber, every topic, every announcement connects here.
// One object, passed everywhere that needs to publish or listen.
// This tutorial adds the Bus, but the game plays identically to tutorial 05.
// The Bus is plumbing for things to come.

// Topics are static facts about this game: named, typed channels that
// publishers and subscribers agree on without knowing each other.
const rpg::core::TopicDef<int>& turnEndedTopic() {
  static const rpg::core::TopicDef<int> kDef{"turn.ended"};
  return kDef;
}
```

3. In `main()`, add before the `Fighter` declarations:

```cpp
rpg::core::Bus bus;
```

4. Subscribe block reset and combat log before the game loop:

```cpp
// Shields expire at the end of each turn — a subscriber handles this
// instead of the game loop doing it explicitly.
turnEndedTopic().on(bus).subscribe([&hero, &goblin](const int& /*turn*/) {
  hero.block = 0;
  goblin.block = 0;
  std::cout << "  Shields expire.\n";
});

// Combat log — another subscriber, another thing the game loop doesn't
// need to know about.
turnEndedTopic().on(bus).subscribe([](const int& turn) {
  std::cout << "— Turn " << turn << " complete —\n";
});
```

5. Remove `hero.block = 0;` from the main loop (the subscriber handles it now)

6. Remove `goblin.block = 0;` from `goblinPhase()` (the subscriber handles it)

7. After the goblin phase (and death check), add:

```cpp
// Announce: the game loop's only interaction with the Bus in this tutorial.
// Everything else (shield expiry, combat log) happens because subscribers
// respond to this announcement.
++turnNumber;
mustBeOk(turnEndedTopic().on(bus).publish(turnNumber));
```

8. Add `int turnNumber = 0;` before the main loop

9. Add `Bus& bus` parameter to `playerPhase` and `goblinPhase` signatures (passing through for future use; not yet used for damage in this tutorial)

10. Update the comment header at top of file to describe what's new since 05

11. Update the build/run command comment to use `06_bus/tutorial_06_bus`

The full file should compile and produce a game that plays identically to tutorial 05, except with turn-end announcements and "Shields expire" messages.

- [ ] **Step 3: Verify it compiles and runs**

```bash
make build
./build/debug/examples/tutorials/06_bus/tutorial_06_bus
```

Play through a few turns. Verify:
- Turn-end announcements appear ("— Turn N complete —")
- "Shields expire." prints after each turn
- Block still works (absorbs damage during a turn, resets at end)
- Game plays identically to tutorial 05 otherwise

- [ ] **Step 4: Commit**

```bash
git add examples/tutorials/06_bus/main.cpp
git commit -m "feat(tutorials): 06 — the Bus, turn events, subscribers"
```

---

### Task 3: Add CMake build target and verify full pipeline

**Files:**
- Create: `examples/tutorials/06_bus/CMakeLists.txt`
- Modify: `examples/tutorials/CMakeLists.txt` — add `add_subdirectory(06_bus)`

- [ ] **Step 1: Create `examples/tutorials/06_bus/CMakeLists.txt`**

```cmake
add_executable(tutorial_06_bus main.cpp)
target_link_libraries(tutorial_06_bus PRIVATE rpg::core rpgkit_warnings)
```

- [ ] **Step 2: Update `examples/tutorials/CMakeLists.txt`** — add `add_subdirectory(06_bus)` at the end

```cmake
# Tutorial checkpoints — each directory is the complete, runnable state of
# the game at the end of one tutorial in docs/tutorials/. CI builds them so
# the tutorials can never silently rot.
add_subdirectory(02_game_base)
add_subdirectory(03_cards_energy)
add_subdirectory(04_block)
add_subdirectory(05_monster_brain)
add_subdirectory(06_bus)
```

- [ ] **Step 3: Verify full build and tests**

```bash
make pre-commit
```

Expected: builds, all tests pass, `tutorial_06_bus` compiles.

- [ ] **Step 4: Commit**

```bash
git add examples/tutorials/06_bus/CMakeLists.txt examples/tutorials/CMakeLists.txt
git commit -m "build(tutorials): add 06_bus CMake target"
```

---

### Task 4: Write the tutorial documentation

**Files:**
- Create: `docs/tutorials/06-the-bus.md`

The doc should follow the same style as existing tutorials (05-monster-brain.md). Key sections:

1. **Title & Goal**: Tutorial 06 — The Bus. Goal: the game loop stops being the only place that knows what happened. It announces; subscribers respond.
2. **The finished version**: link to `examples/tutorials/06_bus/main.cpp`, build/run commands, sample output showing turn announcements and shield expiry
3. **What's new (and the one big lesson)**: numbered list of additions with code snippets
   - The Bus — one object, passed everywhere
   - Topics — `turn.ended` as a notification topic
   - Block reset as a subscriber — not inline game loop code
   - Combat log as a subscriber — pure flavor, zero mechanics
4. **What did NOT change**: player cards, goblin AI, damage chains, no Effects/Bleed
5. **The architectural shift paragraph**: before the Bus, the game loop managed everything inline. After: the loop publishes, subscribers react. Where block reset used to be `hero.block = 0` in the loop, it's now a subscriber that hears `turn.ended`.
6. **Detailed sections with code**:
   - Section 1: The Bus (one line, passed everywhere)
   - Section 2: Topics (notification vs chained, static facts)
   - Section 3: Block reset subscriber (moved off the loop)
   - Section 4: Combat log subscriber (flavor subscriber)
   - Section 5: The publish (what the loop does now)
7. **Challenges** (as specified in the design doc):
   1. Enrage — after turn 5, goblin gets +1 energy
   2. First blood — publish `combat.hit`, print once, unsubscribe
   3. Goblin taunts — random flavor on `turn.ended`

- [ ] **Step 1: Write `docs/tutorials/06-the-bus.md`**

Write the full tutorial doc following the style and format of `docs/tutorials/05-monster-brain.md`. Include all sections listed above. The doc should be self-contained — a reader can start from tutorial 05's checkpoint and add the Bus by following along.

- [ ] **Step 2: Proofread the doc**

Check that all code snippets reference `06_bus`, all build commands use `tutorial_06_bus`, and the challenges section matches the design spec.

- [ ] **Step 3: Commit**

```bash
git add docs/tutorials/06-the-bus.md
git commit -m "docs(tutorials): 06 — the Bus, turn events, subscribers"
```

---

### Task 5: Update README with new numbering

**Files:**
- Modify: `docs/tutorials/README.md`

Update the Campaign table to reflect the new tutorial numbering:

```
| 06 | [The Bus](06-the-bus.md) | Publish/subscribe — the game loop announces, subscribers respond |
| 07 | Rich Events *(coming)* | Events carry context; chained topics let subscribers contribute modifiers |
| 08 | Effects *(coming)* | An autonomous subscriber that manages its own lifecycle |
| 09 | Cards Become Actions *(coming)* | Card-playing logic migrates into Action contracts |
| 10 | The Town Crier *(coming)* | A combat announcer — UI is just another subscriber |
| 11 | Boss Fight *(coming)* | A phase-based boss using the full nervous system |
```

Also update the Workshop rejoin point from "Rejoins the Campaign at tutorial 06" to mention tutorial 08 (Effects).

Also update the footer to say "The Bus" instead of "Statuses that stick around" for 06.

- [ ] **Step 1: Update `docs/tutorials/README.md`**

Replace the Campaign table rows for 06+ and update the Workshop rejoin text.

- [ ] **Step 2: Verify no broken links**

Check that `06-the-bus.md` exists and links work. The "coming" tutorials don't need docs yet.

- [ ] **Step 3: Commit**

```bash
git add docs/tutorials/README.md
git commit -m "docs(tutorials): update README with new numbering (06=Bus, 07=Rich Events, 08=Effects)"
```

---

### Task 6: Final verification and push

- [ ] **Step 1: Run full pre-commit checks**

```bash
make pre-commit
```

Expected: format check passes, clang-tidy passes, all tests pass, tutorial_06_bus builds and runs.

- [ ] **Step 2: Run the tutorial binary and verify gameplay**

```bash
./build/debug/examples/tutorials/06_bus/tutorial_06_bus
```

Verify:
- Game starts, shows hero/goblin stats
- Cards work (Strike, Bash, Salve, Defend)
- Goblin AI plays cards
- Turn-end announcements appear
- "Shields expire" appears after each turn
- Block resets correctly
- Win/lose conditions work

- [ ] **Step 3: Push the branch**

```bash
git push -u origin feat/tutorial-06-bus
```

- [ ] **Step 4: Open PR against main**

Use `gh pr create` with:
- Title: `feat(tutorials): 06 — the Bus, turn events, subscribers`
- Body references issue #27
- Closes #27

---

## Self-Review Checklist

- [x] **Spec coverage**: Design spec covers Bus, Topics, turn.ended, block reset subscriber, combat log subscriber, challenges — all covered in tasks
- [x] **Placeholder scan**: No TBDs, TODOs, or vague steps — all code is specified
- [x] **Type consistency**: `Bus`, `TopicDef<int>`, `turnEndedTopic()` match rpgkit core API — verified against existing `06_statuses` code which used the same types
- [x] **Old tutorial 06 removed**: Task 1 explicitly removes old material before adding new
- [x] **No behavior change**: Tutorial 06 should play identically to 05, just with announcements
- [x] **README renumbering**: Task 5 covers the full renumbering chain