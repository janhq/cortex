#include "engine_list_cmd.h"
#include <json/reader.h>
#include <json/value.h>
#include "common/engine_servicei.h"
#include "server_start_cmd.h"
#include "utils/curl_utils.h"
#include "utils/engine_constants.h"
#include "utils/logging_utils.h"
#include "utils/url_parser.h"
// clang-format off
#include <tabulate/table.hpp>
#include <unordered_map>
// clang-format on

namespace commands {
bool EngineListCmd::Exec(const std::string& host, int port) {
  // Start server if server is not started yet
  if (!commands::IsServerAlive(host, port)) {
    CLI_LOG("Starting server ...");
    commands::ServerStartCmd ssc;
    if (!ssc.Exec(host, port)) {
      return false;
    }
  }

  tabulate::Table table;
  table.add_row({"#", "Name", "Version", "Variant", "Status", "Size"});

  auto url = url_parser::Url{
      .protocol = "http",
      .host = host + ":" + std::to_string(port),
      .pathParams = {"v1", "engines"},
  };
  auto result = curl_utils::SimpleGetJson(url.ToFullPath());
  if (result.has_error()) {
    CTL_ERR(result.error());
    return false;
  }

  std::unordered_map<std::string, std::vector<EngineVariantResponse>>
      engine_map;

  auto engines = engine_service_->GetSupportedEngineNames().value();
  for (const auto& engine : engines) {
    auto installed_variants = result.value()[engine];
    for (const auto& variant : installed_variants) {
      engine_map[engine].push_back(EngineVariantResponse{
          .name = variant["name"].asString(),
          .version = variant["version"].asString(),
          .engine = engine,
      });
    }
  }

  // TODO: namh support onnx and tensorrt
  auto default_engine_url = url_parser::Url{
      .protocol = "http",
      .host = host + ":" + std::to_string(port),
      .pathParams = {"v1", "engines", kLlamaEngine, "default"},
  };
  auto selected_variant_result =
      curl_utils::SimpleGetJson(default_engine_url.ToFullPath());

  std::optional<std::pair<std::string, std::string>> variant_pair =
      std::nullopt;
  if (selected_variant_result.has_value()) {
    variant_pair = std::make_pair<std::string, std::string>(
        selected_variant_result.value()["variant"].asString(),
        selected_variant_result.value()["version"].asString());
  }

  std::unordered_map<std::string, std::string> variant_size_mapping;
  if (variant_pair.has_value()) {
    auto version_releases_url = url_parser::Url{
        .protocol = "http",
        .host = host + ":" + std::to_string(port),
        .pathParams = {"v1", "engines", kLlamaEngine, "releases",
                       variant_pair->second},
    };

    auto version_releases_json =
        curl_utils::SimpleGetJson(version_releases_url.ToFullPath());

    for (auto release_data : version_releases_json.value()) {
      variant_size_mapping[release_data["name"].asString()] =
          release_data["size"].asString();
    }
  }

  std::vector<EngineVariantResponse> output;
  for (const auto& [key, value] : engine_map) {
    output.insert(output.end(), value.begin(), value.end());
  }

  int count = 0;
  for (auto const& v : output) {
    count += 1;
    if (variant_pair.has_value() && v.name == variant_pair->first &&
        v.version == variant_pair->second) {
      table.add_row({std::to_string(count), v.engine, v.version, v.name,
                     "Default", variant_size_mapping[v.name]});
      continue;
    }
    table.add_row({std::to_string(count), v.engine, v.version, v.name,
                   "Installed", variant_size_mapping[v.name]});
  }

  std::cout << table << std::endl;
  return true;
}
};  // namespace commands
