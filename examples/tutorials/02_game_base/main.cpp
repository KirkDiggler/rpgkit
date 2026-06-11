// Tutorial 02 — your first terminal game: a hero, a goblin, and a fight.
//
// This is the playable result of docs/tutorials/02-your-first-terminal-game.md,
// and the base every later tutorial builds on (cards, energy, block...).
//
// Build & run (from the repo root):
//   make build
//   ./build/debug/examples/tutorials/02_game_base/tutorial_02_game_base
//
// The one big idea: damage is never just subtracted. Every hit travels
// through a Chain — a little pipeline of rules — and comes out the other
// side with a receipt showing what each rule did. Right now the goblin has
// one rule (tough skin). Later tutorials add more rules WITHOUT touching
// the combat math that's already here. That's the whole trick of rpgkit.

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "rpg/core/chain.hpp"
#include "rpg/core/status.hpp"

namespace {

// The tuning table: every game number lives here, named, in one place.
// Balancing the fight means editing these four lines and rebuilding —
// no hunting through the code for a stray 6.
constexpr int kHeroHp = 20;
constexpr int kGoblinHp = 18;
constexpr int kStrikeDamage = 6;
constexpr int kClawsDamage = 6;

// A character sheet. That's all a struct is: named numbers travelling
// together.
struct Fighter {
  std::string name;
  int hp = 0;
};

// Example-grade error handling: if the engine says no, stop loudly.
void mustBeOk(const rpg::core::Status& status) {
  if (!status.isOk()) {
    std::cerr << "error: " << status.message() << '\n';
    std::exit(EXIT_FAILURE);
  }
}

// Listen to the player without trusting them. Typing "potato" must not
// crash or hang the game: clear the error, throw the line away, ask again.
// If input ends entirely (Ctrl-D), treat it as choosing 0 (run away).
int readChoice() {
  int choice = 0;
  while (!(std::cin >> choice)) {
    if (std::cin.eof()) {
      return 0;
    }
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cout << "  numbers only — try again: ";
  }
  return choice;
}

// Run one hit through a chain and narrate the receipt. The receipt is the
// point: the player always sees WHY the number is what it is.
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

// The hero's sword vs the goblin's rules. One chain per swing: build it,
// add the rules that apply right now, execute, let it go. Next swing gets
// a fresh one — that's how "rules that come and go" stay easy later.
int heroStrikes() {
  rpg::core::Chain<int> damage(std::vector<std::string>{"base", "final"});
  mustBeOk(damage.add("final", "tough-skin", [](int dmg) { return dmg - 1; }));
  return resolveDamage("Strike", kStrikeDamage, damage);
}

// The goblin's claws vs the hero, who has no defensive rules... yet.
// (Tutorial 04 gives the hero Block — a new modifier in this chain, and
// nothing else in the game changes.)
int goblinClaws() {
  const rpg::core::Chain<int> damage(std::vector<std::string>{"base", "final"});
  return resolveDamage("Claws", kClawsDamage, damage);
}

}  // namespace

int main() {
  Fighter hero{.name = "Hero", .hp = kHeroHp};
  Fighter goblin{.name = "Goblin", .hp = kGoblinHp};

  std::cout << "A goblin blocks your path!\n";

  // The turn loop: show the world, ask the player, apply the rules,
  // check for an ending. Every game you've ever played is this loop.
  while (true) {
    std::cout << '\n'
              << hero.name << ": " << hero.hp << " HP   " << goblin.name << ": " << goblin.hp
              << " HP\n";
    std::cout << "[1] Strike   [0] Run away\n> ";

    const int choice = readChoice();
    if (choice == 0) {
      std::cout << "\nYou live to fight another day. The goblin cackles.\n";
      return 0;
    }
    if (choice != 1) {
      std::cout << "  no such move.\n";
      continue;
    }

    // Hero's turn.
    goblin.hp -= heroStrikes();
    if (goblin.hp <= 0) {
      std::cout << "\nThe goblin falls. Victory!\n";
      return 0;
    }

    // Goblin's turn.
    hero.hp -= goblinClaws();
    if (hero.hp <= 0) {
      std::cout << "\nYou crumple. The goblin wins this time...\n";
      return 0;
    }
  }
}
