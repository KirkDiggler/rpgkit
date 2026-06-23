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

  [[nodiscard]] std::pair<Status, ActionReceipt> activate(ActivateParams params) override {
    const EntityRef& owner = params.owner;
    const StrikeInput& input = params.input;
    ActionReceipt receipt{
        .id = id(), .type = type(), .correlationId = std::move(params.correlationId)};
    Chain<DamageEvent> chain(std::vector<std::string>{"base", "effects", "final"});
    const DamageEvent swing{.attacker = owner, .target = input.target, .amount = 6};
    Status collected = damageTopic().onChained(*bus_).publish(swing, chain);
    if (!collected.isOk()) {
      return {collected, std::move(receipt)};
    }
    lastResult_ = chain.execute(swing);
    return {Status::ok(), std::move(receipt)};
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

  // No effects: base damage straight through, breakdown empty (receipt still
  // carries the action identity).
  auto [st1, receipt1] = strike.activate({.owner = hero, .input = StrikeInput{.target = goblin}});
  ASSERT_TRUE(st1.isOk());
  EXPECT_EQ(receipt1.id, "strike-1");
  EXPECT_EQ(receipt1.type, "strike");
  EXPECT_EQ(strike.lastResult().value.amount, 6);
  EXPECT_TRUE(strike.lastResult().breakdown.empty());

  // Bleed applied: the strike gets stronger, and the chain breakdown says why.
  testBleedEffect bleed("spider-bite");
  auto [applyStatus, applyReceipt] = bleed.apply({.bus = bus});
  ASSERT_TRUE(applyStatus.isOk());
  EXPECT_EQ(applyReceipt.id, "bleed-spider-bite");
  EXPECT_EQ(applyReceipt.source, "spider-bite");
  ASSERT_EQ(applyReceipt.subscriptions.size(), 1U);
  auto [st2, receipt2] = strike.activate({.owner = hero, .input = StrikeInput{.target = goblin}});
  ASSERT_TRUE(st2.isOk());
  EXPECT_EQ(receipt2.id, "strike-1");
  EXPECT_EQ(strike.lastResult().value.amount, 8);
  ASSERT_EQ(strike.lastResult().breakdown.size(), 1U);
  EXPECT_EQ(strike.lastResult().breakdown.at(0).id, "bleed-spider-bite");
  EXPECT_EQ(strike.lastResult().breakdown.at(0).stage, "effects");
  EXPECT_EQ(strike.lastResult().breakdown.at(0).source, "spider-bite");

  // Bleed removed: everything reverts. Neither class referenced the other
  // at any point — the bus + chain composed them.
  auto [removeStatus, removeReceipt] = bleed.remove();
  ASSERT_TRUE(removeStatus.isOk());
  EXPECT_EQ(removeReceipt.id, "bleed-spider-bite");
  EXPECT_EQ(removeReceipt.source, "spider-bite");
  auto [st3, receipt3] = strike.activate({.owner = hero, .input = StrikeInput{.target = goblin}});
  ASSERT_TRUE(st3.isOk());
  EXPECT_EQ(receipt3.id, "strike-1");
  EXPECT_EQ(strike.lastResult().value.amount, 6);
  EXPECT_TRUE(strike.lastResult().breakdown.empty());
}

}  // namespace
}  // namespace rpg::core
