#include "rpg/core/action.hpp"

#include <gtest/gtest.h>

#include <any>
#include <string>

#include "rpg/core/bus.hpp"
#include "rpg/core/entity.hpp"
#include "rpg/core/status.hpp"

namespace rpg::core {
namespace {

// The typed input is rulebook territory (design: targeting lives in TInput).
struct StrikeInput {
  std::string targetId;
  int cost = 1;
};

// Hand-written fake: the gate checks a resource, activate spends it and
// publishes. This is the Action contract's whole shape — transient verb
// that gates, spends, and announces.
class fakeStrikeAction : public Action<StrikeInput> {
 public:
  fakeStrikeAction(Bus& bus, int energy)
      : Action("strike-1", "strike"), bus_(&bus), energy_(energy) {}

  [[nodiscard]] Status canActivate(const EntityRef& /*owner*/, const StrikeInput& input) override {
    if (input.cost > energy_) {
      return Status::error("not enough energy");
    }
    return Status::ok();
  }

  [[nodiscard]] Status activate(ActivateParams params) override {
    const EntityRef& owner = params.owner;
    const StrikeInput& input = params.input;
    Status gate = canActivate(owner, input);
    if (!gate.isOk()) {
      return gate;
    }
    energy_ -= input.cost;
    return bus_->publish("combat.attack", std::any(input.targetId));
  }

  [[nodiscard]] int energy() const { return energy_; }

 private:
  Bus* bus_;
  int energy_;
};

TEST(ActionTest, HasIdentity) {
  Bus bus;
  const fakeStrikeAction strike(bus, 3);
  EXPECT_EQ(strike.id(), "strike-1");
  EXPECT_EQ(strike.type(), "strike");
}

TEST(ActionTest, GateRefusesWhenUnaffordable) {
  Bus bus;
  fakeStrikeAction strike(bus, 0);
  const EntityRef hero{.id = "hero-1", .type = "character"};

  EXPECT_FALSE(strike.canActivate(hero, StrikeInput{.targetId = "goblin", .cost = 1}).isOk());
}

TEST(ActionTest, ActivateSpendsAndPublishes) {
  Bus bus;
  std::string struckTarget;
  bus.subscribe("combat.attack", [&struckTarget](const std::any& payload) {
    struckTarget = std::any_cast<std::string>(payload);
    return Status::ok();
  });

  fakeStrikeAction strike(bus, 3);
  const EntityRef hero{.id = "hero-1", .type = "character"};

  ASSERT_TRUE(
      strike.activate({.owner = hero, .input = StrikeInput{.targetId = "goblin-7", .cost = 1}})
          .isOk());

  EXPECT_EQ(struckTarget, "goblin-7");
  EXPECT_EQ(strike.energy(), 2);
}

TEST(ActionTest, GatedActivateChangesNothing) {
  Bus bus;
  int published = 0;
  bus.subscribe("combat.attack", [&published](const std::any&) {
    ++published;
    return Status::ok();
  });

  fakeStrikeAction strike(bus, 0);
  const EntityRef hero{.id = "hero-1", .type = "character"};

  EXPECT_FALSE(
      strike.activate({.owner = hero, .input = StrikeInput{.targetId = "goblin", .cost = 1}})
          .isOk());
  EXPECT_EQ(published, 0);
  EXPECT_EQ(strike.energy(), 0);
}

}  // namespace
}  // namespace rpg::core
