#pragma once

#include <mutex>
#include <string>
#include "utils/engine_constants.h"
#include "utils/logging_utils.h"

#include <vector>

#include "utils/engine_constants.h"

#include "utils/result.hpp"

namespace config_yaml_utils {

const std::string kDefaultHost{"127.0.0.1"};
const std::string kDefaultPort{"39281"};
const int kDefaultMaxLines{100000};
constexpr const uint64_t kDefaultCheckedForUpdateAt = 0u;
constexpr const uint64_t kDefaultCheckedForLlamacppUpdateAt = 0u;
constexpr const auto kDefaultLatestRelease = "default_version";
constexpr const auto kDefaultLatestLlamacppRelease = "";
constexpr const auto kDefaultCorsEnabled = true;
const std::vector<std::string> kDefaultEnabledOrigins{
    "http://localhost:39281", "http://127.0.0.1:39281", "http://0.0.0.0:39281"};
constexpr const auto kDefaultNoProxy = "example.com,::1,localhost,127.0.0.1";
const std::vector<std::string> kDefaultSupportedEngines{kLlamaEngine,
                                                        kPythonEngine};

struct CortexConfig {
  std::string logFolderPath;
  std::string logLlamaCppPath;
  std::string logTensorrtLLMPath;
  std::string logOnnxPath;
  std::string dataFolderPath;

  int maxLogLines;
  std::string apiServerHost;
  std::string apiServerPort;
  uint64_t checkedForUpdateAt;
  uint64_t checkedForLlamacppUpdateAt;
  std::string latestRelease;

  std::string latestLlamacppRelease;
  std::string huggingFaceToken;
  /**
   * Github's API requires a user-agent string.
   */
  std::string gitHubUserAgent;
  std::string gitHubToken;
  std::string llamacppVariant;
  std::string llamacppVersion;

  bool enableCors;
  std::vector<std::string> allowedOrigins;

  std::string proxyUrl;
  bool verifyProxySsl;
  bool verifyProxyHostSsl;
  std::string proxyUsername;
  std::string proxyPassword;
  std::string noProxy;

  bool verifyPeerSsl;
  bool verifyHostSsl;

  std::string sslCertPath;
  std::string sslKeyPath;
  std::vector<std::string> supportedEngines;
  uint64_t checkedForSyncHubAt;
};

class CortexConfigMgr {
 private:
  CortexConfigMgr() {}
  std::mutex mtx_;

 public:
  CortexConfigMgr(CortexConfigMgr const&) = delete;
  CortexConfigMgr& operator=(CortexConfigMgr const&) = delete;
  ~CortexConfigMgr() {}

  static CortexConfigMgr& GetInstance() {
    static CortexConfigMgr ccm;
    return ccm;
  }

  cpp::result<void, std::string> DumpYamlConfig(const CortexConfig& config,
                                                const std::string& path);

  CortexConfig FromYaml(const std::string& path,
                        const CortexConfig& default_cfg);
};
}  // namespace config_yaml_utils
