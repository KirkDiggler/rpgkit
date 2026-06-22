// Tutorial 04 — block: defense as a chain rule.
//
// This is tutorial 03's game, grown: a Defend card raises a shield, and the
// goblin's claws now resolve through a chain where YOUR block absorbs damage
// before HP — visible on the receipt like every other rule.
//
// Build & run (from the repo root):
//   make build
//   ./build/debug/examples/tutorials/04_block/tutorial_04_block
//
// What's new since 03:
//   1. A block stat on the character sheet, reset at the start of your turn.
//   2. A Defend card — one more DATA row, plus one new field cards can have.
//   3. The goblin's damage chain gains a "block-hero" rule that absorbs
//      damage AND spends the shield. First rule with a side effect — read
//      the note at goblinClaws about why that's safe here.
// Strike code, heal code, the turn loop's bones: unchanged. Defense arrived
// without touching offense — that's the chain doing its job.

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
constexpr int kDefendBlock = 5;

// A card is just its stats. New cards are new DATA — rows in a table a
// designer edits — not new code. (A card that needs genuinely new behavior
// earns a new field, and the play logic in playerPhase learns to read it.)
struct Card {
  std::string name;
  int cost = 0;
  int damage = 0;
  int heal = 0;
  int block = 0;  // new in tutorial 04: cards can raise a shield
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
      {.name = "Defend", .cost = 1, .block = kDefendBlock},
  };
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

struct Fighter {
  std::string name;
  int hp = 0;
  int maxHp = 0;
  int block = 0;  // shield points; absorb damage before HP, expire each turn
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
  mustBeOk(heal.add({
      .stage = "cap",
      .id = "max-hp-cap",
      .modifier = [room](int amount) { return amount > room ? room : amount; },
  }));
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
  mustBeOk(damage.add(
      {.stage = "final", .id = "tough-skin", .modifier = [](int dmg) { return dmg - 1; }}));
  return resolveDamage(card.name, card.damage, damage);
}

// The goblin's claws now meet the hero's shield. The block rule both
// transforms the value (damage minus absorbed) AND spends a resource
// (hero.block goes down) — our first modifier with a side effect. That's
// safe here for one reason: a chain lives for exactly ONE resolution and is
// executed exactly once, so the spend happens exactly once. What a modifier
// must still never do is edit the value other rules are folding behind the
// chain's back — the transform itself stays honest.
int goblinClaws(Fighter& hero) {
  rpg::core::Chain<int> damage(std::vector<std::string>{"base", "block"});
  if (hero.block > 0) {
    mustBeOk(damage.add({
        .stage = "block",
        .id = "block-" + hero.name,
        .modifier =
            [&hero](int dmg) {
              const int absorbed = std::min(hero.block, dmg);
              hero.block -= absorbed;
              return dmg - absorbed;
            },
    }));
  }
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
    if (hand.at(i).block > 0) {
      std::cout << ", " << hand.at(i).block << " block";
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
    if (card.block > 0) {
      hero.block += card.block;
      std::cout << "  " << card.name << ": +" << card.block << " block (shield now " << hero.block
                << ")\n";
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
    // Shields don't stack across turns (the Slay-the-Spire rule): whatever
    // block survived the goblin's attack expires at the START of your turn —
    // cleared BEFORE the status line prints, so the UI never shows a shield
    // that's already gone. A fresh Defend still protects you through the
    // goblin's reply.
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

    // ---- Goblin phase: unchanged from tutorial 02. ----
    hero.hp -= goblinClaws(hero);
    if (hero.hp <= 0) {
      std::cout << "\nYou crumple. The goblin shuffles your deck into its loot bag.\n";
      return 0;
    }
  }
}
