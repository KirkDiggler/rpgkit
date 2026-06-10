#ifndef RPG_CORE_BUS_HPP_
#define RPG_CORE_BUS_HPP_

#include <any>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "rpg/core/status.hpp"

namespace rpg::core {

// Opaque handle for one subscription. A wrapped integer rather than a bare
// std::uint64_t so callers can't do arithmetic on it, and so a future C ABI
// can pass it across the wire unchanged.
struct SubscriptionId {
  std::uint64_t value = 0;  // 0 is never issued: default-constructed = invalid

  friend bool operator==(const SubscriptionId&, const SubscriptionId&) = default;
};

// Type-erased event router: string topic id -> ordered subscriber list over
// an opaque payload. This is the small core the design says to port
// carefully; the typed Topic<T> veneer casts in/out, and game code never
// touches a std::any directly.
//
// Delivery policy (design decision 9): synchronous, subscription order,
// fail-fast on the first handler error. Delivery iterates a snapshot of the
// subscriber list, so handlers may subscribe/unsubscribe mid-publish without
// invalidating the iteration; changes take effect on the next publish.
class Bus {
 public:
  using Handler = std::function<Status(const std::any& payload)>;

  // Relevance filtering is the handler's job (design decision 8): every
  // subscriber on a topic sees every event on that topic.
  SubscriptionId subscribe(std::string_view topicId, Handler handler) {
    const SubscriptionId id{++lastId_};
    std::string key{topicId};
    topicById_.emplace(id.value, key);
    topics_[std::move(key)].push_back({.id = id, .handler = std::move(handler)});
    return id;
  }

  Status unsubscribe(SubscriptionId id) {
    const auto found = topicById_.find(id.value);
    if (found == topicById_.end()) {
      return Status::error("unknown subscription id: " + std::to_string(id.value));
    }
    auto& subscribers = topics_[found->second];
    std::erase_if(subscribers, [id](const Subscription& s) { return s.id == id; });
    topicById_.erase(found);
    return Status::ok();
  }

  Status publish(std::string_view topicId, const std::any& payload) {
    const auto found = topics_.find(std::string{topicId});
    if (found == topics_.end()) {
      return Status::ok();  // no subscribers is a non-event, not an error
    }
    // Copy the subscriber list before delivering: handlers that mutate the
    // bus during delivery would otherwise grow/shrink the vector we are
    // iterating, which is undefined behavior.
    const std::vector<Subscription> snapshot = found->second;
    for (const Subscription& subscriber : snapshot) {
      const Status result = subscriber.handler(payload);
      if (!result.isOk()) {
        return result;  // fail-fast: later subscribers never run
      }
    }
    return Status::ok();
  }

 private:
  struct Subscription {
    SubscriptionId id;
    Handler handler;
  };

  std::uint64_t lastId_ = 0;
  std::unordered_map<std::string, std::vector<Subscription>> topics_;
  // Reverse index so unsubscribe doesn't scan every topic.
  std::unordered_map<std::uint64_t, std::string> topicById_;
};

}  // namespace rpg::core

#endif  // RPG_CORE_BUS_HPP_
