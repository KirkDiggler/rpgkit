#include <iostream>
#include <rpg/core/bus.hpp>
#include <rpg/core/effect.hpp>
#include <rpg/core/topic.hpp>

// A Burning status that deals fire damage each turn until it expires.
class Burning : public rpg::core::Effect {
 public:
  explicit Burning(int stacks) : Effect("burning", "fire_spell"), stacks_(stacks) {}

 protected:
  rpg::core::Status onApply(rpg::core::Bus& bus) override {
    std::cout << "🔥 Burning applied (" << stacks_ << " stacks)\n";
    track(rpg::core::TopicDef<int>{"turn.ended"}.on(bus).subscribe(
        [this](const int& /*turn*/) { return onTurnEnd(); }));
    return rpg::core::Status::ok();
  }

 private:
  int stacks_;

  rpg::core::Status onTurnEnd() {
    if (stacks_ <= 0) {
      return rpg::core::Status::ok();
    }
    std::cout << "🔥 Burning deals " << stacks_ << " fire damage\n";
    --stacks_;
    if (stacks_ == 0) {
      std::cout << "🔥 Burning expired\n";
      return remove();  // unsubscribe everything
    }
    return rpg::core::Status::ok();
  }
};

int main() {
  rpg::core::Bus bus;

  std::cout << "=== Burning Effect Demo ===\n\n";

  Burning burn(3);
  (void)burn.apply(bus);

  constexpr int kMaxTurns = 5;
  for (int turn = 1; turn <= kMaxTurns; ++turn) {
    std::cout << "--- Turn " << turn << " ---\n";
    (void)rpg::core::TopicDef<int>{"turn.ended"}.on(bus).publish(turn);
    std::cout << "\n";
  }

  std::cout << "Done. Effect self-removed at zero stacks.\n";
  return 0;
}
