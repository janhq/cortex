#include "engine_init_cmd.h"
#include <utility>
#include "services/download_service.h"
#include "trantor/utils/Logger.h"
// clang-format off
#include "utils/cortexso_parser.h" 
#include "utils/archive_utils.h"   
#include "utils/system_info_utils.h"
// clang-format on
#include "utils/engine_matcher_utils.h"

namespace commands {

EngineInitCmd::EngineInitCmd(std::string engineName, std::string version)
    : engineName_(std::move(engineName)), version_(std::move(version)) {}

bool EngineInitCmd::Exec() const {
  if (engineName_.empty()) {
    LOG_ERROR << "Engine name is required";
    return false;
  }

  // Check if the architecture and OS are supported
  auto system_info = system_info_utils::GetSystemInfo();
  if (system_info.arch == system_info_utils::kUnsupported ||
      system_info.os == system_info_utils::kUnsupported) {
    LOG_ERROR << "Unsupported OS or architecture: " << system_info.os << ", "
              << system_info.arch;
    return false;
  }
  LOG_INFO << "OS: " << system_info.os << ", Arch: " << system_info.arch;

  // check if engine is supported
  if (std::find(supportedEngines_.begin(), supportedEngines_.end(),
                engineName_) == supportedEngines_.end()) {
    LOG_ERROR << "Engine not supported";
    return false;
  }

  constexpr auto gitHubHost = "https://api.github.com";
  std::string version = version_.empty() ? "latest" : version_;
  std::ostringstream engineReleasePath;
  engineReleasePath << "/repos/janhq/" << engineName_ << "/releases/"
                    << version;
  LOG_INFO << "Engine release path: " << gitHubHost << engineReleasePath.str();
  using namespace nlohmann;

  httplib::Client cli(gitHubHost);
  if (auto res = cli.Get(engineReleasePath.str())) {
    if (res->status == httplib::StatusCode::OK_200) {
      try {
        auto jsonResponse = json::parse(res->body);
        auto assets = jsonResponse["assets"];
        auto os_arch{system_info.os + "-" + system_info.arch};

        std::vector<std::string> variants;
        for (auto& asset : assets) {
          auto asset_name = asset["name"].get<std::string>();
          variants.push_back(asset_name);
        }

        auto cuda_version = system_info_utils::GetCudaVersion();
        LOG_INFO << "engineName_: " << engineName_;
        LOG_INFO << "CUDA version: " << cuda_version;
        std::string matched_variant = "";
        if (engineName_ == "cortex.tensorrt-llm") {
          matched_variant = engine_matcher_utils::ValidateTensorrtLlm(
              variants, system_info.os, cuda_version);
        } else if (engineName_ == "cortex.onnx") {
          matched_variant = engine_matcher_utils::ValidateOnnx(
              variants, system_info.os, system_info.arch);
        } else if (engineName_ == "cortex.llamacpp") {
          auto suitable_avx = engine_matcher_utils::GetSuitableAvxVariant();
          matched_variant = engine_matcher_utils::Validate(
              variants, system_info.os, system_info.arch, suitable_avx,
              cuda_version);
        }
        LOG_INFO << "Matched variant: " << matched_variant;
        if (matched_variant.empty()) {
          LOG_ERROR << "No variant found for " << os_arch;
          return false;
        }

        for (auto& asset : assets) {
          auto assetName = asset["name"].get<std::string>();
          if (assetName == matched_variant) {
            std::string host{"https://github.com"};

            auto full_url = asset["browser_download_url"].get<std::string>();
            std::string path = full_url.substr(host.length());

            auto fileName = asset["name"].get<std::string>();
            LOG_INFO << "URL: " << full_url;

            auto downloadTask = DownloadTask{.id = engineName_,
                                             .type = DownloadType::Engine,
                                             .error = std::nullopt,
                                             .items = {DownloadItem{
                                                 .id = engineName_,
                                                 .host = host,
                                                 .fileName = fileName,
                                                 .type = DownloadType::Engine,
                                                 .path = path,
                                             }}};

            DownloadService().AddDownloadTask(downloadTask, [](const std::string&
                                                                   absolute_path,
                                                               bool unused) {
              // try to unzip the downloaded file
              std::filesystem::path downloadedEnginePath{absolute_path};
              LOG_INFO << "Downloaded engine path: "
                       << downloadedEnginePath.string();

              archive_utils::ExtractArchive(
                  downloadedEnginePath.string(),
                  downloadedEnginePath.parent_path().parent_path().string());

              // remove the downloaded file
              // TODO(any) Could not delete file on Windows because it is currently hold by httplib(?)
              // Not sure about other platforms
              try {
                std::filesystem::remove(absolute_path);
              } catch (const std::exception& e) {
                LOG_ERROR << "Could not delete file: " << e.what();
              }
              LOG_INFO << "Finished!";
            });

            return false;
          }
        }
      } catch (const json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return false;
      }
    } else {
      LOG_ERROR << "HTTP error: " << res->status;
      return false;
    }
  } else {
    auto err = res.error();
    LOG_ERROR << "HTTP error: " << httplib::to_string(err);
    return false;
  }
  return true;
}
};  // namespace commands
