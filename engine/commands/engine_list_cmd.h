#pragma once

#include <string>

namespace commands {
class EngineListCmd {
 public:
  bool Exec(const std::string& host, int port);
};

}  // namespace commands
