// Tutorial 05 — monster brain: the goblin learns to play.
//
// This is tutorial 04's game, grown: the goblin gets its own hand of cards,
// an energy budget, and a simple AI. Same Fighter, same chains, same rules
// — both sides play by identical machinery. The only difference is the card
// pool each draws from and who makes the choices.
//
// Build & run (from the repo root):
//   make build
//   ./build/debug/examples/tutorials/05_monster_brain/tutorial_05_monster_brain
//
// What's new since 04:
//   1. A monster card pool — goblins fight dirty (free Claws, heavy Bite).
//   2. A 10-line AI that picks the biggest attack it can afford.
//   3. The goblin phase is now a proper card loop, same shape as the player's.
//   4. Symmetry: both sides share Fighter, chains, cards-as-data — zero
//      special-case "this is how the monster attacks" code.
// What did NOT change: the player's cards, the player phase, resolveDamage,
// resolveHeal, the turn loop's bones.
//
// Last chain-only episode. Tutorial 06 introduces the Bus and autonomous
// Effects — statuses that listen and act on their own, no game loop needed.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "rpg/core/chain.hpp"
#include "rpg/core/status.hpp"

namespace {

// The tuning table: every game number, named, in one place.
constexpr int kHeroMaxHp = 20;
constexpr int kGoblinHp = 30;
constexpr int kEnergyPerTurn = 3;
constexpr int kGoblinEnergyPerTurn = 2;
constexpr int kHandSize = 4;
constexpr int kDefendBlock = 5;
constexpr int kCardDelayMs = 300;

// A card is just its stats. New cards are new DATA — rows in a table a
// designer edits — not new code. (A card that needs genuinely new behavior
// earns a new field, and the play logic learns to read it.)
struct Card {
  std::string name;
  int cost = 0;
  int damage = 0;
  int heal = 0;
  int block = 0;
};

// The pool the hero's hand is dealt from. Unchanged since tutorial 04.
std::vector<Card> playerCardPool() {
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  return {
      {.name = "Strike", .cost = 1, .damage = 6, .heal = 0},
      {.name = "Bash", .cost = 2, .damage = 11, .heal = 0},
      {.name = "Salve", .cost = 1, .damage = 0, .heal = 4},
      {.name = "Defend", .cost = 1, .block = kDefendBlock},
  };
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

// The monster card pool: same struct, different numbers. Goblins are
// aggressive — free Claws every turn if drawn, a heavy Bite when they can
// afford it, and Hide to survive when on the back foot.
std::vector<Card> monsterCardPool() {
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  return {
      {.name = "Claws", .cost = 0, .damage = 6},
      {.name = "Bite", .cost = 1, .damage = 9},
      {.name = "Hide", .cost = 1, .block = 4},
  };
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

struct Fighter {
  std::string name;
  int hp = 0;
  int maxHp = 0;
  int block = 0;
};

void mustBeOk(const rpg::core::Status& status) {
  if (!status.isOk()) {
    std::cerr << "error: " << status.message() << '\n';
    std::exit(EXIT_FAILURE);
  }
}

constexpr int kQuit = -1;
int readChoice() {
  int choice = 0;
  while (!(std::cin >> choice)) {
    if (std::cin.eof()) {
      return kQuit;
    }
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cout << "  numbers only — try again: ";
  }
  return choice;
}

// Unchanged since tutorial 02. Growing the game never touched it.
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

int resolveHeal(const std::string& cardName, int baseHeal, const Fighter& target) {
  rpg::core::Chain<int> heal(std::vector<std::string>{"base", "cap"});
  const int room = std::max(0, target.maxHp - target.hp);
  mustBeOk(
      heal.add("cap", "max-hp-cap", [room](int amount) { return amount > room ? room : amount; }));
  const rpg::core::Chain<int>::Result result = heal.execute(baseHeal);
  std::cout << "  " << cardName << ": " << baseHeal << " healing\n";
  for (const rpg::core::Chain<int>::Step& step : result.breakdown) {
    std::cout << "    " << step.id << " (" << step.stage << "): " << step.before << " -> "
              << step.after << '\n';
  }
  return result.value;
}

int cardDamage(const Card& card) {
  rpg::core::Chain<int> damage(std::vector<std::string>{"base", "final"});
  mustBeOk(damage.add("final", "tough-skin", [](int dmg) { return dmg - 1; }));
  return resolveDamage(card.name, card.damage, damage);
}

// The goblin's damage chain includes the hero's block, same as tutorial 04.
int monsterDamage(const Card& card, Fighter& hero) {
  rpg::core::Chain<int> damage(std::vector<std::string>{"base", "block"});
  if (hero.block > 0) {
    mustBeOk(damage.add("block", "block-" + hero.name, [&hero](int dmg) {
      const int absorbed = std::min(hero.block, dmg);
      hero.block -= absorbed;
      return dmg - absorbed;
    }));
  }
  return resolveDamage(card.name, card.damage, damage);
}

std::vector<Card> dealHand(const std::vector<Card>& pool, std::mt19937& rng) {
  std::uniform_int_distribution<std::size_t> pick{0, pool.size() - 1};
  std::vector<Card> hand;
  hand.reserve(kHandSize);
  for (int i = 0; i < kHandSize; ++i) {
    hand.push_back(pool.at(pick(rng)));
  }
  return hand;
}

// ---- Player phase (unchanged from tutorial 04) ----

void printHand(const std::vector<Card>& hand, const std::vector<bool>& played, int energy) {
  std::cout << "\n  energy: " << energy << '\n';
  for (std::size_t i = 0; i < hand.size(); ++i) {
    std::cout << "  [" << i + 1 << "] " << hand.at(i).name << " (cost " << hand.at(i).cost;
    if (hand.at(i).damage > 0) {
      std::cout << ", " << hand.at(i).damage << " dmg";
    }
    if (hand.at(i).heal > 0) {
      std::cout << ", " << hand.at(i).heal << " heal";
    }
    if (hand.at(i).block > 0) {
      std::cout << ", " << hand.at(i).block << " block";
    }
    std::cout << ")" << (played.at(i) ? "  [played]" : "") << '\n';
  }
  std::cout << "  [0] end turn\n> ";
}

enum class PhaseResult : std::uint8_t { kTurnOver, kGoblinDown, kQuit };

PhaseResult playerPhase(Fighter& hero, Fighter& goblin, std::mt19937& rng) {
  std::vector<Card> hand = dealHand(playerCardPool(), rng);
  std::vector<bool> played(hand.size(), false);
  int energy = kEnergyPerTurn;

  while (energy > 0) {
    printHand(hand, played, energy);
    const int choice = readChoice();
    if (choice == kQuit) {
      return PhaseResult::kQuit;
    }
    if (choice == 0) {
      return PhaseResult::kTurnOver;
    }
    const std::size_t index = static_cast<std::size_t>(choice) - 1;
    if (choice < 1 || index >= hand.size()) {
      std::cout << "  no such card.\n";
      continue;
    }
    if (played.at(index)) {
      std::cout << "  already played that one.\n";
      continue;
    }
    const Card& card = hand.at(index);
    if (card.cost > energy) {
      std::cout << "  not enough energy (" << card.name << " costs " << card.cost << ").\n";
      continue;
    }

    energy -= card.cost;
    played.at(index) = true;
    if (card.damage > 0) {
      goblin.hp -= cardDamage(card);
      if (goblin.hp <= 0) {
        return PhaseResult::kGoblinDown;
      }
    }
    if (card.heal > 0) {
      hero.hp += resolveHeal(card.name, card.heal, hero);
    }
    if (card.block > 0) {
      hero.block += card.block;
      std::cout << "  " << card.name << ": +" << card.block << " block (shield now " << hero.block
                << ")\n";
    }
  }
  std::cout << "  out of energy.\n";
  return PhaseResult::kTurnOver;
}

// ---- Goblin phase: now a proper AI turn ----

// The AI brain: scan the goblin's hand for the best affordable card.
// Priority: highest damage > highest block > first available.
// This is the only code that makes the goblin different from the player.
int aiPick(const std::vector<Card>& hand, const std::vector<bool>& played, int energy) {
  int best = -1;
  for (std::size_t i = 0; i < hand.size(); ++i) {
    if (played.at(i) || hand.at(i).cost > energy) {
      continue;
    }
    if (best == -1 || hand.at(i).damage > hand.at(best).damage ||
        (hand.at(i).damage == hand.at(best).damage && hand.at(i).block > hand.at(best).block)) {
      best = static_cast<int>(i);
    }
  }
  return best;
}

// Returns true if the hero fell.
bool goblinPhase(Fighter& goblin, Fighter& hero, std::mt19937& rng) {
  // Shields don't stack across turns — same rule the hero plays by.
  goblin.block = 0;

  std::vector<Card> hand = dealHand(monsterCardPool(), rng);
  std::vector<bool> played(hand.size(), false);
  int energy = kGoblinEnergyPerTurn;

  std::cout << "\n  --- Goblin's turn (" << goblin.name << ": " << goblin.hp << " HP) ---\n";

  while (energy > 0) {
    const int pick = aiPick(hand, played, energy);
    if (pick == -1) {
      break;  // no playable cards
    }

    const Card& card = hand.at(static_cast<std::size_t>(pick));
    energy -= card.cost;
    played.at(static_cast<std::size_t>(pick)) = true;

    std::cout << "  " << goblin.name << " plays " << card.name << " (cost " << card.cost;
    if (card.damage > 0) {
      std::cout << ", " << card.damage << " dmg";
    }
    if (card.block > 0) {
      std::cout << ", " << card.block << " block";
    }
    std::cout << ")\n";

    if (card.damage > 0) {
      hero.hp -= monsterDamage(card, hero);
      if (hero.hp <= 0) {
        return true;
      }
    }
    if (card.block > 0) {
      goblin.block += card.block;
      std::cout << "    " << goblin.name << " gains " << card.block << " block (shield now "
                << goblin.block << ")\n";
    }
    // Brief pause so the player sees each card the goblin plays.
    std::this_thread::sleep_for(std::chrono::milliseconds(kCardDelayMs));
  }

  if (energy == 0 && goblin.hp > 0) {
    std::cout << "  (" << goblin.name << " out of energy)\n";
  }
  if (goblin.block > 0) {
    std::cout << "  " << goblin.name << " stands with " << goblin.hp << " HP, " << goblin.block
              << " block\n";
  }
  return false;
}

}  // namespace

int main() {
  Fighter hero{.name = "Hero", .hp = kHeroMaxHp, .maxHp = kHeroMaxHp};
  Fighter goblin{.name = "Goblin", .hp = kGoblinHp, .maxHp = kGoblinHp};
  std::mt19937 rng{std::random_device{}()};

  std::cout << "A goblin blocks your path — and it's brought its own deck.\n";

  while (true) {
    hero.block = 0;

    std::cout << '\n'
              << hero.name << ": " << hero.hp << "/" << hero.maxHp << " HP, " << hero.block
              << " block   " << goblin.name << ": " << goblin.hp << " HP\n";

    const PhaseResult result = playerPhase(hero, goblin, rng);
    if (result == PhaseResult::kQuit) {
      std::cout << "\nYou fold your hand and slip away. The goblin keeps your deck.\n";
      return 0;
    }
    if (result == PhaseResult::kGoblinDown) {
      std::cout << "\nThe goblin falls. Victory!\n";
      return 0;
    }

    if (goblinPhase(goblin, hero, rng)) {
      std::cout << "\nYou crumple. The goblin shuffles your deck into its loot bag.\n";
      return 0;
    }
  }
}
