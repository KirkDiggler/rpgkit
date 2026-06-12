#ifndef RPG_CORE_VERSION_HPP_
#define RPG_CORE_VERSION_HPP_

#include <string_view>

namespace rpg::core {

// rpgkit release version. Kept in lockstep with the CMake project() version,
// enforced by version_test.
inline constexpr std::string_view kVersion = "0.1.1";

}  // namespace rpg::core

#endif  // RPG_CORE_VERSION_HPP_
