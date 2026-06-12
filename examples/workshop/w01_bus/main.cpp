#include <iostream>
#include <rpg/core/bus.hpp>
#include <rpg/core/topic.hpp>

int main() {
  using namespace rpg::core;

  Bus bus;
  int total{0};
  constexpr int kPublish1 = 5;
  constexpr int kPublish2 = 3;
  constexpr int kPublish3 = 7;
  constexpr int kPublishAfter = 10;

  // Define a topic and bind it to this bus
  auto damage_topic = TopicDef<int>{"damage_dealt"}.on(bus);

  // Subscribe — handler runs on every publish
  auto sub = damage_topic.subscribe([&](int dmg) {
    total += dmg;
    std::cout << "  subscriber: +" << dmg << " damage (total: " << total << ")\n";
    return Status::ok();
  });

  // Publish — anyone can, no reference to the subscriber
  std::cout << "Publishing " << kPublish1 << " damage...\n";
  (void)damage_topic.publish(kPublish1);

  std::cout << "Publishing " << kPublish2 << " damage...\n";
  (void)damage_topic.publish(kPublish2);

  std::cout << "Publishing " << kPublish3 << " damage...\n";
  (void)damage_topic.publish(kPublish3);

  std::cout << "Final total: " << total << "\n";

  // Unsubscribe
  (void)bus.unsubscribe(sub);

  std::cout << "After unsubscribe, publishing " << kPublishAfter << " (should be ignored)...\n";
  (void)damage_topic.publish(kPublishAfter);
  std::cout << "Total still: " << total << " (unchanged)\n";

  return 0;
}
