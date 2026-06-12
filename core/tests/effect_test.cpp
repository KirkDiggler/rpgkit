#include "rpg/core/effect.hpp"

#include <gtest/gtest.h>

#include <any>
#include <string>

#include "rpg/core/bus.hpp"
#include "rpg/core/status.hpp"

namespace rpg::core {
namespace {

// Hand-written fake (not gmock — this is a base-class exercise): an effect
// that subscribes to TWO topics, so remove() can prove it cleans up
// everything it tracked, not just the first thing.
class fakeBleedEffect : public Effect {
 public:
  fakeBleedEffect() : Effect("bleed-1", "spider-bite") {}

  int attackCalls = 0;
  int turnCalls = 0;

 protected:
  Status onApply(Bus& bus) override {
    track(bus.subscribe("combat.attack", [this](const std::any&) {
      ++attackCalls;
      return Status::ok();
    }));
    track(bus.subscribe("turn.ended", [this](const std::any&) {
      ++turnCalls;
      return Status::ok();
    }));
    return Status::ok();
  }
};

// An effect whose setup fails partway — apply() must clean up the half-done
// subscriptions and leave the effect inactive.
class fakeBrokenEffect : public Effect {
 public:
  fakeBrokenEffect() : Effect("broken-1", "test") {}

  int calls = 0;

 protected:
  Status onApply(Bus& bus) override {
    track(bus.subscribe("combat.attack", [this](const std::any&) {
      ++calls;
      return Status::ok();
    }));
    return Status::error("setup exploded after one subscription");
  }
};

TEST(EffectTest, HasIdentityAndSource) {
  const fakeBleedEffect effect;
  EXPECT_EQ(effect.id(), "bleed-1");
  EXPECT_EQ(effect.source(), "spider-bite");
  EXPECT_FALSE(effect.isActive());
}

TEST(EffectTest, ApplySubscribesAndMarksActive) {
  Bus bus;
  fakeBleedEffect effect;

  ASSERT_TRUE(effect.apply(bus).isOk());

  EXPECT_TRUE(effect.isActive());
  ASSERT_TRUE(bus.publish("combat.attack", std::any(0)).isOk());
  EXPECT_EQ(effect.attackCalls, 1);
}

TEST(EffectTest, RemoveUnsubscribesEverythingTracked) {
  Bus bus;
  fakeBleedEffect effect;
  ASSERT_TRUE(effect.apply(bus).isOk());

  ASSERT_TRUE(effect.remove().isOk());

  EXPECT_FALSE(effect.isActive());
  ASSERT_TRUE(bus.publish("combat.attack", std::any(0)).isOk());
  ASSERT_TRUE(bus.publish("turn.ended", std::any(0)).isOk());
  EXPECT_EQ(effect.attackCalls, 0);
  EXPECT_EQ(effect.turnCalls, 0);
}

TEST(EffectTest, DoubleApplyIsError) {
  Bus bus;
  fakeBleedEffect effect;
  ASSERT_TRUE(effect.apply(bus).isOk());

  EXPECT_FALSE(effect.apply(bus).isOk());
  EXPECT_TRUE(effect.isActive());  // still active from the first apply
}

TEST(EffectTest, RemoveWithoutApplyIsError) {
  fakeBleedEffect effect;
  EXPECT_FALSE(effect.remove().isOk());
}

TEST(EffectTest, ApplyCanRepeatAfterRemove) {
  // The Bless-for-three-turns story: effects cycle on and off the bus.
  Bus bus;
  fakeBleedEffect effect;
  ASSERT_TRUE(effect.apply(bus).isOk());
  ASSERT_TRUE(effect.remove().isOk());

  ASSERT_TRUE(effect.apply(bus).isOk());

  EXPECT_TRUE(effect.isActive());
  ASSERT_TRUE(bus.publish("combat.attack", std::any(0)).isOk());
  EXPECT_EQ(effect.attackCalls, 1);
}

TEST(EffectTest, FailedApplyCleansUpAndStaysInactive) {
  Bus bus;
  fakeBrokenEffect effect;

  const Status applied = effect.apply(bus);

  EXPECT_FALSE(applied.isOk());
  EXPECT_FALSE(effect.isActive());
  // The half-done subscription was rolled back: publishing reaches nothing.
  ASSERT_TRUE(bus.publish("combat.attack", std::any(0)).isOk());
  EXPECT_EQ(effect.calls, 0);
}

}  // namespace
}  // namespace rpg::core
