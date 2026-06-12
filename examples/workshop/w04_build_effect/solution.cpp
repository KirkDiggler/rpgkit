#include <cstdio>

#include "rpg/core/bus.hpp"
#include "rpg/core/effect.hpp"
#include "rpg/core/topic.hpp"

// A Burning status that deals fire damage each turn until it expires.
class Burning : public rpg::core::Effect {
 public:
  explicit Burning(int stacks) : Effect("burning", "fire_spell"), stacks_(stacks) {}

 protected:
  rpg::core::Status onApply(rpg::core::Bus& bus) override {
    std::printf("🔥 Burning applied (%d stacks)\n", stacks_);
    track(rpg::core::TopicDef<int>{"turn.ended"}.on(bus).subscribe([this](const int& /*turn*/) {
      return onTurnEnd();
    }));
    return rpg::core::Status::ok();
  }

 private:
  int stacks_;

  rpg::core::Status onTurnEnd() {
    if (stacks_ <= 0) {
      return rpg::core::Status::ok();
    }
    std::printf("🔥 Burning deals %d fire damage\n", stacks_);
    --stacks_;
    if (stacks_ == 0) {
      std::printf("🔥 Burning expired\n");
      return remove();  // unsubscribe everything
    }
    return rpg::core::Status::ok();
  }
};

int main() {
  rpg::core::Bus bus;

  std::printf("=== Burning Effect Demo ===\n\n");

  Burning burn(3);
  (void)burn.apply(bus);

  for (int turn = 1; turn <= 5; ++turn) {
    std::printf("--- Turn %d ---\n", turn);
    (void)rpg::core::TopicDef<int>{"turn.ended"}.on(bus).publish(turn);
    std::printf("\n");
  }

  std::printf("Done. Effect self-removed at zero stacks.\n");
  return 0;
}
