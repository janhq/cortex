#pragma once

#include <string>
#include "utils/result.hpp"

namespace commands {

class EngineUnloadCmd {
 public:
  cpp::result<void, std::string> Exec(const std::string& host, int port,
                                      const std::string& engine);
};
}  // namespace commands
