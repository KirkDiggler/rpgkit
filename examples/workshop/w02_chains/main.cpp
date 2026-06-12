#include <cstdio>
#include <rpg/core/chain.hpp>

int main() {
    using namespace rpg::core;

    // Stage list declared once — the rulebook's ordering authority
    Chain<int> chain{{"base", "features", "conditions", "equipment", "final"}};

    // Each contributor names its stage and provides a unique id
    (void)chain.add("features",  "rage",        [](int dmg) { return dmg + 2; });
    (void)chain.add("equipment", "magic_sword", [](int dmg) { return dmg + 1; });
    (void)chain.add("final",     "resistance",  [](int dmg) { return dmg / 2; });

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
    (void)fight2.add("features", "rage",      [](int dmg) { return dmg + 2; });
    (void)fight2.add("final",    "resistance", [](int dmg) { return dmg / 2; });

    auto result2 = fight2.execute(10);
    std::printf("Rage only, vs resistant:\n");
    for (const auto& step : result2.breakdown) {
        std::printf("  %s [%s]: %d -> %d\n",
            step.id.c_str(), step.stage.c_str(), step.before, step.after);
    }
    std::printf("Final: %d\n", result2.value);

    return 0;
}
