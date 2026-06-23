// Tutorial 08 — Effects: an autonomous subscriber that manages its own
// lifecycle. BleedEffect subscribes to turn.ended, ticks damage-over-time, and
// self-removes when its stacks hit zero. The game loop goes from orchestrator
// to announcer — it publishes turn.ended and lets effects respond.
//
// Build & run (from the repo root):
//   make build
//   ./build/debug/examples/tutorials/08_effects/tutorial_08_effects
//
// What's new since 07:
//   1. rpg::core::Effect base class — onApply, track(), remove(), lifecycle.
//      An Effect is born dormant. apply() runs onApply (which subscribes via
//      track()), remembers the bus, and marks active. remove() unsubscribes
//      everything tracked — automatically, on the right bus.
//   2. BleedEffect — subscribes to turn.ended only. Each tick deals `stacks`
//      damage, decrements, and self-removes at zero.
//   3. The Rend card — card.bleed > 0 creates a BleedEffect and applies it.
//   4. activeEffects list — held in main(), pruned after each turn, but never
//      managing effect state. The loop doesn't know what Bleed does.
//   5. The announcer shift: turnEndedTopic().publish(turnNumber) drives all
//      effect behavior. The loop never asks "is Bleed active?"
// What this tutorial does NOT include:
//   Bleed amplifying incoming damage — that's challenge 1, the payoff for
//   understanding the entire pipeline so far.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "rpg/core/bus.hpp"
#include "rpg/core/chain.hpp"
#include "rpg/core/effect.hpp"
#include "rpg/core/status.hpp"
#include "rpg/core/topic.hpp"

namespace {

constexpr int kHeroMaxHp = 20;
constexpr int kGoblinHp = 30;
constexpr int kEnergyPerTurn = 3;
constexpr int kGoblinEnergyPerTurn = 2;
constexpr int kHandSize = 4;
constexpr int kDefendBlock = 5;
constexpr int kCardDelayMs = 300;
constexpr int kBleedStacks = 3;

// ---- Events & topics ----

struct DamageAttempt {
  std::string target;
  int baseAmount;
};

const rpg::core::TopicDef<DamageAttempt>& damageTopic() {
  static const rpg::core::TopicDef<DamageAttempt> kDef{"combat.damage"};
  return kDef;
}

const rpg::core::TopicDef<int>& turnEndedTopic() {
  static const rpg::core::TopicDef<int> kDef{"turn.ended"};
  return kDef;
}

std::vector<std::string> damageStages() { return {"base", "effects", "final"}; }

// ---- Cards ----

struct Card {
  std::string name;
  int cost = 0;
  int damage = 0;
  int heal = 0;
  int block = 0;
  int bleed = 0;
};

std::vector<Card> playerCardPool() {
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  return {
      {.name = "Strike", .cost = 1, .damage = 6},
      {.name = "Rend", .cost = 2, .bleed = kBleedStacks},
      {.name = "Defend", .cost = 1, .block = kDefendBlock},
      {.name = "Bash", .cost = 2, .damage = 11},
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

// ---- Bleed Effect ----

// An autonomous status: subscribes to turn.ended to tick down and deal
// damage-over-time. Self-removes when stacks hit zero — no game loop code
// manages its lifecycle.
class BleedEffect : public rpg::core::Effect {
  static std::string makeId(const std::string& target) {
    static int nextId = 1;
    return "bleed-" + target + "-" + std::to_string(nextId++);
  }

 public:
  BleedEffect(Fighter& target, int stacks)
      : Effect(makeId(target.name), "rend"), target_(target), stacks_(stacks) {}

 protected:
  rpg::core::Status onApply(rpg::core::Bus& bus) override {
    std::cout << "  Bleed applied to " << target_.name << " (" << stacks_ << " stacks)\n";
    track(turnEndedTopic().on(bus).subscribe([this](const int& /*turn*/) { return onTurnEnd(); }));
    return rpg::core::Status::ok();
  }

 private:
  rpg::core::Status onTurnEnd() {
    std::cout << "  Bleed deals " << stacks_ << " damage to " << target_.name << '\n';
    target_.hp -= stacks_;
    --stacks_;
    if (stacks_ == 0) {
      std::cout << "  Bleed faded\n";
      auto [removeStatus, removeReceipt] = remove();
      (void)removeReceipt;
      return removeStatus;
    }
    return rpg::core::Status::ok();
  }

  Fighter& target_;
  int stacks_;
};

// ---- Damage resolution (Bus-aware, chained topic) ----

void printDamageReceipt(const std::string& attackName, int baseDamage,
                        const rpg::core::Chain<DamageAttempt>::Result& result) {
  std::cout << "  " << attackName << ": " << baseDamage << " damage\n";
  for (const rpg::core::Chain<DamageAttempt>::Step& step : result.breakdown) {
    std::cout << "    " << step.id << " (" << step.stage << "): " << step.before.baseAmount
              << " -> " << step.after.baseAmount << '\n';
  }
}

int resolveCardDamage(const Card& card, const std::string& targetName, rpg::core::Bus& bus) {
  rpg::core::Chain<DamageAttempt> chain(damageStages());

  const DamageAttempt attempt{.target = targetName, .baseAmount = card.damage};
  mustBeOk(damageTopic().onChained(bus).publish(attempt, chain));

  const rpg::core::Chain<DamageAttempt>::Result result = chain.execute(attempt);
  printDamageReceipt(card.name, card.damage, result);
  return result.value.baseAmount;
}

int resolveMonsterDamage(const Card& card, Fighter& hero, rpg::core::Bus& bus) {
  rpg::core::Chain<DamageAttempt> chain(damageStages());

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
    if (hand.at(i).bleed > 0) {
      std::cout << ", " << hand.at(i).bleed << " bleed";
    }
    std::cout << ")" << (played.at(i) ? "  [played]" : "") << '\n';
  }
  std::cout << "  [0] end turn\n> ";
}

enum class PhaseResult : std::uint8_t { kTurnOver, kGoblinDown, kQuit };

bool playCard(const Card& card, Fighter& hero, Fighter& goblin,
              std::vector<std::unique_ptr<rpg::core::Effect>>& activeEffects, rpg::core::Bus& bus) {
  if (card.bleed > 0) {
    auto effect = std::make_unique<BleedEffect>(goblin, card.bleed);
    auto [status, receipt] = effect->apply({.bus = bus});
    mustBeOk(status);
    std::cout << "  " << receipt.id << " applied to " << goblin.name << " ("
              << receipt.subscriptions.size() << " subscriptions)\n";
    activeEffects.push_back(std::move(effect));
    return false;
  }
  if (card.damage > 0) {
    goblin.hp -= resolveCardDamage(card, goblin.name, bus);
    if (goblin.hp <= 0) {
      return true;
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
  return false;
}

PhaseResult playerPhase(Fighter& hero, Fighter& goblin,
                        std::vector<std::unique_ptr<rpg::core::Effect>>& activeEffects,
                        std::mt19937& rng, rpg::core::Bus& bus) {
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

    if (playCard(card, hero, goblin, activeEffects, bus)) {
      return PhaseResult::kGoblinDown;
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

  // Active effects: held here, never managed by the game loop. Effects
  // subscribe and unsubscribe on the bus; the loop just publishes turn.ended
  // and prunes the passive (self-removed) ones.
  std::vector<std::unique_ptr<rpg::core::Effect>> activeEffects;

  int turnNumber = 0;
  while (true) {
    std::cout << '\n'
              << hero.name << ": " << hero.hp << "/" << hero.maxHp << " HP, " << hero.block
              << " block   " << goblin.name << ": " << goblin.hp << " HP\n";

    const PhaseResult result = playerPhase(hero, goblin, activeEffects, rng, bus);
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

    // Prune self-removed effects — the loop doesn't manage effect state,
    // it just cleans up deactivated ones.
    std::erase_if(activeEffects, [](const auto& e) { return !e->isActive(); });

    // Bleed's tick may have killed the goblin or the hero.
    if (goblin.hp <= 0) {
      std::cout << "\nThe goblin falls as its wounds claim it. Victory!\n";
      return 0;
    }
    if (hero.hp <= 0) {
      std::cout << "\nYou crumple. The goblin shuffles your deck into its loot bag.\n";
      return 0;
    }
  }
}