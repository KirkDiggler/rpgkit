// Proves the toolchain pipeline (CMake -> FetchContent gtest -> ctest), not real behavior.
#include "rpg/core/version.hpp"

#include <gtest/gtest.h>

namespace rpg::core {
namespace {

TEST(VersionTest, MatchesProjectVersion) { EXPECT_EQ(kVersion, "0.1.0"); }

}  // namespace
}  // namespace rpg::core
