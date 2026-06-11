#ifndef RPG_CORE_CHAIN_HPP_
#define RPG_CORE_CHAIN_HPP_

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "rpg/core/status.hpp"

namespace rpg::core {

// Per-resolution modifier collector (design decisions 4-7). Born for one
// attack, filled by subscribers, executed once, discarded. The chain stores
// FUNCTIONS, not results: execute() re-evaluates every modifier, which is
// what makes "Bless re-rolls its 1d4 each attack" free.
//
// Ordering authority: the stage list given at construction (declared once by
// the rulebook). Within a stage, insertion order. Contributors never know
// about each other — they only name a stage.
template <typename T>
class Chain {
 public:
  using Modifier = std::function<T(T)>;

  explicit Chain(std::vector<std::string> stages) : stages_(std::move(stages)) {}

  // One record per modifier applied: the breakdown is a first-class output
  // ("rage: +2, bless: +4"), not a debug afterthought.
  struct Step {
    std::string id;
    std::string stage;
    T before;
    T after;
  };
  struct Result {
    T value;
    std::vector<Step> breakdown;
  };

  // Duplicate ids are rejected so an effect can't stack by accident; the id
  // is also the removal key and the name on the breakdown receipt.
  Status add(std::string_view stage, std::string_view id, Modifier modifier) {
    if (std::ranges::find(stages_, stage) == stages_.end()) {
      return Status::error("unknown stage: " + std::string(stage));
    }
    const auto sameId = [id](const Entry& e) { return e.id == id; };
    if (std::ranges::any_of(entries_, sameId)) {
      return Status::error("duplicate modifier id: " + std::string(id));
    }
    entries_.push_back(
        {.stage = std::string(stage), .id = std::string(id), .modifier = std::move(modifier)});
    return Status::ok();
  }

  Status remove(std::string_view id) {
    const auto removed = std::erase_if(entries_, [id](const Entry& e) { return e.id == id; });
    if (removed == 0) {
      return Status::error("no modifier with id: " + std::string(id));
    }
    return Status::ok();
  }

  // Folds data through the stages in declared order; within a stage, in
  // insertion order. Each modifier is a pure T -> T transform over a value —
  // no modifier can see another's intermediate state except through the
  // value handed to it.
  [[nodiscard]] Result execute(T data) const {
    Result result{.value = std::move(data), .breakdown = {}};
    for (const std::string& stage : stages_) {
      for (const Entry& entry : entries_) {
        if (entry.stage != stage) {
          continue;
        }
        T before = result.value;
        result.value = entry.modifier(std::move(result.value));
        result.breakdown.push_back({.id = entry.id,
                                    .stage = entry.stage,
                                    .before = std::move(before),
                                    .after = result.value});
      }
    }
    return result;
  }

 private:
  struct Entry {
    std::string stage;
    std::string id;
    Modifier modifier;
  };

  std::vector<std::string> stages_;
  std::vector<Entry> entries_;  // insertion order preserved; execute groups by stage
};

}  // namespace rpg::core

#endif  // RPG_CORE_CHAIN_HPP_
