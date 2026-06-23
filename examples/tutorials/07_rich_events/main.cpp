// Tutorial 07 — rich events: damage carries context, subscribers contribute
// modifiers.
//
// This is tutorial 06's game, grown: damage is no longer a bare int traveling
// through a chain. It's a DamageAttempt — a struct that says who's being hit
// and for how much. Subscribers on a chained topic see the target and decide
// whether to modify the damage. The game plays identically to tutorial 06;
// this is a plumbing refactor that makes Effects possible (tutorial 08).
//
// Build & run (from the repo root):
//   make build
//   ./build/debug/examples/tutorials/07_rich_events/tutorial_07_rich_events
//
// What's new since 06:
//   1. DamageAttempt — damage carries target and baseAmount, not just a number.
//   2. combat.damage — a chained topic where subscribers register modifiers.
//   3. damageStages() — {"base", "effects", "final"}, shared across every
//      resolution. The "effects" stage is empty in this tutorial (no active
//      effects yet), but it's where Bleed will contribute in tutorial 08.
//   4. Tough-skin is now a subscriber on combat.damage — it checks the target
//      and only reduces damage against the goblin (not the hero).
//   5. Block is still an inline chain modifier in resolveMonsterDamage (it
//      needs mutable hero state), but it now operates on DamageAttempt.
//   6. resolveCardDamage and resolveMonsterDamage publish to the chained topic
//      to collect contributions, then execute the chain locally.
// What did NOT change: no Effects, no Bleed, no Rend, no behavior change.
// Healing stays Chain<int> (no effect modifies it yet).

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

#include "rpg/core/bus.hpp"
#include "rpg/core/chain.hpp"
#include "rpg/core/status.hpp"
#include "rpg/core/topic.hpp"

namespace {

// The tuning table: every game number, named, in one place.
constexpr int kHeroMaxHp = 20;
constexpr int kGoblinHp = 30;
constexpr int kEnergyPerTurn = 3;
constexpr int kGoblinEnergyPerTurn = 2;
constexpr int kHandSize = 4;
constexpr int kDefendBlock = 5;
constexpr int kCardDelayMs = 300;

// ---- Events & topics ----

// Damage now carries context: who's being hit, and for how much. This is the
// key shift from Chain<int> to Chain<DamageAttempt>. Subscribers can read the
// target and decide "do I modify this?" — the chain doesn't know who's
// subscribed, it just runs whatever modifiers it has.
struct DamageAttempt {
  std::string target;
  int baseAmount;
};

// A chained topic: every resolution publishes here, effects register
// modifiers, and the chain does the fold. This is the second kind of topic
// (the first was turn.ended, a notification topic). Chained topics let
// subscribers contribute to a resolution, not just observe it.
const rpg::core::TopicDef<DamageAttempt>& damageTopic() {
  static const rpg::core::TopicDef<DamageAttempt> kDef{"combat.damage"};
  return kDef;
}

// A notification topic: the turn loop broadcasts, subscribers respond.
const rpg::core::TopicDef<int>& turnEndedTopic() {
  static const rpg::core::TopicDef<int> kDef{"turn.ended"};
  return kDef;
}

// The stage order any damage chain follows. Fixed once per ruleset, passed
// to every chain constructor so every receipt reads the same way.
std::vector<std::string> damageStages() { return {"base", "effects", "final"}; }

// ---- Cards ----

struct Card {
  std::string name;
  int cost = 0;
  int damage = 0;
  int heal = 0;
  int block = 0;
};

std::vector<Card> playerCardPool() {
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  return {
      {.name = "Strike", .cost = 1, .damage = 6},
      {.name = "Bash", .cost = 2, .damage = 11},
      {.name = "Salve", .cost = 1, .heal = 4},
      {.name = "Defend", .cost = 1, .block = kDefendBlock},
  };
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

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

// ---- Damage resolution (Bus-aware, chained topic) ----

void printDamageReceipt(const std::string& attackName, int baseDamage,
                        const rpg::core::Chain<DamageAttempt>::Result& result) {
  std::cout << "  " << attackName << ": " << baseDamage << " damage\n";
  for (const rpg::core::Chain<DamageAttempt>::Step& step : result.breakdown) {
    std::cout << "    " << step.id << " (" << step.stage << "): " << step.before.baseAmount
              << " -> " << step.after.baseAmount << '\n';
  }
}

// Resolve damage from a player's card. Tough-skin contributes via the chained
// topic (it subscribes at startup, not inline here). The chain collects
// contributions from any active effects (Bleed in tutorial 08), then the caller
// executes locally.
int resolveCardDamage(const Card& card, const std::string& targetName, rpg::core::Bus& bus) {
  rpg::core::Chain<DamageAttempt> chain(damageStages());

  const DamageAttempt attempt{.target = targetName, .baseAmount = card.damage};
  // Publish to collect contributions from subscribers (tough-skin, etc.).
  mustBeOk(damageTopic().onChained(bus).publish(attempt, chain));

  const rpg::core::Chain<DamageAttempt>::Result result = chain.execute(attempt);
  printDamageReceipt(card.name, card.damage, result);
  return result.value.baseAmount;
}

// Resolve damage from a monster's card. Block is still handled here (it's a
// per-resolution modifier tied to the hero's current shield state, not a
// subscriber on the bus — it's added inline to the chain before publish).
int resolveMonsterDamage(const Card& card, Fighter& hero, rpg::core::Bus& bus) {
  rpg::core::Chain<DamageAttempt> chain(damageStages());

  // Block absorbs first — spends the shield, reduces the damage. This is a
  // per-resolution modifier, not a bus subscriber, because it needs to mutate
  // the hero's block state.
  if (hero.block > 0) {
    mustBeOk(chain.add({
        .stage = "final",
        .id = "block-" + hero.name,
        .modifier =
            [&hero](DamageAttempt data) {
              const int absorbed = std::min(hero.block, data.baseAmount);
              hero.block -= absorbed;
              data.baseAmount -= absorbed;
              return data;
            },
    }));
  }

  const DamageAttempt attempt{.target = hero.name, .baseAmount = card.damage};
  mustBeOk(damageTopic().onChained(bus).publish(attempt, chain));

  const rpg::core::Chain<DamageAttempt>::Result result = chain.execute(attempt);
  printDamageReceipt(card.name, card.damage, result);
  return result.value.baseAmount;
}

// ---- Healing (unchanged, no Bus needed) ----

int resolveHeal(const std::string& cardName, int baseHeal, const Fighter& target) {
  rpg::core::Chain<int> heal(std::vector<std::string>{"base", "cap"});
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

// ---- Card helpers ----

std::vector<Card> dealHand(const std::vector<Card>& pool, std::mt19937& rng) {
  std::uniform_int_distribution<std::size_t> pick{0, pool.size() - 1};
  std::vector<Card> hand;
  hand.reserve(kHandSize);
  for (int i = 0; i < kHandSize; ++i) {
    hand.push_back(pool.at(pick(rng)));
  }
  return hand;
}

// ---- Player phase ----

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

PhaseResult playerPhase(Fighter& hero, Fighter& goblin, std::mt19937& rng, rpg::core::Bus& bus) {
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
      goblin.hp -= resolveCardDamage(card, goblin.name, bus);
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

// ---- Goblin phase ----

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

bool goblinPhase(Fighter& goblin, Fighter& hero, std::mt19937& rng, rpg::core::Bus& bus) {
  goblin.block = 0;

  std::vector<Card> hand = dealHand(monsterCardPool(), rng);
  std::vector<bool> played(hand.size(), false);
  int energy = kGoblinEnergyPerTurn;

  std::cout << "\n  --- Goblin's turn (" << goblin.name << ": " << goblin.hp << " HP) ---\n";

  while (energy > 0) {
    const int pick = aiPick(hand, played, energy);
    if (pick == -1) {
      break;
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
      hero.hp -= resolveMonsterDamage(card, hero, bus);
      if (hero.hp <= 0) {
        return true;
      }
    }
    if (card.block > 0) {
      goblin.block += card.block;
      std::cout << "    " << goblin.name << " gains " << card.block << " block (shield now "
                << goblin.block << ")\n";
    }
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
  rpg::core::Bus bus;

  Fighter hero{.name = "Hero", .hp = kHeroMaxHp, .maxHp = kHeroMaxHp};
  Fighter goblin{.name = "Goblin", .hp = kGoblinHp, .maxHp = kGoblinHp};
  std::mt19937 rng{std::random_device{}()};

  // Tough-skin: the goblin has thick skin, reducing incoming damage by 1.
  // In tutorial 06 this was an inline lambda baked into cardDamage(). Now it's
  // a subscriber on the combat damage chained topic, and it checks the target —
  // only the goblin benefits from tough-skin, not the hero. This is the key
  // advantage of DamageAttempt over Chain<int>: subscribers can read the target
  // and decide whether to modify the damage.
  damageTopic().onChained(bus).subscribe(
      [&goblin](const DamageAttempt& event, rpg::core::Chain<DamageAttempt>& chain) {
        if (event.target != goblin.name) {
          return rpg::core::Status::ok();
        }
        mustBeOk(chain.add({
            .stage = "final",
            .id = "tough-skin",
            .modifier =
                [](DamageAttempt data) {
                  data.baseAmount = std::max(0, data.baseAmount - 1);
                  return data;
                },
        }));
        return rpg::core::Status::ok();
      });

  // Combat log — a notification subscriber, same as tutorial 06.
  turnEndedTopic().on(bus).subscribe([](const int& turn) {
    std::cout << "— Turn " << turn << " complete —\n";
    return rpg::core::Status::ok();
  });

  // Shield expiry — hero block resets at end of turn.
  turnEndedTopic().on(bus).subscribe([&hero](const int& /*turn*/) {
    hero.block = 0;
    std::cout << "  Shields expire.\n";
    return rpg::core::Status::ok();
  });

  std::cout << "A goblin blocks your path — and it's brought its own deck.\n";

  int turnNumber = 0;
  while (true) {
    std::cout << '\n'
              << hero.name << ": " << hero.hp << "/" << hero.maxHp << " HP, " << hero.block
              << " block   " << goblin.name << ": " << goblin.hp << " HP\n";

    const PhaseResult result = playerPhase(hero, goblin, rng, bus);
    if (result == PhaseResult::kQuit) {
      std::cout << "\nYou fold your hand and slip away. The goblin keeps your deck.\n";
      return 0;
    }
    if (result == PhaseResult::kGoblinDown) {
      std::cout << "\nThe goblin falls. Victory!\n";
      return 0;
    }

    if (goblinPhase(goblin, hero, rng, bus)) {
      std::cout << "\nYou crumple. The goblin shuffles your deck into its loot bag.\n";
      return 0;
    }

    ++turnNumber;
    mustBeOk(turnEndedTopic().on(bus).publish(turnNumber));

    if (hero.hp <= 0) {
      std::cout << "\nYou crumple. The goblin shuffles your deck into its loot bag.\n";
      return 0;
    }
    if (goblin.hp <= 0) {
      std::cout << "\nThe goblin falls as its wounds claim it. Victory!\n";
      return 0;
    }
  }
}