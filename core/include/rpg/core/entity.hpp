#ifndef RPG_CORE_ENTITY_HPP_
#define RPG_CORE_ENTITY_HPP_

#include <string>

namespace rpg::core {

// A reference to a game entity — id plus a rulebook-defined type string
// ("character", "monster"). The core routes references; what an entity IS
// lives entirely in the rulebook (the boundary rule).
struct EntityRef {
  std::string id;
  std::string type;
};

}  // namespace rpg::core

#endif  // RPG_CORE_ENTITY_HPP_
