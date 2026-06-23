// The v1 acceptance flow as a machine-checked test (examples/strike is the
// human-readable twin): an Action fires, an Effect modifies it, the chain's
// breakdown names the contributor — and removing the effect reverts
// everything. All four core pieces composing.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "rpg/core/action.hpp"
#include "rpg/core/bus.hpp"
#include "rpg/core/chain.hpp"
#include "rpg/core/effect.hpp"
#include "rpg/core/entity.hpp"
#include "rpg/core/status.hpp"
#include "rpg/core/topic.hpp"

namespace rpg::core {
namespace {

struct DamageEvent {
  EntityRef attacker;
  EntityRef target;
  int amount = 0;
};

const TopicDef<DamageEvent>& damageTopic() {
  static const TopicDef<DamageEvent> kDef{"combat.damage"};
  return kDef;
}

class testBleedEffect : public Effect {
 public:
  explicit testBleedEffect(const std::string& source) : Effect("bleed-" + source, source) {}

 protected:
  Status onApply(Bus& bus) override {
    track(damageTopic().onChained(bus).subscribe(
        [id = id(), source = source()](const DamageEvent&, Chain<DamageEvent>& chain) {
          return chain.add({.stage = "effects",
                            .id = id,
                            .modifier =
                                [](DamageEvent data) {
                                  data.amount += 2;
                                  return data;
                                },
                            .source = source});
        }));
    return Status::ok();
  }
};

struct StrikeInput {
  EntityRef target;
};

// Resolves a strike and reports the result for assertions.
class testStrikeAction : public Action<StrikeInput> {
 public:
  explicit testStrikeAction(Bus& bus) : Action("strike-1", "strike"), bus_(&bus) {}

  [[nodiscard]] const Chain<DamageEvent>::Result& lastResult() const { return lastResult_; }

  [[nodiscard]] Status canActivate(const EntityRef& /*owner*/,
                                   const StrikeInput& /*input*/) override {
    return Status::ok();
  }

  [[nodiscard]] Status activate(ActivateParams params) override {
    const EntityRef& owner = params.owner;
    const StrikeInput& input = params.input;
    Chain<DamageEvent> chain(std::vector<std::string>{"base", "effects", "final"});
    const DamageEvent swing{.attacker = owner, .target = input.target, .amount = 6};
    Status collected = damageTopic().onChained(*bus_).publish(swing, chain);
    if (!collected.isOk()) {
      return collected;
    }
    lastResult_ = chain.execute(swing);
    return Status::ok();
  }

 private:
  Bus* bus_;
  Chain<DamageEvent>::Result lastResult_;
};

TEST(StrikeIntegrationTest, EffectModifiesActionAndRemovalReverts) {
  Bus bus;
  const EntityRef hero{.id = "hero", .type = "character"};
  const EntityRef goblin{.id = "goblin", .type = "monster"};
  testStrikeAction strike(bus);

  // No effects: base damage straight through, empty receipt.
  ASSERT_TRUE(strike.activate({.owner = hero, .input = StrikeInput{.target = goblin}}).isOk());
  EXPECT_EQ(strike.lastResult().value.amount, 6);
  EXPECT_TRUE(strike.lastResult().breakdown.empty());

  // Bleed applied: the strike gets stronger, and the receipt says why.
  testBleedEffect bleed("spider-bite");
  ASSERT_TRUE(bleed.apply({.bus = bus}).isOk());
  ASSERT_TRUE(strike.activate({.owner = hero, .input = StrikeInput{.target = goblin}}).isOk());
  EXPECT_EQ(strike.lastResult().value.amount, 8);
  ASSERT_EQ(strike.lastResult().breakdown.size(), 1U);
  EXPECT_EQ(strike.lastResult().breakdown.at(0).id, "bleed-spider-bite");
  EXPECT_EQ(strike.lastResult().breakdown.at(0).stage, "effects");
  EXPECT_EQ(strike.lastResult().breakdown.at(0).source, "spider-bite");

  // Bleed removed: everything reverts. Neither class referenced the other
  // at any point — the bus + chain composed them.
  ASSERT_TRUE(bleed.remove().isOk());
  ASSERT_TRUE(strike.activate({.owner = hero, .input = StrikeInput{.target = goblin}}).isOk());
  EXPECT_EQ(strike.lastResult().value.amount, 6);
  EXPECT_TRUE(strike.lastResult().breakdown.empty());
}

}  // namespace
}  // namespace rpg::core
