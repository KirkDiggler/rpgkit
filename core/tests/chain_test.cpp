#include "rpg/core/chain.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "rpg/core/status.hpp"

namespace rpg::core {
namespace {

// The rulebook declares stage order ONCE; contributors only name a stage.
// (A function, not a global vector: globals with constructors run before
// main() where exceptions cannot be caught — clang-tidy rightly objects.)
std::vector<std::string> kStages() { return {"base", "effects", "final"}; }

TEST(ChainTest, ExecuteWithNoModifiersReturnsInputUnchanged) {
  const Chain<int> chain(kStages());
  const Chain<int>::Result result = chain.execute(5);
  EXPECT_EQ(result.value, 5);
  EXPECT_TRUE(result.breakdown.empty());
}

TEST(ChainTest, StageOrderBeatsAddOrder) {
  // Add the "final" x2 BEFORE the "effects" +2. If add order ruled, 5*2+2=12.
  // Stage order rules: (5+2)*2 = 14. Rage and Vulnerable never coordinate;
  // the stage list composes them (design decision 5).
  Chain<int> chain(kStages());
  ASSERT_TRUE(
      chain.add({.stage = "final", .id = "vulnerable", .modifier = [](int v) { return v * 2; }})
          .isOk());
  ASSERT_TRUE(chain.add({.stage = "effects", .id = "rage", .modifier = [](int v) { return v + 2; }})
                  .isOk());

  EXPECT_EQ(chain.execute(5).value, 14);
}

TEST(ChainTest, WithinAStageInsertionOrderRules) {
  // Both in "effects": +3 then x2 -> (5+3)*2 = 16 (not 5*2+3 = 13).
  Chain<int> chain(kStages());
  ASSERT_TRUE(
      chain.add({.stage = "effects", .id = "bless", .modifier = [](int v) { return v + 3; }})
          .isOk());
  ASSERT_TRUE(
      chain.add({.stage = "effects", .id = "empower", .modifier = [](int v) { return v * 2; }})
          .isOk());

  EXPECT_EQ(chain.execute(5).value, 16);
}

TEST(ChainTest, BreakdownIsTheReceipt) {
  // The whole selling point: not just 14, but WHO did WHAT (decision 7).
  Chain<int> chain(kStages());
  ASSERT_TRUE(chain.add({.stage = "effects", .id = "rage", .modifier = [](int v) { return v + 2; }})
                  .isOk());
  ASSERT_TRUE(
      chain.add({.stage = "final", .id = "vulnerable", .modifier = [](int v) { return v * 2; }})
          .isOk());

  const Chain<int>::Result result = chain.execute(5);

  EXPECT_EQ(result.value, 14);
  ASSERT_EQ(result.breakdown.size(), 2U);
  EXPECT_EQ(result.breakdown.at(0).id, "rage");
  EXPECT_EQ(result.breakdown.at(0).stage, "effects");
  EXPECT_EQ(result.breakdown.at(0).before, 5);
  EXPECT_EQ(result.breakdown.at(0).after, 7);
  EXPECT_EQ(result.breakdown.at(1).id, "vulnerable");
  EXPECT_EQ(result.breakdown.at(1).stage, "final");
  EXPECT_EQ(result.breakdown.at(1).before, 7);
  EXPECT_EQ(result.breakdown.at(1).after, 14);
}

TEST(ChainTest, ModifiersReEvaluatePerExecute) {
  // "Bless re-rolls its 1d4 every attack" (decision 6): the chain stores the
  // FUNCTION, not its result. A stateful modifier proves each execute calls
  // it fresh.
  Chain<int> chain(kStages());
  int rolls = 0;
  ASSERT_TRUE(chain
                  .add({.stage = "effects",
                        .id = "bless",
                        .modifier = [&rolls](int v) { return v + (++rolls); }})
                  .isOk());

  EXPECT_EQ(chain.execute(10).value, 11);  // first "roll": +1
  EXPECT_EQ(chain.execute(10).value, 12);  // same chain, fresh evaluation: +2
}

TEST(ChainTest, DuplicateIdIsRejected) {
  // Ids power dedup: raging twice must not stack (decision 7).
  Chain<int> chain(kStages());
  ASSERT_TRUE(
      chain.add({.stage = "effects", .id = "rage-barb-1", .modifier = [](int v) { return v + 2; }})
          .isOk());

  const Status dup =
      chain.add({.stage = "effects", .id = "rage-barb-1", .modifier = [](int v) { return v + 2; }});
  EXPECT_FALSE(dup.isOk());
  EXPECT_EQ(chain.execute(0).value, 2);  // still applied exactly once
}

TEST(ChainTest, UnknownStageIsRejected) {
  Chain<int> chain(kStages());
  EXPECT_FALSE(
      chain.add({.stage = "equipment", .id = "ring", .modifier = [](int v) { return v; }}).isOk());
}

TEST(ChainTest, RemovePullsTheModifier) {
  // Effect ends -> its modifier leaves the chain (decision 7).
  Chain<int> chain(kStages());
  ASSERT_TRUE(chain.add({.stage = "effects", .id = "rage", .modifier = [](int v) { return v + 2; }})
                  .isOk());
  ASSERT_TRUE(chain.remove("rage").isOk());

  EXPECT_EQ(chain.execute(5).value, 5);
  EXPECT_FALSE(chain.remove("rage").isOk());  // already gone: error
}

TEST(ChainTest, WorksWithStructEventData) {
  // T is whatever the rulebook says. Modifiers transform copies — value
  // semantics means no modifier can corrupt another's view mid-flight.
  struct Damage {
    int amount = 0;
    bool critical = false;
  };
  Chain<Damage> chain(kStages());
  ASSERT_TRUE(chain
                  .add({.stage = "final",
                        .id = "crit",
                        .modifier =
                            [](Damage d) {
                              if (d.critical) {
                                d.amount *= 2;
                              }
                              return d;
                            }})
                  .isOk());

  const Chain<Damage>::Result result = chain.execute(Damage{.amount = 7, .critical = true});
  EXPECT_EQ(result.value.amount, 14);
}

}  // namespace
}  // namespace rpg::core
