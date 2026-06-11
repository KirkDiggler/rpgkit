#ifndef RPG_CORE_STATUS_HPP_
#define RPG_CORE_STATUS_HPP_

#include <string>
#include <utility>

namespace rpg::core {

// Result of a fallible core operation. The core API never throws — host
// engines (Unreal in particular) build with exceptions disabled, so errors
// travel by value instead. [[nodiscard]] makes the compiler diagnose an
// ignored Status (a hard error under -Werror builds like rpgkit's own).
class [[nodiscard]] Status {
 public:
  static Status ok() { return {true, std::string()}; }
  static Status error(std::string message) { return {false, std::move(message)}; }

  [[nodiscard]] bool isOk() const { return ok_; }
  [[nodiscard]] const std::string& message() const { return message_; }

 private:
  Status(bool ok, std::string message) : ok_(ok), message_(std::move(message)) {}

  bool ok_;
  std::string message_;
};

}  // namespace rpg::core

#endif  // RPG_CORE_STATUS_HPP_
