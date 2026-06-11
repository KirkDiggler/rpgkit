#include "rpg/core/status.hpp"

#include <gtest/gtest.h>

namespace rpg::core {
namespace {

TEST(StatusTest, OkIsOkWithEmptyMessage) {
  const Status s = Status::ok();
  EXPECT_TRUE(s.isOk());
  EXPECT_EQ(s.message(), "");
}

TEST(StatusTest, ErrorCarriesMessage) {
  const Status s = Status::error("duplicate modifier id: rage-barb-1");
  EXPECT_FALSE(s.isOk());
  EXPECT_EQ(s.message(), "duplicate modifier id: rage-barb-1");
}

TEST(StatusTest, EmptyMessageErrorIsStillAnError) {
  // ok-ness is a real flag, not "message happens to be empty".
  const Status s = Status::error("");
  EXPECT_FALSE(s.isOk());
}

TEST(StatusTest, CopyIsIndependent) {
  // Value semantics: assignment copies the whole object, string included.
  // There is no shared reference here — unlike Go/Java/C#, `copy` is not
  // another pointer to the same Status.
  const Status original = Status::error("boom");
  // The copy is the point of this test, not an accident:
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  const Status copy = original;
  EXPECT_FALSE(copy.isOk());
  EXPECT_EQ(copy.message(), "boom");
  EXPECT_EQ(original.message(), "boom");
}

}  // namespace
}  // namespace rpg::core
