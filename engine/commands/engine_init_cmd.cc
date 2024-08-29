#include "engine_init_cmd.h"
#include <utility>
#include "services/download_service.h"
#include "trantor/utils/Logger.h"
// clang-format off
#include "utils/cortexso_parser.h" 
#include "utils/archive_utils.h"   
#include "utils/system_info_utils.h"
// clang-format on
#include "utils/cuda_toolkit_utils.h"
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
  CTLOG_INFO("OS: " << system_info.os << ", Arch: " << system_info.arch);

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
  CTLOG_INFO("Engine release path: " << gitHubHost << engineReleasePath.str());
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
        CTLOG_INFO("engineName_: " << engineName_);
        CTLOG_INFO("CUDA version: " << cuda_version);
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
        CTLOG_INFO("Matched variant: " << matched_variant);
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
            CTLOG_INFO("URL: " << full_url);

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

            DownloadService download_service;
            download_service.AddDownloadTask(downloadTask, [](const std::string&
                                                                  absolute_path,
                                                              bool unused) {
              // try to unzip the downloaded file
              std::filesystem::path downloadedEnginePath{absolute_path};
              CTLOG_INFO("Downloaded engine path: "
                       << downloadedEnginePath.string());

              archive_utils::ExtractArchive(
                  downloadedEnginePath.string(),
                  downloadedEnginePath.parent_path().parent_path().string());

              // remove the downloaded file
              // TODO(any) Could not delete file on Windows because it is currently hold by httplib(?)
              // Not sure about other platforms
              try {
                std::filesystem::remove(absolute_path);
              } catch (const std::exception& e) {
                CTLOG_WARN("Could not delete file: " << e.what());
              }
              CTLOG_INFO("Finished!");
            });
            if (system_info.os == "mac" || engineName_ == "cortex.onnx") {
              return false;
            }
            // download cuda toolkit
            const std::string jan_host = "https://catalog.jan.ai";
            const std::string cuda_toolkit_file_name = "cuda.tar.gz";
            const std::string download_id = "cuda";

            auto gpu_driver_version = system_info_utils::GetDriverVersion();       
            if(gpu_driver_version.empty()) return true;     

            auto cuda_runtime_version =
                cuda_toolkit_utils::GetCompatibleCudaToolkitVersion(
                    gpu_driver_version, system_info.os, engineName_);
            LOG_INFO << "abc";
            std::ostringstream cuda_toolkit_path;
            cuda_toolkit_path << "dist/cuda-dependencies/" << 11.7 << "/"
                              << system_info.os << "/"
                              << cuda_toolkit_file_name;

            LOG_DEBUG << "Cuda toolkit download url: " << jan_host
                      << cuda_toolkit_path.str();

            auto downloadCudaToolkitTask = DownloadTask{
                .id = download_id,
                .type = DownloadType::CudaToolkit,
                .error = std::nullopt,
                .items = {DownloadItem{
                    .id = download_id,
                    .host = jan_host,
                    .fileName = cuda_toolkit_file_name,
                    .type = DownloadType::CudaToolkit,
                    .path = cuda_toolkit_path.str(),
                }},
            };

            download_service.AddDownloadTask(
                downloadCudaToolkitTask,
                [](const std::string& absolute_path, bool unused) {
                  LOG_DEBUG << "Downloaded cuda path: " << absolute_path;
                  // try to unzip the downloaded file
                  std::filesystem::path downloaded_path{absolute_path};

                  archive_utils::ExtractArchive(
                      absolute_path,
                      downloaded_path.parent_path().parent_path().string());

                  try {
                    std::filesystem::remove(absolute_path);
                  } catch (std::exception& e) {
                    LOG_ERROR << "Error removing downloaded file: " << e.what();
                  }
                });

            return true;
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
