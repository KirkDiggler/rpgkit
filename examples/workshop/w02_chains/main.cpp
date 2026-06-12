#include <iostream>
#include <rpg/core/chain.hpp>

int main() {
  using namespace rpg::core;

  // Stage list declared once — the rulebook's ordering authority
  Chain<int> chain{{"base", "features", "conditions", "equipment", "final"}};
  constexpr int kBaseDamage = 10;

  // Each contributor names its stage and provides a unique id
  (void)chain.add("features", "rage", [](int dmg) { return dmg + 2; });
  (void)chain.add("equipment", "magic_sword", [](int dmg) { return dmg + 1; });
  (void)chain.add("final", "resistance", [](int dmg) { return dmg / 2; });

  // Execute folds base value through all stages
  auto result = chain.execute(kBaseDamage);

  std::cout << "Base damage: " << kBaseDamage << "\n";
  for (const auto& step : result.breakdown) {
    std::cout << "  " << step.id << " [" << step.stage << "]: " << step.before << " -> "
              << step.after << "\n";
  }
  std::cout << "Final: " << result.value << "\n\n";

  // Second example — different modifiers, same stages
  Chain<int> fight2{{"base", "features", "conditions", "equipment", "final"}};
  (void)fight2.add("features", "rage", [](int dmg) { return dmg + 2; });
  (void)fight2.add("final", "resistance", [](int dmg) { return dmg / 2; });

  auto result2 = fight2.execute(kBaseDamage);
  std::cout << "Rage only, vs resistant:\n";
  for (const auto& step : result2.breakdown) {
    std::cout << "  " << step.id << " [" << step.stage << "]: " << step.before << " -> "
              << step.after << "\n";
  }
  std::cout << "Final: " << result2.value << "\n";

  return 0;
}
