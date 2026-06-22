#include "rpg/core/topic.hpp"

#include <gtest/gtest.h>

#include <any>
#include <string>
#include <vector>

#include "rpg/core/bus.hpp"
#include "rpg/core/chain.hpp"
#include "rpg/core/status.hpp"

namespace rpg::core {
namespace {

// A real event shape, not just an int — proves whole structs ride the
// erased bus intact.
struct AttackEvent {
  std::string attacker;
  std::string target;
  int amount = 0;
};

// Topic definitions are static facts about the game, declared once.
const TopicDef<AttackEvent>& attackTopic() {
  static const TopicDef<AttackEvent> kDef{"combat.attack"};
  return kDef;
}

TEST(TopicTest, TypedPayloadArrivesIntact) {
  Bus bus;
  Topic<AttackEvent> topic = attackTopic().on(bus);

  AttackEvent received;
  topic.subscribe([&received](const AttackEvent& event) {
    received = event;
    return Status::ok();
  });

  const Status published =
      topic.publish(AttackEvent{.attacker = "hero", .target = "goblin", .amount = 6});

  EXPECT_TRUE(published.isOk());
  EXPECT_EQ(received.attacker, "hero");
  EXPECT_EQ(received.target, "goblin");
  EXPECT_EQ(received.amount, 6);
}

TEST(TopicTest, WrongTypePayloadIsAnErrorNotACrash) {
  // Game code can't hit this through the typed veneer — but raw bus access
  // or two defs sharing an id string could. The failure mode must be a
  // Status, never undefined behavior or a throw.
  Bus bus;
  Topic<AttackEvent> topic = attackTopic().on(bus);
  topic.subscribe([](const AttackEvent&) { return Status::ok(); });

  const Status published = bus.publish("combat.attack", std::any(42));

  EXPECT_FALSE(published.isOk());
  EXPECT_NE(published.message().find("combat.attack"), std::string::npos);
}

TEST(TopicTest, OneDefBindsManyBusesIndependently) {
  // The per-encounter-bus story (design decision 2): one static definition,
  // many isolated runtime buses.
  Bus busA;
  Bus busB;
  Topic<AttackEvent> onA = attackTopic().on(busA);
  Topic<AttackEvent> onB = attackTopic().on(busB);

  int aCount = 0;
  int bCount = 0;
  onA.subscribe([&aCount](const AttackEvent&) {
    ++aCount;
    return Status::ok();
  });
  onB.subscribe([&bCount](const AttackEvent&) {
    ++bCount;
    return Status::ok();
  });

  ASSERT_TRUE(onA.publish(AttackEvent{.attacker = "hero", .target = "goblin", .amount = 1}).isOk());

  EXPECT_EQ(aCount, 1);
  EXPECT_EQ(bCount, 0);
}

TEST(TopicTest, UnsubscribeThroughTheVeneer) {
  Bus bus;
  Topic<AttackEvent> topic = attackTopic().on(bus);

  int calls = 0;
  const SubscriptionId id = topic.subscribe([&calls](const AttackEvent&) {
    ++calls;
    return Status::ok();
  });

  ASSERT_TRUE(bus.unsubscribe(id).isOk());
  ASSERT_TRUE(topic.publish(AttackEvent{}).isOk());
  EXPECT_EQ(calls, 0);
}

TEST(ChainedTopicTest, PublishCollectsButNeverTransforms) {
  // The two-verbs rule (design decision 4): publish gathers modifiers into
  // the chain; only execute applies them.
  Bus bus;
  ChainedTopic<AttackEvent> topic = attackTopic().onChained(bus);

  topic.subscribe([](const AttackEvent& event, Chain<AttackEvent>& chain) {
    // A "rage" effect: sees the attack, registers its contribution.
    return chain.add(
        {.stage = "effects", .id = "rage-" + event.attacker, .modifier = [](AttackEvent data) {
           data.amount += 2;
           return data;
         }});
  });

  Chain<AttackEvent> chain(std::vector<std::string>{"base", "effects", "final"});
  const AttackEvent swing{.attacker = "hero", .target = "goblin", .amount = 5};

  ASSERT_TRUE(topic.publish(swing, chain).isOk());

  // Collected, not applied: the event is untouched until execute.
  EXPECT_EQ(swing.amount, 5);

  const Chain<AttackEvent>::Result result = chain.execute(swing);
  EXPECT_EQ(result.value.amount, 7);
  ASSERT_EQ(result.breakdown.size(), 1U);
  EXPECT_EQ(result.breakdown.at(0).id, "rage-hero");
}

TEST(ChainedTopicTest, MixingFlavorsOnOneIdFailsLoud) {
  // A notification subscriber and a chained publisher on the same id is a
  // wiring bug. It must surface as a Status error (payload type mismatch),
  // never as silent weirdness.
  Bus bus;
  Topic<AttackEvent> notification = attackTopic().on(bus);
  ChainedTopic<AttackEvent> chained = attackTopic().onChained(bus);

  notification.subscribe([](const AttackEvent&) { return Status::ok(); });

  Chain<AttackEvent> chain(std::vector<std::string>{"base"});
  const Status published = chained.publish(AttackEvent{}, chain);

  EXPECT_FALSE(published.isOk());
}

}  // namespace
}  // namespace rpg::core
