// strike — the core v1 acceptance example, straight from the design doc:
//
//   "An Action that fires, and an Effect that modifies it — riding on the
//    Bus + Chain. Execute the chain, read the breakdown."
//
// Build & run (from the repo root):
//   make build
//   ./build/debug/examples/strike/strike
//
// Every piece of the nervous system, composed:
//   TopicDef    — "damage events exist, shaped like DamageEvent" (static)
//   Bus         — one per encounter; everything meets here
//   StrikeAction— the transient verb: fires, publishes, executes, reports
//   BleedEffect — the persistent listener: applies, contributes, removes
//   Chain       — fresh per swing; folds contributions; yields the receipt
//
// The punchline: StrikeAction and BleedEffect never reference each other.
// Apply the effect and strikes get stronger; remove it and they revert.
// That composition-without-coupling is the entire toolkit thesis.

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "rpg/core/action.hpp"
#include "rpg/core/bus.hpp"
#include "rpg/core/chain.hpp"
#include "rpg/core/effect.hpp"
#include "rpg/core/entity.hpp"
#include "rpg/core/status.hpp"
#include "rpg/core/topic.hpp"

namespace {

using rpg::core::Action;
using rpg::core::Bus;
using rpg::core::Chain;
using rpg::core::Effect;
using rpg::core::EntityRef;
using rpg::core::Status;
using rpg::core::TopicDef;

constexpr int kStrikeDamage = 6;
constexpr int kBleedBonus = 2;

// The event: immutable data, no behavior (an event never carries a chain).
struct DamageEvent {
  EntityRef attacker;
  EntityRef target;
  int amount = 0;
};

// The topic: a static fact about the game, defined once.
const TopicDef<DamageEvent>& damageTopic() {
  static const TopicDef<DamageEvent> kDef{"combat.damage"};
  return kDef;
}

// The rulebook's ordering authority, declared once.
std::vector<std::string> damageStages() { return {"base", "effects", "final"}; }

void mustBeOk(const Status& status) {
  if (!status.isOk()) {
    std::cerr << "error: " << status.message() << '\n';
    std::exit(EXIT_FAILURE);
  }
}

// The persistent listener. On apply it subscribes to the chained damage
// topic; whenever any strike resolves while it's active, it registers its
// +2 into the "effects" stage. It knows nothing about StrikeAction.
class BleedEffect : public Effect {
 public:
  explicit BleedEffect(const std::string& source) : Effect("bleed-" + source, source) {}

 protected:
  Status onApply(Bus& bus) override {
    track(damageTopic().onChained(bus).subscribe(
        [id = id()](const DamageEvent&, Chain<DamageEvent>& chain) {
          return chain.add("effects", id, [](DamageEvent data) {
            data.amount += kBleedBonus;
            return data;
          });
        }));
    return Status::ok();
  }
};

// The transient verb. activate(): fresh chain, publish (collect), execute
// (transform), narrate the receipt. It knows nothing about BleedEffect.
struct StrikeInput {
  EntityRef target;
};

class StrikeAction : public Action<StrikeInput> {
 public:
  explicit StrikeAction(Bus& bus) : Action("strike-1", "strike"), bus_(&bus) {}

  // Always ready in this demo: costs/cooldowns are rulebook concerns and
  // arrive with the rulebook, not the core (see the deck_brawler-style
  // energy gate in the tutorials for what goes here).
  [[nodiscard]] Status canActivate(const EntityRef& /*owner*/,
                                   const StrikeInput& /*input*/) override {
    return Status::ok();
  }

  [[nodiscard]] Status activate(const EntityRef& owner, const StrikeInput& input) override {
    Status gate = canActivate(owner, input);
    if (!gate.isOk()) {
      return gate;
    }

    // One resolution = one fresh chain (design decision 6).
    Chain<DamageEvent> chain(damageStages());
    const DamageEvent swing{.attacker = owner, .target = input.target, .amount = kStrikeDamage};

    // Publish COLLECTS contributions from whatever effects are listening...
    Status collected = damageTopic().onChained(*bus_).publish(swing, chain);
    if (!collected.isOk()) {
      return collected;
    }

    // ...and execute APPLIES them, yielding the receipt.
    const Chain<DamageEvent>::Result result = chain.execute(swing);

    std::cout << "  " << owner.id << " strikes " << input.target.id << ": " << swing.amount
              << " base damage\n";
    for (const Chain<DamageEvent>::Step& step : result.breakdown) {
      std::cout << "    " << step.id << " (" << step.stage << "): " << step.before.amount << " -> "
                << step.after.amount << '\n';
    }
    std::cout << "  total: " << result.value.amount << " damage\n";
    return Status::ok();
  }

 private:
  Bus* bus_;
};

}  // namespace

int main() {
  Bus bus;  // one encounter, one bus
  const EntityRef hero{.id = "hero", .type = "character"};
  const EntityRef goblin{.id = "goblin", .type = "monster"};

  StrikeAction strike(bus);

  std::cout << "-- no effects --\n";
  mustBeOk(strike.activate(hero, StrikeInput{.target = goblin}));

  std::cout << "\n-- bleed applied (the goblin bit back) --\n";
  BleedEffect bleed("spider-bite");
  mustBeOk(bleed.apply(bus));
  mustBeOk(strike.activate(hero, StrikeInput{.target = goblin}));

  std::cout << "\n-- bleed removed (the wound closed) --\n";
  mustBeOk(bleed.remove());
  mustBeOk(strike.activate(hero, StrikeInput{.target = goblin}));

  return 0;
}
