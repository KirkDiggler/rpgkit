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

  [[nodiscard]] int attackCalls() const { return attackCalls_; }
  [[nodiscard]] int turnCalls() const { return turnCalls_; }
  [[nodiscard]] SubscriptionId attackSubscription() const { return attackSub_; }

 protected:
  Status onApply(Bus& bus) override {
    attackSub_ = bus.subscribe("combat.attack", [this](const std::any&) {
      ++attackCalls_;
      return Status::ok();
    });
    track(attackSub_);
    track(bus.subscribe("turn.ended", [this](const std::any&) {
      ++turnCalls_;
      return Status::ok();
    }));
    return Status::ok();
  }

 private:
  int attackCalls_ = 0;
  int turnCalls_ = 0;
  SubscriptionId attackSub_;
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
  EXPECT_EQ(effect.attackCalls(), 1);
}

TEST(EffectTest, RemoveUnsubscribesEverythingTracked) {
  Bus bus;
  fakeBleedEffect effect;
  ASSERT_TRUE(effect.apply(bus).isOk());

  ASSERT_TRUE(effect.remove().isOk());

  EXPECT_FALSE(effect.isActive());
  ASSERT_TRUE(bus.publish("combat.attack", std::any(0)).isOk());
  ASSERT_TRUE(bus.publish("turn.ended", std::any(0)).isOk());
  EXPECT_EQ(effect.attackCalls(), 0);
  EXPECT_EQ(effect.turnCalls(), 0);
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
  EXPECT_EQ(effect.attackCalls(), 1);
}

TEST(EffectTest, RemoveSweepsEverythingEvenWhenOneUnsubscribeFails) {
  // If a tracked subscription vanished externally, remove() must still
  // sweep the rest and deactivate — half-removed-but-active is the one
  // state the lifecycle must never produce. The error is still reported.
  Bus bus;
  fakeBleedEffect effect;
  ASSERT_TRUE(effect.apply(bus).isOk());
  ASSERT_TRUE(bus.unsubscribe(effect.attackSubscription()).isOk());  // gone behind its back

  const Status removed = effect.remove();

  EXPECT_FALSE(removed.isOk());     // the unknown-id failure is surfaced...
  EXPECT_FALSE(effect.isActive());  // ...but the effect still fully deactivated
  ASSERT_TRUE(bus.publish("turn.ended", std::any(0)).isOk());
  EXPECT_EQ(effect.turnCalls(), 0);  // and the OTHER subscription was swept
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
