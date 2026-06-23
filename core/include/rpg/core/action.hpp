#ifndef RPG_CORE_ACTION_HPP_
#define RPG_CORE_ACTION_HPP_

#include <string>
#include <utility>

#include "rpg/core/entity.hpp"
#include "rpg/core/status.hpp"

namespace rpg::core {

// The receipt from an Action::activate call. Populated even on failure so the
// host can log "action X failed at gate Y" with X's identity. The host routes
// this to its own sinks (UE logs, HUD, debug tools) — core owns the shape,
// not the routing.
struct ActionReceipt {
  std::string id;
  std::string type;
  std::string correlationId;  // echoed from ActivateParams; "" if unused
};

// The transient verb (design decision 10's other half): an Action fires
// once — gate, spend, publish — while an Effect persists and listens. A
// card is an Action; the Bleed it applies is an Effect.
//
// This is an abstract base class — C++'s explicit interface: subclasses
// MUST implement the pure-virtual (= 0) methods, and callers hold
// Action<TInput>& without knowing the concrete type. TInput is the
// rulebook's typed input (targeting, cost choices); the core never looks
// inside it.
template <typename TInput>
class Action {
 public:
  Action(std::string id, std::string type) : id_(std::move(id)), type_(std::move(type)) {}

  // Virtual destructor: deleting a subclass through a base pointer without
  // one is undefined behavior. Any class with virtual methods needs this.
  virtual ~Action() = default;

  // Identity types: an action is not a value — copying "strike-1" would
  // mean two things claiming the same identity. Copies and moves deleted.
  Action(const Action&) = delete;
  Action& operator=(const Action&) = delete;
  Action(Action&&) = delete;
  Action& operator=(Action&&) = delete;

  [[nodiscard]] const std::string& id() const { return id_; }
  [[nodiscard]] const std::string& type() const { return type_; }

  // The gate: cost, target validity, cooldown. Never mutates anything.
  // No receipt — the gate is a yes/no, not a lifecycle fact.
  [[nodiscard]] virtual Status canActivate(const EntityRef& owner, const TInput& input) = 0;

  // Params struct (binding decision 9): new receipt fields become defaulted
  // members instead of positional inserts, so the signature stays stable.
  // Reference members are intentional — the struct is a temporary that only
  // outlives the function call it's passed to.
  struct ActivateParams {
    const EntityRef& owner;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const TInput& input;     // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    // Caller-supplied; the host passes the same id to every core operation it
    // wants correlated. Core copies it onto the receipt; it never propagates
    // across operations. Defaulted so callers that don't correlate still compile.
    std::string correlationId = "";  // NOLINT(readability-redundant-string-init)
  };

  // The firing: spend the cost, publish events, apply effects. Implementations
  // re-check the gate first — callers may skip straight to activate.
  // Returns (Status, ActionReceipt) — the receipt is populated even on failure
  // so the host can log "action X failed at gate Y" with X's identity.
  [[nodiscard]] virtual std::pair<Status, ActionReceipt> activate(ActivateParams params) = 0;

 private:
  std::string id_;
  std::string type_;
};

}  // namespace rpg::core

#endif  // RPG_CORE_ACTION_HPP_
