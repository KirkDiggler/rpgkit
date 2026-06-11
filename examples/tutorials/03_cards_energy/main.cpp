// Tutorial 03 — cards & energy: the deck-builder begins.
//
// This is tutorial 02's game, grown: instead of one Strike button, you get a
// random hand of cards each turn and an energy budget that can't pay for all
// of them. That budget is the whole game. When you can't do everything,
// choosing becomes interesting.
//
// Build & run (from the repo root):
//   make build
//   ./build/debug/examples/tutorials/03_cards_energy/tutorial_03_cards_energy
//
// What's new since 02:
//   1. A Card struct — a card is DATA (name, cost, numbers), not code.
//   2. A dealt hand: 4 random cards a turn. Randomness is replayability.
//   3. The energy budget: 3 a turn, spend it or lose it.
//   4. A second chain: healing, with a max-HP cap that shows up on the
//      receipt like any other rule.
// What did NOT change: resolveDamage, readChoice, the goblin, the loop's
// bones. Growing a game should mostly be adding, not rewriting.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "rpg/core/chain.hpp"
#include "rpg/core/status.hpp"

namespace {

// The tuning table: every game number, named, in one place.
constexpr int kHeroMaxHp = 20;
constexpr int kGoblinHp = 30;
constexpr int kClawsDamage = 6;
constexpr int kEnergyPerTurn = 3;
constexpr int kHandSize = 4;

// A card is just its stats. New cards are new DATA — rows in a table a
// designer edits — not new code. (A card that needs genuinely new behavior
// earns a new field, and the play logic in playerPhase learns to read it.)
struct Card {
  std::string name;
  int cost = 0;
  int damage = 0;
  int heal = 0;
};

// The pool a hand is dealt from. (A function, not a global — globals with
// constructors are a C++ trap; tutorial 02's doc has the why.)
std::vector<Card> cardPool() {
  // This table IS the named home of each card stat — a per-stat constant
  // (kStrikeDamage...) would just say the row again, worse.
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  return {
      {.name = "Strike", .cost = 1, .damage = 6, .heal = 0},
      {.name = "Bash", .cost = 2, .damage = 11, .heal = 0},
      {.name = "Salve", .cost = 1, .damage = 0, .heal = 4},
  };
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

struct Fighter {
  std::string name;
  int hp = 0;
  int maxHp = 0;
};

void mustBeOk(const rpg::core::Status& status) {
  if (!status.isOk()) {
    std::cerr << "error: " << status.message() << '\n';
    std::exit(EXIT_FAILURE);
  }
}

// Same safe reader as tutorial 02, one upgrade: if input ends entirely
// (Ctrl-D, or a script's piped input running out), return kQuit so the game
// exits cleanly instead of looping on "end turn" forever.
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

// Unchanged from tutorial 02. Growing the game didn't touch it.
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

// Healing rides the same machinery as damage: a chain, a rule, a receipt.
// The cap is just a modifier — "you can't heal past full" appears on the
// receipt exactly like tough-skin does.
int resolveHeal(const std::string& cardName, int baseHeal, const Fighter& target) {
  rpg::core::Chain<int> heal(std::vector<std::string>{"base", "cap"});
  // Clamped to zero: if hp ever exceeds maxHp (an over-heal card, a max-HP
  // debuff), a negative room would turn healing into damage.
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

// Tutorial 02's heroStrikes, generalized: the damage number now comes from
// a card instead of a constant. The goblin's tough-skin rule is unchanged.
int cardDamage(const Card& card) {
  rpg::core::Chain<int> damage(std::vector<std::string>{"base", "final"});
  mustBeOk(damage.add("final", "tough-skin", [](int dmg) { return dmg - 1; }));
  return resolveDamage(card.name, card.damage, damage);
}

int goblinClaws() {
  const rpg::core::Chain<int> damage(std::vector<std::string>{"base", "final"});
  return resolveDamage("Claws", kClawsDamage, damage);
}

// Deal a fresh hand: kHandSize random picks from the pool, duplicates
// welcome (two Strikes in hand is a fine hand).
std::vector<Card> dealHand(std::mt19937& rng) {
  const std::vector<Card> pool = cardPool();
  std::uniform_int_distribution<std::size_t> pick{0, pool.size() - 1};
  std::vector<Card> hand;
  hand.reserve(kHandSize);
  for (int i = 0; i < kHandSize; ++i) {
    hand.push_back(pool.at(pick(rng)));
  }
  return hand;
}

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
    std::cout << ")" << (played.at(i) ? "  [played]" : "") << '\n';
  }
  std::cout << "  [0] end turn\n> ";
}

// How a player phase can end. Naming the outcomes keeps the turn loop in
// main readable as a sentence.
enum class PhaseResult : std::uint8_t { kTurnOver, kGoblinDown, kQuit };

// The player phase: a hand, a budget, and choices — keep playing cards
// until the energy is gone, the player stops, or the goblin drops.
PhaseResult playerPhase(Fighter& hero, Fighter& goblin, std::mt19937& rng) {
  std::vector<Card> hand = dealHand(rng);
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

    // Pay, mark, resolve.
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
  }
  std::cout << "  out of energy.\n";
  return PhaseResult::kTurnOver;
}

}  // namespace

int main() {
  Fighter hero{.name = "Hero", .hp = kHeroMaxHp, .maxHp = kHeroMaxHp};
  Fighter goblin{.name = "Goblin", .hp = kGoblinHp, .maxHp = kGoblinHp};
  std::mt19937 rng{std::random_device{}()};

  std::cout << "A goblin blocks your path — and you've brought your deck.\n";

  // The turn loop, now readable as a sentence: player phase, then unless
  // it ended the game, goblin phase, then check the hero still stands.
  while (true) {
    std::cout << '\n'
              << hero.name << ": " << hero.hp << "/" << hero.maxHp << " HP   " << goblin.name
              << ": " << goblin.hp << " HP\n";

    const PhaseResult result = playerPhase(hero, goblin, rng);
    if (result == PhaseResult::kQuit) {
      std::cout << "\nYou fold your hand and slip away. The goblin keeps your deck.\n";
      return 0;
    }
    if (result == PhaseResult::kGoblinDown) {
      std::cout << "\nThe goblin falls. Victory!\n";
      return 0;
    }

    // ---- Goblin phase: unchanged from tutorial 02. ----
    hero.hp -= goblinClaws();
    if (hero.hp <= 0) {
      std::cout << "\nYou crumple. The goblin shuffles your deck into its loot bag.\n";
      return 0;
    }
  }
}
