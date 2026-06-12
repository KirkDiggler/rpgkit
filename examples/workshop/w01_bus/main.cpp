#include <cstdio>
#include <rpg/core/bus.hpp>
#include <rpg/core/topic.hpp>

int main() {
    using namespace rpg::core;

    Bus bus;
    int total{0};

    // Define a topic and bind it to this bus
    auto damage_topic = TopicDef<int>{"damage_dealt"}.on(bus);

    // Subscribe — handler runs on every publish
    auto sub = damage_topic.subscribe([&](int dmg) {
        total += dmg;
        std::printf("  subscriber: +%d damage (total: %d)\n", dmg, total);
        return Status::ok();
    });

    // Publish — anyone can, no reference to the subscriber
    std::printf("Publishing 5 damage...\n");
    (void)damage_topic.publish(5);

    std::printf("Publishing 3 damage...\n");
    (void)damage_topic.publish(3);

    std::printf("Publishing 7 damage...\n");
    (void)damage_topic.publish(7);

    std::printf("Final total: %d\n", total);

    // Unsubscribe
    (void)bus.unsubscribe(sub);

    std::printf("After unsubscribe, publishing 10 (should be ignored)...\n");
    (void)damage_topic.publish(10);
    std::printf("Total still: %d (unchanged)\n", total);

    return 0;
}
