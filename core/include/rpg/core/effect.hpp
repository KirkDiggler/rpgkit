#ifndef RPG_CORE_EFFECT_HPP_
#define RPG_CORE_EFFECT_HPP_

#include <string>
#include <utility>
#include <vector>

#include "rpg/core/bus.hpp"
#include "rpg/core/status.hpp"

namespace rpg::core {

// The persistent listener (design decision 10): an Effect lives on the bus
// between resolutions. apply() runs the subclass's onApply (which subscribes
// via track()), remembers the bus, and marks active; remove() unsubscribes
// everything tracked — automatically, on the right bus, because remove takes
// no bus parameter: the wrong-bus mistake is unrepresentable.
//
// Lifetime precondition: the bus must outlive applied effects (true by
// construction — an encounter's bus outlives its effects).
class Effect {
 public:
  Effect(std::string id, std::string source) : id_(std::move(id)), source_(std::move(source)) {}

  virtual ~Effect() = default;

  // Identity types: see action.hpp — copies and moves deleted.
  Effect(const Effect&) = delete;
  Effect& operator=(const Effect&) = delete;
  Effect(Effect&&) = delete;
  Effect& operator=(Effect&&) = delete;

  [[nodiscard]] const std::string& id() const { return id_; }
  // What granted this effect ("spider-bite", "rage-spell") — powers the
  // stable modifier ids that make breakdowns readable.
  [[nodiscard]] const std::string& source() const { return source_; }
  [[nodiscard]] bool isActive() const { return active_; }

  [[nodiscard]] Status apply(Bus& bus) {
    if (active_) {
      return Status::error("effect already active: " + id_);
    }
    bus_ = &bus;
    tracked_.clear();
    Status applied = onApply(bus);
    if (!applied.isOk()) {
      // Roll back whatever half of setup happened. Best-effort: the
      // original error is what the caller needs; an unsubscribe failure
      // here would only mask it.
      for (const SubscriptionId sub : tracked_) {
        (void)bus.unsubscribe(sub);
      }
      tracked_.clear();
      bus_ = nullptr;
      return applied;
    }
    active_ = true;
    return Status::ok();
  }

  [[nodiscard]] Status remove() {
    if (!active_ || bus_ == nullptr) {
      return Status::error("effect not active: " + id_);
    }
    // Best-effort sweep: Bus::unsubscribe only fails for unknown ids (the
    // subscription is already gone), so never stop early — finish removing
    // the rest, clear state regardless, and report the first error. A
    // fail-fast here would leave the effect half-removed and still active.
    Status firstError = Status::ok();
    for (const SubscriptionId sub : tracked_) {
      Status removed = bus_->unsubscribe(sub);
      if (!removed.isOk() && firstError.isOk()) {
        firstError = std::move(removed);
      }
    }
    tracked_.clear();
    bus_ = nullptr;
    active_ = false;
    return firstError;
  }

 protected:
  // Subclasses subscribe their handlers here, registering every
  // SubscriptionId via track() so cleanup is automatic.
  virtual Status onApply(Bus& bus) = 0;

  void track(SubscriptionId id) { tracked_.push_back(id); }

 private:
  std::string id_;
  std::string source_;
  Bus* bus_ = nullptr;  // non-owning; set while active
  std::vector<SubscriptionId> tracked_;
  bool active_ = false;
};

}  // namespace rpg::core

#endif  // RPG_CORE_EFFECT_HPP_
