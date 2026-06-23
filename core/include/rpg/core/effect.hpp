#ifndef RPG_CORE_EFFECT_HPP_
#define RPG_CORE_EFFECT_HPP_

#include <string>
#include <utility>
#include <vector>

#include "rpg/core/bus.hpp"
#include "rpg/core/status.hpp"

namespace rpg::core {

// The receipt from an Effect::apply or Effect::remove call. Populated even on
// failure so the host can log which effect failed and what it had subscribed
// to. The host routes this to its own sinks (UE logs, HUD, debug tools).
struct EffectReceipt {
  std::string id;
  std::string source;
  std::vector<SubscriptionId> subscriptions;  // what was tracked (apply) or swept (remove)
  std::string correlationId;                  // echoed from ApplyParams/RemoveParams
};

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

  // Params structs (binding decision 9): new receipt fields become defaulted
  // members instead of positional inserts, so the signature stays stable.
  // remove has a zero-arg overload (delegates to remove(RemoveParams{})) so
  // internal effect self-removal (onTurnEnd) and callers that don't correlate
  // still compile without naming the struct.
  // Reference members are intentional — the struct is a temporary that only
  // outlives the function call it's passed to.
  struct ApplyParams {
    Bus& bus;                        // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    std::string correlationId = "";  // NOLINT(readability-redundant-string-init)
  };
  struct RemoveParams {
    std::string correlationId = "";  // NOLINT(readability-redundant-string-init)
  };

  [[nodiscard]] std::pair<Status, EffectReceipt> apply(ApplyParams params) {
    EffectReceipt receipt{.id = id_,
                          .source = source_,
                          .subscriptions = {},
                          .correlationId = std::move(params.correlationId)};
    if (active_) {
      return {Status::error("effect already active: " + id_), std::move(receipt)};
    }
    Bus& bus = params.bus;
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
      return {applied, std::move(receipt)};
    }
    receipt.subscriptions = tracked_;
    active_ = true;
    return {Status::ok(), std::move(receipt)};
  }

  [[nodiscard]] std::pair<Status, EffectReceipt> remove() { return remove(RemoveParams{}); }

  [[nodiscard]] std::pair<Status, EffectReceipt> remove(RemoveParams params) {
    EffectReceipt receipt{.id = id_,
                          .source = source_,
                          .subscriptions = {},
                          .correlationId = std::move(params.correlationId)};
    if (!active_ || bus_ == nullptr) {
      return {Status::error("effect not active: " + id_), std::move(receipt)};
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
    receipt.subscriptions = std::move(tracked_);
    bus_ = nullptr;
    active_ = false;
    return {firstError, std::move(receipt)};
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
