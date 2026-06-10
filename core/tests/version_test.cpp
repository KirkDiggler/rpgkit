// Proves the toolchain pipeline (CMake -> FetchContent gtest -> ctest), not real behavior.
#include "rpg/core/version.hpp"

#include <gtest/gtest.h>

namespace rpg::core {
namespace {

// RPGKIT_PROJECT_VERSION is injected by core/CMakeLists.txt from the CMake project() version.
TEST(VersionTest, MatchesProjectVersion) { EXPECT_EQ(kVersion, RPGKIT_PROJECT_VERSION); }

}  // namespace
}  // namespace rpg::core
