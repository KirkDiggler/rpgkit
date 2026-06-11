#include "rpg/core/bus.hpp"

#include <gtest/gtest.h>

#include <any>
#include <string>
#include <vector>

#include "rpg/core/status.hpp"

namespace rpg::core {
namespace {

// A handler that records what it saw into `log` (captured by reference) and
// always succeeds. The [&log] capture means the lambda holds a reference to
// the test's local vector — safe here because the test outlives the bus.
Bus::Handler recorder(std::vector<std::string>& log, const std::string& name) {
  return [&log, name](const std::any& payload) {
    log.push_back(name + ":" + std::any_cast<std::string>(payload));
    return Status::ok();
  };
}

TEST(BusTest, PublishWithNoSubscribersIsOk) {
  Bus bus;
  const Status s = bus.publish("combat.attack", std::any(std::string("swing")));
  EXPECT_TRUE(s.isOk());
}

TEST(BusTest, SubscriberReceivesPayload) {
  Bus bus;
  std::vector<std::string> log;
  bus.subscribe("combat.attack", recorder(log, "a"));

  const Status s = bus.publish("combat.attack", std::any(std::string("swing")));

  EXPECT_TRUE(s.isOk());
  const std::vector<std::string> want = {"a:swing"};
  EXPECT_EQ(log, want);
}

TEST(BusTest, SubscribersCalledInSubscriptionOrder) {
  Bus bus;
  std::vector<std::string> log;
  bus.subscribe("combat.attack", recorder(log, "first"));
  bus.subscribe("combat.attack", recorder(log, "second"));
  bus.subscribe("combat.attack", recorder(log, "third"));

  ASSERT_TRUE(bus.publish("combat.attack", std::any(std::string("x"))).isOk());

  const std::vector<std::string> want = {"first:x", "second:x", "third:x"};
  EXPECT_EQ(log, want);
}

TEST(BusTest, FailFastStopsDeliveryAtFirstError) {
  Bus bus;
  std::vector<std::string> log;
  bus.subscribe("combat.attack", recorder(log, "before"));
  bus.subscribe("combat.attack", [](const std::any&) { return Status::error("handler exploded"); });
  bus.subscribe("combat.attack", recorder(log, "after"));

  const Status s = bus.publish("combat.attack", std::any(std::string("x")));

  EXPECT_FALSE(s.isOk());
  EXPECT_EQ(s.message(), "handler exploded");
  // "after" never ran: delivery stopped at the failing handler.
  const std::vector<std::string> want = {"before:x"};
  EXPECT_EQ(log, want);
}

TEST(BusTest, UnsubscribedHandlerNoLongerReceives) {
  Bus bus;
  std::vector<std::string> log;
  const SubscriptionId id = bus.subscribe("combat.attack", recorder(log, "a"));
  bus.subscribe("combat.attack", recorder(log, "b"));

  ASSERT_TRUE(bus.unsubscribe(id).isOk());
  ASSERT_TRUE(bus.publish("combat.attack", std::any(std::string("x"))).isOk());

  const std::vector<std::string> want = {"b:x"};
  EXPECT_EQ(log, want);
}

TEST(BusTest, UnsubscribeUnknownIdIsError) {
  Bus bus;
  EXPECT_FALSE(bus.unsubscribe(SubscriptionId{12345}).isOk());
}

TEST(BusTest, TopicsAreIndependent) {
  Bus bus;
  std::vector<std::string> log;
  bus.subscribe("combat.attack", recorder(log, "attack"));
  bus.subscribe("combat.heal", recorder(log, "heal"));

  ASSERT_TRUE(bus.publish("combat.attack", std::any(std::string("x"))).isOk());

  const std::vector<std::string> want = {"attack:x"};
  EXPECT_EQ(log, want);
}

TEST(BusTest, SubscribeDuringPublishDoesNotReceiveCurrentEvent) {
  // Delivery iterates a snapshot of the subscriber list. A handler that adds
  // a new subscription mid-publish must not have that new handler called for
  // the in-flight event (and must not corrupt the iteration underneath us —
  // in C++, growing a vector you're iterating is undefined behavior, so this
  // semantic is a memory-safety decision, not just a logic preference).
  Bus bus;
  std::vector<std::string> log;
  bus.subscribe("combat.attack", [&](const std::any&) {
    bus.subscribe("combat.attack", recorder(log, "late"));
    log.emplace_back("adder ran");
    return Status::ok();
  });

  ASSERT_TRUE(bus.publish("combat.attack", std::any(std::string("x"))).isOk());
  const std::vector<std::string> afterFirst = {"adder ran"};
  EXPECT_EQ(log, afterFirst);

  // The late subscriber is live for the NEXT publish.
  ASSERT_TRUE(bus.publish("combat.attack", std::any(std::string("y"))).isOk());
  const std::vector<std::string> afterSecond = {"adder ran", "adder ran", "late:y"};
  EXPECT_EQ(log, afterSecond);
}

}  // namespace
}  // namespace rpg::core
