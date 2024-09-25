#pragma once
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "utils/logging_utils.h"
#include "yaml-cpp/yaml.h"

namespace config_yaml_utils {
struct CortexConfig {
  std::string logFolderPath;
  std::string dataFolderPath;
  int maxLogLines;
  std::string apiServerHost;
  std::string apiServerPort;
  uint64_t checkedForUpdateAt;
  std::string latestRelease;
};

const std::string kCortexFolderName = "cortexcpp";
const std::string kDefaultHost{"127.0.0.1"};
const std::string kDefaultPort{"3928"};
const int kDefaultMaxLines{100000};
constexpr const uint64_t kDefaultCheckedForUpdateAt = 0u;
constexpr const auto kDefaultLatestRelease = "default_version";

inline void DumpYamlConfig(const CortexConfig& config,
                           const std::string& path) {
  std::filesystem::path config_file_path{path};

  try {
    std::ofstream out_file(config_file_path);
    if (!out_file) {
      throw std::runtime_error("Failed to open output file.");
    }
    YAML::Node node;
    node["logFolderPath"] = config.logFolderPath;
    node["dataFolderPath"] = config.dataFolderPath;
    node["maxLogLines"] = config.maxLogLines;
    node["apiServerHost"] = config.apiServerHost;
    node["apiServerPort"] = config.apiServerPort;
    node["checkedForUpdateAt"] = config.checkedForUpdateAt;
    node["latestRelease"] = config.latestRelease;

    out_file << node;
    out_file.close();
  } catch (const std::exception& e) {
    CTL_ERR("Error writing to file: " << e.what());
    throw;
  }
}

inline CortexConfig FromYaml(const std::string& path,
                             const CortexConfig& default_cfg) {
  std::filesystem::path config_file_path{path};
  if (!std::filesystem::exists(config_file_path)) {
    throw std::runtime_error("File not found: " + path);
  }

  try {
    auto node = YAML::LoadFile(config_file_path.string());
    bool should_update_config =
        (!node["logFolderPath"] || !node["dataFolderPath"] ||
         !node["maxLogLines"] || !node["apiServerHost"] ||
         !node["apiServerPort"] || !node["checkedForUpdateAt"] ||
         !node["latestRelease"]);

    CortexConfig config = {
        .logFolderPath = node["logFolderPath"]
                             ? node["logFolderPath"].as<std::string>()
                             : default_cfg.logFolderPath,
        .dataFolderPath = node["dataFolderPath"]
                              ? node["dataFolderPath"].as<std::string>()
                              : default_cfg.dataFolderPath,
        .maxLogLines = node["maxLogLines"] ? node["maxLogLines"].as<int>()
                                           : default_cfg.maxLogLines,
        .apiServerHost = node["apiServerHost"]
                             ? node["apiServerHost"].as<std::string>()
                             : default_cfg.apiServerHost,
        .apiServerPort = node["apiServerPort"]
                             ? node["apiServerPort"].as<std::string>()
                             : default_cfg.apiServerPort,
        .checkedForUpdateAt = node["checkedForUpdateAt"]
                                  ? node["checkedForUpdateAt"].as<uint64_t>()
                                  : default_cfg.checkedForUpdateAt,
        .latestRelease = node["latestRelease"]
                             ? node["latestRelease"].as<std::string>()
                             : default_cfg.latestRelease,
    };
    if (should_update_config) {
      DumpYamlConfig(config, path);
    }
    return config;
  } catch (const YAML::BadFile& e) {
    CTL_ERR("Failed to read file: " << e.what());
    throw;
  }
}

}  // namespace config_yaml_utils
