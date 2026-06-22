#ifndef RPG_CORE_ACTION_HPP_
#define RPG_CORE_ACTION_HPP_

#include <string>
#include <utility>

#include "rpg/core/entity.hpp"
#include "rpg/core/status.hpp"

namespace rpg::core {

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
  [[nodiscard]] virtual Status canActivate(const EntityRef& owner, const TInput& input) = 0;

  // Params struct (binding decision 9): new receipt fields become defaulted
  // members instead of positional inserts, so the signature stays stable.
  // Reference members are intentional — the struct is a temporary that only
  // outlives the function call it's passed to.
  struct ActivateParams {
    const EntityRef& owner;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const TInput& input;     // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  };

  // The firing: spend the cost, publish events, apply effects. Implementations
  // re-check the gate first — callers may skip straight to activate.
  [[nodiscard]] virtual Status activate(ActivateParams params) = 0;

 private:
  std::string id_;
  std::string type_;
};

}  // namespace rpg::core

#endif  // RPG_CORE_ACTION_HPP_
