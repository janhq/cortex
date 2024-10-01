#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "utils/cpuid/cpu_info.h"
#include "utils/result.hpp"

struct EngineInfo {
  std::string name;
  std::string description;
  std::string format;
  std::optional<std::string> version;
  std::string product_name;
  std::string status;
  std::optional<std::string> variant;
};

namespace system_info_utils {
struct SystemInfo;
}
class EngineService {
 public:
  constexpr static auto kIncompatible = "Incompatible";
  constexpr static auto kReady = "Ready";
  constexpr static auto kNotInstalled = "Not Installed";

  const std::vector<std::string_view> kSupportEngines = {
      "cortex.llamacpp", "cortex.onnx", "cortex.tensorrt-llm"};

  EngineService();
  ~EngineService();

  cpp::result<EngineInfo, std::string> GetEngineInfo(
      const std::string& engine) const;

  std::vector<EngineInfo> GetEngineInfoList() const;

  cpp::result<bool, std::string> InstallEngine(
      const std::string& engine, const std::string& version = "latest",
      const std::string& src = "");

  cpp::result<bool, std::string> UninstallEngine(const std::string& engine);

 private:
  cpp::result<bool, std::string> UnzipEngine(const std::string& engine,
                                             const std::string& version,
                                             const std::string& path);

  cpp::result<bool, std::string> DownloadEngine(
      const std::string& engine, const std::string& version = "latest");

  cpp::result<bool, std::string> DownloadCuda(const std::string& engine);

  std::string GetMatchedVariant(const std::string& engine,
                                const std::vector<std::string>& variants);

 private:
  struct HardwareInfo {
    std::unique_ptr<system_info_utils::SystemInfo> sys_inf;
    cortex::cpuid::CpuInfo cpu_inf;
    std::string cuda_driver_version;
  };
  HardwareInfo hw_inf_;
};
