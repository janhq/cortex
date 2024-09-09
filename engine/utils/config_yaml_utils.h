#pragma once
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "utils/logging_utils.h"
#include "yaml-cpp/yaml.h"

namespace config_yaml_utils {
struct CortexConfig {
  std::string dataFolderPath;
  std::string apiServerHost;
  std::string apiServerPort;
};

const std::string kCortexFolderName = "cortexcpp";
const std::string kDefaultHost{"127.0.0.1"};
const std::string kDefaultPort{"3928"};

inline void DumpYamlConfig(const CortexConfig& config,
                           const std::string& path) {
  std::filesystem::path config_file_path{path};

  try {
    std::ofstream out_file(config_file_path);
    if (!out_file) {
      throw std::runtime_error("Failed to open output file.");
    }
    YAML::Node node;
    node["dataFolderPath"] = config.dataFolderPath;
    node["apiServerHost"] = config.apiServerHost;
    node["apiServerPort"] = config.apiServerPort;

    out_file << node;
    out_file.close();
  } catch (const std::exception& e) {
    CTL_ERR("Error writing to file: " << e.what());
    throw;
  }
}

inline CortexConfig FromYaml(const std::string& path,
                             const std::string& variant) {
  std::filesystem::path config_file_path{path};
  if (!std::filesystem::exists(config_file_path)) {
    throw std::runtime_error("File not found: " + path);
  }

  try {
    auto node = YAML::LoadFile(config_file_path.string());
    CortexConfig config = {
        .dataFolderPath = node["dataFolderPath"].as<std::string>(),
        .apiServerHost = node["apiServerHost"].as<std::string>(),
        .apiServerPort = node["apiServerPort"].as<std::string>(),
    };
    return config;
  } catch (const YAML::BadFile& e) {
    CTL_ERR("Failed to read file: " << e.what());
    throw;
  }
}

}  // namespace config_yaml_utils
