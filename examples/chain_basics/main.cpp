// chain_basics — cookbook recipe #1: resolve damage through a staged chain
// and read the breakdown receipt.
//
// Build & run (from the repo root):
//   make build
//   ./build/debug/examples/chain_basics/chain_basics
//
// What this shows:
//   1. The rulebook declares stage order ONCE ("base" -> "effects" -> "final").
//   2. Contributors (rage, bless, vulnerable) each register one modifier into
//      a named stage. None of them knows the others exist.
//   3. execute() folds the value through the stages and returns a receipt:
//      who changed what, in order.
//   4. The chain stores FUNCTIONS, not results — bless re-rolls its d4 every
//      execute. Run the program twice and watch the bless line change.

#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "rpg/core/chain.hpp"
#include "rpg/core/status.hpp"

namespace {

// Example-grade error handling: stop loudly. A real game would inspect the
// Status and recover; recipes keep the happy path readable.
void mustBeOk(const rpg::core::Status& status) {
  if (!status.isOk()) {
    std::cerr << "error: " << status.message() << '\n';
    std::exit(EXIT_FAILURE);
  }
}

}  // namespace

int main() {
  // The stage list is the rulebook's single source of ordering truth.
  // "Vulnerable applies after bonuses" is guaranteed HERE — not by add()
  // order, and not by the contributors coordinating.
  rpg::core::Chain<int> damage(std::vector<std::string>{"base", "effects", "final"});

  // Rage: flat +2 in "effects". The id ("rage") is stable and unique — it
  // powers dedup (can't rage twice), removal (rage ends -> pull it), and
  // names the line on the receipt.
  mustBeOk(damage.add("effects", "rage", [](int dmg) { return dmg + 2; }));

  // Bless: +1d4 in "effects". Captured rng lives as long as the lambda does;
  // the die rolls inside the modifier, so every execute() rolls fresh.
  std::mt19937 rng{std::random_device{}()};
  mustBeOk(damage.add("effects", "bless", [&rng](int dmg) {
    std::uniform_int_distribution<int> d4{1, 4};
    return dmg + d4(rng);
  }));

  // Vulnerable: x2 in "final" — added last here, but it would apply last even
  // if it had been added first. Stage order beats add order.
  mustBeOk(damage.add("final", "vulnerable", [](int dmg) { return dmg * 2; }));

  // One sword swing: 5 base damage in, receipt out.
  const rpg::core::Chain<int>::Result result = damage.execute(5);

  std::cout << "Strike! base damage 5\n";
  for (const rpg::core::Chain<int>::Step& step : result.breakdown) {
    std::cout << "  " << step.id << " (" << step.stage << "): " << step.before << " -> "
              << step.after << '\n';
  }
  std::cout << "final damage: " << result.value << '\n';

  return 0;
}
