#ifndef RPG_CORE_TOPIC_HPP_
#define RPG_CORE_TOPIC_HPP_

#include <any>
#include <functional>
#include <string>
#include <utility>

#include "rpg/core/bus.hpp"
#include "rpg/core/chain.hpp"
#include "rpg/core/status.hpp"

namespace rpg::core {

// The typed veneer over the erased Bus (design decision 1): the string id
// routes, the type T protects. Game code uses these; it never touches a
// std::any.
//
// Two flavors, distinct at the type level (decision 3):
//   Topic<T>        — notification: one-way, handlers observe a const event.
//   ChainedTopic<T> — chained: handlers REGISTER modifiers into a chain;
//                     transformation happens only at chain.execute().
// Mixing flavors on one topic id is a wiring bug; it surfaces as a payload
// type-mismatch Status at publish, never as silent corruption.
//
// Lifetime: a bound topic holds a reference to its bus and must not outlive
// it — bind topics where the bus lives (an encounter, a test).

// Notification flavor: subscribe observers, publish events.
template <typename T>
class Topic {
 public:
  Topic(Bus& bus, std::string id) : bus_(&bus), id_(std::move(id)) {}

  using Handler = std::function<Status(const T&)>;

  SubscriptionId subscribe(Handler handler) {
    // The typed-to-erased bridge. std::any_cast with a POINTER argument is
    // the exception-free checked cast: nullptr on mismatch instead of a
    // throw — which is what lets the no-exceptions rule hold here.
    return bus_->subscribe(id_, [handler = std::move(handler), id = id_](const std::any& payload) {
      const auto* event = std::any_cast<T>(&payload);
      if (event == nullptr) {
        return Status::error("wrong payload type on topic: " + id);
      }
      return handler(*event);
    });
  }

  [[nodiscard]] Status publish(const T& event) { return bus_->publish(id_, std::any(event)); }

 private:
  Bus* bus_;  // non-owning; the bus outlives its bound topics
  std::string id_;
};

// Chained flavor: handlers contribute modifiers; the caller executes.
template <typename T>
class ChainedTopic {
 public:
  ChainedTopic(Bus& bus, std::string id) : bus_(&bus), id_(std::move(id)) {}

  using Handler = std::function<Status(const T&, Chain<T>&)>;

  SubscriptionId subscribe(Handler handler) {
    return bus_->subscribe(id_, [handler = std::move(handler), id = id_](const std::any& payload) {
      const auto* envelope = std::any_cast<Envelope>(&payload);
      if (envelope == nullptr) {
        return Status::error("wrong payload type on chained topic: " + id);
      }
      return handler(*envelope->event, *envelope->chain);
    });
  }

  // Publish COLLECTS contributions into the chain — it never transforms.
  // Execute is the caller's verb, afterwards (design decision 4).
  [[nodiscard]] Status publish(const T& event, Chain<T>& chain) {
    return bus_->publish(id_, std::any(Envelope{.event = &event, .chain = &chain}));
  }

 private:
  // A chained handler needs two things, but the erased bus carries one
  // payload — so both ride in an envelope. The pointers live only for the
  // synchronous duration of one publish call; nothing retains them.
  struct Envelope {
    const T* event;
    Chain<T>* chain;
  };

  Bus* bus_;  // non-owning; the bus outlives its bound topics
  std::string id_;
};

// A topic definition: a static fact about the game ("attacks exist, shaped
// like AttackEvent"). Define once at namespace/static scope; bind to any
// number of runtime buses with on()/onChained() (design decision 2).
template <typename T>
class TopicDef {
 public:
  explicit TopicDef(std::string id) : id_(std::move(id)) {}

  [[nodiscard]] Topic<T> on(Bus& bus) const { return Topic<T>(bus, id_); }
  [[nodiscard]] ChainedTopic<T> onChained(Bus& bus) const { return ChainedTopic<T>(bus, id_); }
  [[nodiscard]] const std::string& id() const { return id_; }

 private:
  std::string id_;
};

}  // namespace rpg::core

#endif  // RPG_CORE_TOPIC_HPP_
