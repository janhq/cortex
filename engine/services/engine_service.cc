#include "engine_service.h"
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <vector>
#include "algorithm"
#include "database/engines.h"
#include "extensions/remote-engine/anthropic_engine.h"
#include "extensions/remote-engine/openai_engine.h"
#include "utils/archive_utils.h"
#include "utils/engine_constants.h"
#include "utils/engine_matcher_utils.h"
#include "utils/file_manager_utils.h"
#include "utils/github_release_utils.h"
#include "utils/logging_utils.h"
#include "utils/result.hpp"
#include "utils/semantic_version_utils.h"
#include "utils/system_info_utils.h"
#include "utils/url_parser.h"
namespace {
std::string GetSuitableCudaVersion(const std::string& engine,
                                   const std::string& cuda_driver_version) {
  auto suitable_toolkit_version = "";
  if (engine == kTrtLlmRepo || engine == kTrtLlmEngine) {
    // for tensorrt-llm, we need to download cuda toolkit v12.4
    suitable_toolkit_version = "12.4";
  } else {
    // llamacpp
    auto cuda_driver_semver =
        semantic_version_utils::SplitVersion(cuda_driver_version);
    if (cuda_driver_semver.major == 11) {
      suitable_toolkit_version = "11.7";
    } else if (cuda_driver_semver.major == 12) {
      suitable_toolkit_version = "12.0";
    }
  }
  return suitable_toolkit_version;
}

// Need to change this after we rename repositories
std::string NormalizeEngine(const std::string& engine) {
  if (engine == kLlamaEngine) {
    return kLlamaRepo;
  } else if (engine == kOnnxEngine) {
    return kOnnxRepo;
  } else if (engine == kTrtLlmEngine) {
    return kTrtLlmRepo;
  }
  return engine;
};

std::string Repo2Engine(const std::string& r) {
  if (r == kLlamaRepo) {
    return kLlamaEngine;
  } else if (r == kOnnxRepo) {
    return kOnnxEngine;
  } else if (r == kTrtLlmRepo) {
    return kTrtLlmEngine;
  }
  return r;
};

std::string GetEnginePath(std::string_view e) {
  if (e == kLlamaRepo) {
    return kLlamaLibPath;
  } else if (e == kOnnxRepo) {
    return kOnnxLibPath;
  } else if (e == kTrtLlmRepo) {
    return kTensorrtLlmPath;
  }
  return kLlamaLibPath;
};
}  // namespace

cpp::result<void, std::string> EngineService::InstallEngineAsync(
    const std::string& engine, const std::string& version,
    const std::optional<std::string> variant_name) {
  auto ne = NormalizeEngine(engine);
  CTL_INF("InstallEngineAsync: " << ne << ", " << version << ", "
                                 << variant_name.value_or(""));
  auto os = hw_inf_.sys_inf->os;
  if (os == kMacOs && (ne == kOnnxRepo || ne == kTrtLlmRepo)) {
    return cpp::fail("Engine " + ne + " is not supported on macOS");
  }

  if (os == kLinuxOs && ne == kOnnxRepo) {
    return cpp::fail("Engine " + ne + " is not supported on Linux");
  }

  auto result = DownloadEngine(ne, version, variant_name);
  if (result.has_error()) {
    return cpp::fail(result.error());
  }
  auto cuda_res = DownloadCuda(ne, true);
  if (cuda_res.has_error()) {
    return cpp::fail(cuda_res.error());
  }
  return {};
}

cpp::result<bool, std::string> EngineService::UnzipEngine(
    const std::string& engine, const std::string& version,
    const std::string& path) {
  bool found_cuda = false;

  CTL_INF("engine: " << engine);
  CTL_INF("CUDA version: " << hw_inf_.cuda_driver_version);
  std::string cuda_variant = "cuda-";
  auto cuda_github =
      GetSuitableCudaVersion(engine, hw_inf_.cuda_driver_version);
  // Github release cuda example: cuda-12-0-windows-amd64.tar.gz
  std::replace(cuda_github.begin(), cuda_github.end(), '.', '-');
  cuda_variant += cuda_github + "-" + hw_inf_.sys_inf->os + "-" +
                  hw_inf_.sys_inf->arch + ".tar.gz";
  CTL_INF("cuda_variant: " << cuda_variant);

  std::vector<std::string> variants;
  // Loop through all files in the directory
  // 1. Push all engine variants to a list
  // 2. If cuda version is matched, extract it
  if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
      CTL_INF("file path: " << entry.path().string());
      if (entry.is_regular_file() && (entry.path().extension() == ".tar.gz" ||
                                      entry.path().extension() == ".gz")) {
        CTL_INF("file name: " << entry.path().filename().string());
        variants.push_back(entry.path().filename().string());
        if (std::string cf = entry.path().filename().string();
            cf == cuda_variant) {
          CTL_INF("Found cuda variant, extract it");
          found_cuda = true;
          // extract binary
          auto cuda_path =
              file_manager_utils::GetCudaToolkitPath(NormalizeEngine(engine));
          archive_utils::ExtractArchive(path + "/" + cf, cuda_path.string(),
                                        true);
        }
      }
    }
  } else {
    // Folder does not exist
    return cpp::fail("Folder does not exist: " + path);
  }

  auto matched_variant = GetMatchedVariant(engine, variants);
  CTL_INF("Matched variant: " << matched_variant);
  if ((!found_cuda && system_info_utils::IsNvidiaSmiAvailable()) ||
      matched_variant.empty()) {
    return false;
  }

  if (matched_variant.empty()) {
    CTL_INF("No variant found for " << hw_inf_.sys_inf->os << "-"
                                    << hw_inf_.sys_inf->arch
                                    << ", will get engine from remote");
    // Go with the remote flow
  } else {
    auto [v, ar] = engine_matcher_utils::GetVersionAndArch(matched_variant);
    auto engine_path = file_manager_utils::GetEnginesContainerPath() /
                       NormalizeEngine(engine) / ar / v;
    CTL_INF("engine_path: " << engine_path.string());
    archive_utils::ExtractArchive(path + "/" + matched_variant,
                                  engine_path.string(), true);

    auto variant =
        engine_matcher_utils::GetVariantFromNameAndVersion(ar, engine, v);
    CTL_INF("Extracted variant: " + variant.value());
    // set as default
    auto res = SetDefaultEngineVariant(engine, v, variant.value());
    if (res.has_error()) {
      CTL_ERR("Failed to set default engine variant: " << res.error());
      return false;
    } else {
      CTL_INF("Set default engine variant: " << res.value().variant);
    }
  }

  return true;
}

cpp::result<bool, std::string> EngineService::UninstallEngineVariant(
    const std::string& engine, const std::optional<std::string> version,
    const std::optional<std::string> variant) {
  auto ne = NormalizeEngine(engine);
  // TODO: handle uninstall remote engine
  // only delete a remote engine if no model are using it
  auto exist_engine = GetEngineByNameAndVariant(engine);
  if (exist_engine.has_value() && exist_engine.value().type == "remote") {
    auto result = DeleteEngine(exist_engine.value().id);
    if (!result.empty()) {  // This mean no error when delete model
      CTL_ERR("Failed to delete engine: " << result);
      return cpp::fail(result);
    }
    return cpp::result<bool, std::string>(true);
  }

  if (IsEngineLoaded(ne)) {
    CTL_INF("Engine " << ne << " is already loaded, unloading it");
    auto unload_res = UnloadEngine(ne);
    if (unload_res.has_error()) {
      CTL_INF("Failed to unload engine: " << unload_res.error());
      return cpp::fail(unload_res.error());
    } else {
      CTL_INF("Engine " << ne << " unloaded successfully");
    }
  }

  std::optional<std::filesystem::path> path_to_remove = std::nullopt;
  if (version == std::nullopt && variant == std::nullopt) {
    // if no version and variant provided, remove all engines variant of that engine
    path_to_remove = file_manager_utils::GetEnginesContainerPath() / ne;
  } else if (version != std::nullopt && variant != std::nullopt) {
    // if both version and variant are provided, we only remove that variant
    path_to_remove = file_manager_utils::GetEnginesContainerPath() / ne /
                     variant.value() / version.value();
  } else if (version == std::nullopt) {
    // if only have variant, we remove all of that variant
    path_to_remove =
        file_manager_utils::GetEnginesContainerPath() / ne / variant.value();
  } else {
    return cpp::fail("No variant provided");
  }

  if (path_to_remove == std::nullopt) {
    return cpp::fail("Uninstall engine variant failed!");
  }
  if (!std::filesystem::exists(path_to_remove.value())) {
    return cpp::fail("Engine variant does not exist!");
  }

  try {
    std::filesystem::remove_all(path_to_remove.value());
    CTL_INF("Engine " << ne << " uninstalled successfully!");
    return true;
  } catch (const std::exception& e) {
    CTL_ERR("Failed to uninstall engine " << ne << ": " << e.what());
    return cpp::fail("Failed to uninstall engine " + ne + ": " + e.what());
  }
}

cpp::result<void, std::string> EngineService::DownloadEngine(
    const std::string& engine, const std::string& version,
    const std::optional<std::string> variant_name) {

  auto normalized_version = version == "latest"
                                ? "latest"
                                : string_utils::RemoveSubstring(version, "v");
  auto res = GetEngineVariants(engine, version);
  if (res.has_error()) {
    return cpp::fail("Failed to fetch engine releases: " + res.error());
  }
  if (res.value().empty()) {
    return cpp::fail("No release found for " + version);
  }

  std::optional<EngineVariant> selected_variant = std::nullopt;
  if (variant_name.has_value()) {
    auto latest_version_semantic = normalized_version == "latest"
                                       ? res.value()[0].version
                                       : normalized_version;
    auto merged_variant_name = engine + "-" + latest_version_semantic + "-" +
                               variant_name.value() + ".tar.gz";

    for (const auto& asset : res.value()) {
      if (asset.name == merged_variant_name) {
        selected_variant = asset;
        break;
      }
    }
  } else {
    std::vector<std::string> variants;
    for (const auto& asset : res.value()) {
      variants.push_back(asset.name);
    }

    auto matched_variant_name = GetMatchedVariant(engine, variants);
    for (const auto& v : res.value()) {
      if (v.name == matched_variant_name) {
        selected_variant = v;
        break;
      }
    }
  }

  if (!selected_variant) {
    return cpp::fail("Failed to find a suitable variant for " + engine);
  }

  if (IsEngineLoaded(engine)) {
    CTL_INF("Engine " << engine << " is already loaded, unloading it");
    auto unload_res = UnloadEngine(engine);
    if (unload_res.has_error()) {
      CTL_INF("Failed to unload engine: " << unload_res.error());
      return cpp::fail(unload_res.error());
    } else {
      CTL_INF("Engine " << engine << " unloaded successfully");
    }
  }

  auto normalize_version = "v" + selected_variant->version;
  auto variant_folder_name = engine_matcher_utils::GetVariantFromNameAndVersion(
      selected_variant->name, engine, selected_variant->version);
  auto variant_folder_path = file_manager_utils::GetEnginesContainerPath() /
                             engine / variant_folder_name.value() /
                             normalize_version;
  auto variant_path = variant_folder_path / selected_variant->name;

  std::filesystem::create_directories(variant_folder_path);

  CTL_INF("variant_folder_path: " + variant_folder_path.string());
  auto on_finished = [this, engine, selected_variant, variant_folder_path,
                      normalize_version](const DownloadTask& finishedTask) {
    // try to unzip the downloaded file
    CTL_INF("Engine zip path: " << finishedTask.items[0].localPath.string());
    CTL_INF("Version: " + normalize_version);

    auto extract_path = finishedTask.items[0].localPath.parent_path();
    archive_utils::ExtractArchive(finishedTask.items[0].localPath.string(),
                                  extract_path.string(), true);

    auto variant = engine_matcher_utils::GetVariantFromNameAndVersion(
        selected_variant->name, engine, normalize_version);

    CTL_INF("Extracted variant: " + variant.value());
    // set as default

    auto res =
        SetDefaultEngineVariant(engine, normalize_version, variant.value());
    if (res.has_error()) {
      CTL_ERR("Failed to set default engine variant: " << res.error());
    } else {
      CTL_INF("Set default engine variant: " << res.value().variant);
    }
    auto create_res =
        EngineService::UpsertEngine(engine,   // engine_name
                                    "local",  // todo - luke
                                    "",       // todo - luke
                                    "",       // todo - luke
                                    normalize_version, variant.value(),
                                    "Default",  // todo - luke
                                    ""          // todo - luke
        );

    if (create_res.has_value()) {
      CTL_ERR("Failed to create engine entry: " << create_res->engine_name);
    } else {
      CTL_INF("Engine entry created successfully");
    }

    for (const auto& entry : std::filesystem::directory_iterator(
             variant_folder_path.parent_path())) {
      if (entry.is_directory() &&
          entry.path().filename() != normalize_version) {
        try {
          std::filesystem::remove_all(entry.path());
        } catch (const std::exception& e) {
          CTL_WRN("Could not delete directory: " << e.what());
        }
      }
    }

    try {
      std::filesystem::remove(finishedTask.items[0].localPath);
    } catch (const std::exception& e) {
      CTL_WRN("Could not delete file: " << e.what());
    }
    CTL_INF("Finished!");
  };

  auto downloadTask =
      DownloadTask{.id = engine,
                   .type = DownloadType::Engine,
                   .items = {DownloadItem{
                       .id = engine,
                       .downloadUrl = selected_variant->browser_download_url,
                       .localPath = variant_path,
                   }}};

  auto add_task_result = download_service_->AddTask(downloadTask, on_finished);
  if (add_task_result.has_error()) {
    return cpp::fail(add_task_result.error());
  }
  return {};
}

cpp::result<bool, std::string> EngineService::DownloadCuda(
    const std::string& engine, bool async) {
  if (hw_inf_.sys_inf->os == "mac" || engine == kOnnxRepo ||
      engine == kOnnxEngine) {
    // mac and onnx engine does not require cuda toolkit
    return true;
  }

  if (hw_inf_.cuda_driver_version.empty()) {
    CTL_WRN("No cuda driver, continue with CPU");
    return true;
  }
  // download cuda toolkit
  const std::string jan_host = "catalog.jan.ai";
  const std::string cuda_toolkit_file_name = "cuda.tar.gz";
  const std::string download_id = "cuda";

  auto suitable_toolkit_version =
      GetSuitableCudaVersion(engine, hw_inf_.cuda_driver_version);

  // compare cuda driver version with cuda toolkit version
  // cuda driver version should be greater than toolkit version to ensure compatibility
  if (semantic_version_utils::CompareSemanticVersion(
          hw_inf_.cuda_driver_version, suitable_toolkit_version) < 0) {
    CTL_ERR("Your Cuda driver version "
            << hw_inf_.cuda_driver_version
            << " is not compatible with cuda toolkit version "
            << suitable_toolkit_version);
    return cpp::fail("Cuda driver is not compatible with cuda toolkit");
  }

  auto url_obj = url_parser::Url{
      .protocol = "https",
      .host = jan_host,
      .pathParams = {"dist", "cuda-dependencies", suitable_toolkit_version,
                     hw_inf_.sys_inf->os, cuda_toolkit_file_name},
  };

  auto cuda_toolkit_url = url_parser::FromUrl(url_obj);

  CTL_DBG("Cuda toolkit download url: " << cuda_toolkit_url);
  auto cuda_toolkit_local_path =
      file_manager_utils::GetContainerFolderPath(
          file_manager_utils::DownloadTypeToString(DownloadType::CudaToolkit)) /
      cuda_toolkit_file_name;
  CTL_DBG("Download to: " << cuda_toolkit_local_path.string());
  auto downloadCudaToolkitTask{DownloadTask{
      .id = download_id,
      .type = DownloadType::CudaToolkit,
      .items = {DownloadItem{.id = download_id,
                             .downloadUrl = cuda_toolkit_url,
                             .localPath = cuda_toolkit_local_path}},
  }};

  auto on_finished = [engine](const DownloadTask& finishedTask) {
    auto engine_path = file_manager_utils::GetCudaToolkitPath(engine);
    archive_utils::ExtractArchive(finishedTask.items[0].localPath.string(),
                                  engine_path.string());
    try {
      std::filesystem::remove(finishedTask.items[0].localPath);
    } catch (std::exception& e) {
      CTL_ERR("Error removing downloaded file: " << e.what());
    }
  };
  if (async) {
    auto res = download_service_->AddTask(downloadCudaToolkitTask, on_finished);
    if (res.has_error()) {
      return cpp::fail(res.error());
    }
    return true;
  } else {
    return download_service_->AddDownloadTask(downloadCudaToolkitTask,
                                              on_finished);
  }
}

std::string EngineService::GetMatchedVariant(
    const std::string& engine, const std::vector<std::string>& variants) {
  std::string matched_variant;
  if (engine == kTrtLlmRepo || engine == kTrtLlmEngine) {
    matched_variant = engine_matcher_utils::ValidateTensorrtLlm(
        variants, hw_inf_.sys_inf->os, hw_inf_.cuda_driver_version);
  } else if (engine == kOnnxRepo || engine == kOnnxEngine) {
    matched_variant = engine_matcher_utils::ValidateOnnx(
        variants, hw_inf_.sys_inf->os, hw_inf_.sys_inf->arch);
  } else if (engine == kLlamaRepo || engine == kLlamaEngine) {
    auto suitable_avx =
        engine_matcher_utils::GetSuitableAvxVariant(hw_inf_.cpu_inf);
    matched_variant = engine_matcher_utils::Validate(
        variants, hw_inf_.sys_inf->os, hw_inf_.sys_inf->arch, suitable_avx,
        hw_inf_.cuda_driver_version);
  }
  return matched_variant;
}

cpp::result<std::vector<EngineService::EngineRelease>, std::string>
EngineService::GetEngineReleases(const std::string& engine) const {
  auto ne = NormalizeEngine(engine);
  return github_release_utils::GetReleases("janhq", ne);
}

cpp::result<std::vector<EngineService::EngineVariant>, std::string>
EngineService::GetEngineVariants(const std::string& engine,
                                 const std::string& version) const {
  auto ne = NormalizeEngine(engine);
  auto engine_release =
      github_release_utils::GetReleaseByVersion("janhq", ne, version);

  if (engine_release.has_error()) {
    return cpp::fail("Failed to get engine release: " + engine_release.error());
  }

  std::vector<EngineVariant> compatible_variants;
  for (const auto& variant : engine_release.value().assets) {
    if (variant.content_type != "application/gzip") {
      continue;
    }
    if (variant.state != "uploaded") {
      continue;
    }
    compatible_variants.push_back(variant);
  }

  if (compatible_variants.empty()) {
    return cpp::fail("No compatible variants found for " + engine);
  }

  return compatible_variants;
}

cpp::result<DefaultEngineVariant, std::string>
EngineService::SetDefaultEngineVariant(const std::string& engine,
                                       const std::string& version,
                                       const std::string& variant) {
  auto ne = NormalizeEngine(engine);
  auto is_installed = IsEngineVariantReady(engine, version, variant);
  if (is_installed.has_error()) {
    return cpp::fail(is_installed.error());
  }

  if (!is_installed.value()) {
    return cpp::fail("Engine variant " + version + "-" + variant +
                     " is not installed yet!");
  }

  if (IsEngineLoaded(ne)) {
    CTL_INF("Engine " << ne << " is already loaded, unloading it");
    auto unload_res = UnloadEngine(ne);
    if (unload_res.has_error()) {
      CTL_INF("Failed to unload engine: " << unload_res.error());
      return cpp::fail(unload_res.error());
    } else {
      CTL_INF("Engine " << ne << " unloaded successfully");
    }
  }

  auto normalized_version = string_utils::RemoveSubstring(version, "v");

  auto config = file_manager_utils::GetCortexConfig();
  config.llamacppVersion = "v" + normalized_version;
  config.llamacppVariant = variant;
  auto result = file_manager_utils::UpdateCortexConfig(config);
  if (result.has_error()) {
    return cpp::fail(result.error());
  }

  return DefaultEngineVariant{
      .engine = engine,
      .version = normalized_version,
      .variant = variant,
  };
}

cpp::result<bool, std::string> EngineService::IsEngineVariantReady(
    const std::string& engine, const std::string& version,
    const std::string& variant) {
  auto ne = NormalizeEngine(engine);
  auto normalized_version = string_utils::RemoveSubstring(version, "v");
  auto installed_engines = GetInstalledEngineVariants(ne);
  if (installed_engines.has_error()) {
    return cpp::fail(installed_engines.error());
  }

  CLI_LOG("IsEngineVariantReady: " << ne << ", " << normalized_version << ", "
                                   << variant);
  for (const auto& installed_engine : installed_engines.value()) {
    CLI_LOG("Installed: name: " + installed_engine.name +
            ", version: " + installed_engine.version);
    if (installed_engine.name == variant &&
            installed_engine.version == normalized_version ||
        installed_engine.version == "v" + normalized_version) {
      return true;
    }
  }
  return false;
}

cpp::result<DefaultEngineVariant, std::string>
EngineService::GetDefaultEngineVariant(const std::string& engine) {
  auto ne = NormalizeEngine(engine);
  // current we don't support other engine
  if (ne != kLlamaRepo) {
    return cpp::fail("Engine " + engine + " is not supported yet!");
  }

  auto config = file_manager_utils::GetCortexConfig();
  auto variant = config.llamacppVariant;
  auto version = config.llamacppVersion;

  if (variant.empty() || version.empty()) {
    return cpp::fail("Default engine variant for " + engine +
                     " is not set yet!");
  }

  return DefaultEngineVariant{
      .engine = engine,
      .version = version,
      .variant = variant,
  };
}

cpp::result<std::vector<EngineVariantResponse>, std::string>
EngineService::GetInstalledEngineVariants(const std::string& engine) const {
  auto ne = NormalizeEngine(engine);
  auto os = hw_inf_.sys_inf->os;
  if (os == kMacOs && (ne == kOnnxRepo || ne == kTrtLlmRepo)) {
    return cpp::fail("Engine " + engine + " is not supported on macOS");
  }

  if (os == kLinuxOs && ne == kOnnxRepo) {
    return cpp::fail("Engine " + engine + " is not supported on Linux");
  }

  auto engines_variants_dir =
      file_manager_utils::GetEnginesContainerPath() / ne;

  if (!std::filesystem::exists(engines_variants_dir)) {
    return {};
  }

  std::vector<EngineVariantResponse> variants;
  for (const auto& entry :
       std::filesystem::directory_iterator(engines_variants_dir)) {
    if (entry.is_directory()) {
      // epectation is each directory is a variant
      for (const auto& version_entry :
           std::filesystem::directory_iterator(entry.path())) {
        // try to find version.txt
        auto version_txt_path = version_entry.path() / "version.txt";
        if (!std::filesystem::exists(version_txt_path)) {
          continue;
        }

        try {
          auto node = YAML::LoadFile(version_txt_path.string());
          auto ev = EngineVariantResponse{
              .name = node["name"].as<std::string>(),
              .version = "v" + node["version"].as<std::string>(),
              .engine = engine,
          };
          variants.push_back(ev);
        } catch (const YAML::Exception& e) {
          CTL_ERR("Error reading version.txt: " << e.what());
          continue;
        }
      }
    }
  }

  return variants;
}

bool EngineService::IsEngineLoaded(const std::string& engine) {
  auto ne = NormalizeEngine(engine);
  return engines_.find(ne) != engines_.end();
}

cpp::result<EngineV, std::string> EngineService::GetLoadedEngine(
    const std::string& engine_name) {
  std::lock_guard<std::mutex> lock(engines_mutex_);
  auto ne = NormalizeEngine(engine_name);
  if (engines_.find(ne) == engines_.end()) {
    return cpp::fail("Engine " + engine_name + " is not loaded yet!");
  }

  return engines_[ne].engine;
}

cpp::result<void, std::string> EngineService::LoadEngine(
    const std::string& engine_name) {
  auto ne = NormalizeEngine(engine_name);
  std::lock_guard<std::mutex> lock(engines_mutex_);
  if (IsEngineLoaded(ne)) {
    CTL_INF("Engine " << ne << " is already loaded");
    return {};
  }

  // Check for remote engine
  if (remote_engine::IsRemoteEngine(engine_name)) {
    auto exist_engine = GetEngineByNameAndVariant(engine_name);
    if (exist_engine.has_error()) {
      return cpp::fail("Remote engine '" + engine_name + "' is not installed");
    }

    if (engine_name == kOpenAiEngine) {
      engines_[engine_name].engine = new remote_engine::OpenAiEngine();
    } else {
      engines_[engine_name].engine = new remote_engine::AnthropicEngine();
    }

    CTL_INF("Loaded engine: " << engine_name);
    return {};
  }

  // End hard code

  CTL_INF("Loading engine: " << ne);

  auto selected_engine_variant = GetDefaultEngineVariant(ne);

  if (selected_engine_variant.has_error()) {
    return cpp::fail(selected_engine_variant.error());
  }

  CTL_INF("Selected engine variant: "
          << json_helper::DumpJsonString(selected_engine_variant->ToJson()));
#if defined(_WIN32)
  auto user_defined_engine_path = _wgetenv(L"ENGINE_PATH");
#else
  auto user_defined_engine_path = getenv("ENGINE_PATH");
#endif

  CTL_DBG("user defined engine path: " << user_defined_engine_path);
  const std::filesystem::path engine_dir_path = [&] {
    if (user_defined_engine_path != nullptr) {
      return std::filesystem::path(user_defined_engine_path) /
             GetEnginePath(ne) / selected_engine_variant->variant /
             selected_engine_variant->version;
    } else {
      return file_manager_utils::GetEnginesContainerPath() / ne /
             selected_engine_variant->variant /
             selected_engine_variant->version;
    }
  }();

  CTL_DBG("Engine path: " << engine_dir_path.string());

  if (!std::filesystem::exists(engine_dir_path)) {
    CTL_ERR("Directory " + engine_dir_path.string() + " is not exist!");
    return cpp::fail("Directory " + engine_dir_path.string() +
                     " is not exist!");
  }

  CTL_INF("Engine path: " << engine_dir_path.string());

  try {
#if defined(_WIN32)
    // TODO(?) If we only allow to load an engine at a time, the logic is simpler.
    // We would like to support running multiple engines at the same time. Therefore,
    // the adding/removing dll directory logic is quite complicated:
    // 1. If llamacpp is loaded and new requested engine is tensorrt-llm:
    // Unload the llamacpp dll directory then load the tensorrt-llm
    // 2. If tensorrt-llm is loaded and new requested engine is llamacpp:
    // Do nothing, llamacpp can re-use tensorrt-llm dependencies (need to be tested careful)
    // 3. Add dll directory if met other conditions

    auto add_dll = [this](const std::string& e_type,
                          const std::filesystem::path& p) {
      if (auto cookie = AddDllDirectory(p.c_str()); cookie != 0) {
        CTL_DBG("Added dll directory: " << p.string());
        engines_[e_type].cookie = cookie;
      } else {
        CTL_WRN("Could not add dll directory: " << p.string());
      }

      auto cuda_path = file_manager_utils::GetCudaToolkitPath(e_type);
      if (auto cuda_cookie = AddDllDirectory(cuda_path.c_str());
          cuda_cookie != 0) {
        CTL_DBG("Added cuda dll directory: " << p.string());
        engines_[e_type].cuda_cookie = cuda_cookie;
      } else {
        CTL_WRN("Could not add cuda dll directory: " << p.string());
      }
    };

#if defined(_WIN32)
    if (bool should_use_dll_search_path = !(_wgetenv(L"ENGINE_PATH"));
#else
    if (bool should_use_dll_search_path = !(getenv("ENGINE_PATH"));
#endif
        should_use_dll_search_path) {
      if (IsEngineLoaded(kLlamaRepo) && ne == kTrtLlmRepo &&
          should_use_dll_search_path) {

        {

          // Remove llamacpp dll directory
          if (!RemoveDllDirectory(engines_[kLlamaRepo].cookie)) {
            CTL_WRN("Could not remove dll directory: " << kLlamaRepo);
          } else {
            CTL_DBG("Removed dll directory: " << kLlamaRepo);
          }
          if (!RemoveDllDirectory(engines_[kLlamaRepo].cuda_cookie)) {
            CTL_WRN("Could not remove cuda dll directory: " << kLlamaRepo);
          } else {
            CTL_DBG("Removed cuda dll directory: " << kLlamaRepo);
          }
        }

        add_dll(ne, engine_dir_path);
      } else if (IsEngineLoaded(kTrtLlmRepo) && ne == kLlamaRepo) {
        // Do nothing
      } else {
        add_dll(ne, engine_dir_path);
      }
    }
#endif
    engines_[ne].dl =
        std::make_unique<cortex_cpp::dylib>(engine_dir_path.string(), "engine");
#if defined(__linux__)
    const char* name = "LD_LIBRARY_PATH";
    auto data = getenv(name);
    std::string v;
    if (auto g = getenv(name); g) {
      v += g;
    }
    CTL_INF("LD_LIBRARY_PATH: " << v);
    auto llamacpp_path = file_manager_utils::GetCudaToolkitPath(kLlamaRepo);
    CTL_INF("llamacpp_path: " << llamacpp_path);
    // tensorrt is not supported for now
    // auto trt_path = file_manager_utils::GetCudaToolkitPath(kTrtLlmRepo);

    auto new_v = llamacpp_path.string() + ":" + v;
    setenv(name, new_v.c_str(), true);
    CTL_INF("LD_LIBRARY_PATH: " << getenv(name));
#endif

  } catch (const cortex_cpp::dylib::load_error& e) {
    CTL_ERR("Could not load engine: " << e.what());
    engines_.erase(ne);
    return cpp::fail("Could not load engine " + ne + ": " + e.what());
  }

  auto func = engines_[ne].dl->get_function<EngineI*()>("get_engine");
  engines_[ne].engine = func();

  auto& en = std::get<EngineI*>(engines_[ne].engine);
  if (ne == kLlamaRepo) {  //fix for llamacpp engine first
    auto config = file_manager_utils::GetCortexConfig();
    if (en->IsSupported("SetFileLogger")) {
      en->SetFileLogger(config.maxLogLines,
                        (std::filesystem::path(config.logFolderPath) /
                         std::filesystem::path(config.logLlamaCppPath))
                            .string());
    } else {
      CTL_WRN("Method SetFileLogger is not supported yet");
    }
    if (en->IsSupported("SetLogLevel")) {
      en->SetLogLevel(logging_utils_helper::global_log_level);
    } else {
      CTL_WRN("Method SetLogLevel is not supported yet");
    }
  }
  CTL_DBG("loaded engine: " << ne);
  return {};
}

cpp::result<void, std::string> EngineService::UnloadEngine(
    const std::string& engine) {
  auto ne = NormalizeEngine(engine);
  std::lock_guard<std::mutex> lock(engines_mutex_);
  {
    if (!IsEngineLoaded(ne)) {
      return cpp::fail("Engine " + ne + " is not loaded yet!");
    }
    if (std::holds_alternative<EngineI*>(engines_[ne].engine)) {
      delete std::get<EngineI*>(engines_[ne].engine);
    } else {
      delete std::get<RemoteEngineI*>(engines_[ne].engine);
    }

#if defined(_WIN32)
    if (!RemoveDllDirectory(engines_[ne].cookie)) {
      CTL_WRN("Could not remove dll directory: " << ne);
    } else {
      CTL_DBG("Removed dll directory: " << ne);
    }
    if (!RemoveDllDirectory(engines_[ne].cuda_cookie)) {
      CTL_WRN("Could not remove cuda dll directory: " << ne);
    } else {
      CTL_DBG("Removed cuda dll directory: " << ne);
    }
#endif
    engines_.erase(ne);
  }
  CTL_DBG("Unloaded engine " + ne);
  return {};
}

std::vector<EngineV> EngineService::GetLoadedEngines() {
  std::lock_guard<std::mutex> lock(engines_mutex_);
  std::vector<EngineV> loaded_engines;
  for (const auto& [key, value] : engines_) {
    loaded_engines.push_back(value.engine);
  }
  return loaded_engines;
}

cpp::result<github_release_utils::GitHubRelease, std::string>
EngineService::GetLatestEngineVersion(const std::string& engine) const {
  auto ne = NormalizeEngine(engine);
  auto res = github_release_utils::GetReleaseByVersion("janhq", ne, "latest");
  if (res.has_error()) {
    return cpp::fail("Failed to fetch engine " + engine + " latest version!");
  }
  return res.value();
}

cpp::result<bool, std::string> EngineService::IsEngineReady(
    const std::string& engine) {
  auto ne = NormalizeEngine(engine);

  // Check for remote engine
  if (remote_engine::IsRemoteEngine(engine)) {
    auto exist_engine = GetEngineByNameAndVariant(engine);
    if (exist_engine.has_error()) {
      return cpp::fail("Remote engine '" + engine + "' is not installed");
    }
    return true;
  }

  // End hard code

  auto os = hw_inf_.sys_inf->os;
  if (os == kMacOs && (ne == kOnnxRepo || ne == kTrtLlmRepo)) {
    return cpp::fail("Engine " + engine + " is not supported on macOS");
  }

  if (os == kLinuxOs && ne == kOnnxRepo) {
    return cpp::fail("Engine " + engine + " is not supported on Linux");
  }
  auto installed_variants = GetInstalledEngineVariants(engine);
  if (installed_variants.has_error()) {
    return cpp::fail(installed_variants.error());
  }

  return installed_variants->size() > 0;
}

cpp::result<EngineUpdateResult, std::string> EngineService::UpdateEngine(
    const std::string& engine) {
  auto ne = NormalizeEngine(engine);
  auto default_variant = GetDefaultEngineVariant(ne);

  if (default_variant.has_error()) {
    // if we don't have a default variant, just stop
    CTL_INF("No default variant found for " << ne << ". Exit update engine");
    return cpp::fail(default_variant.error());
  }
  CTL_INF("Default variant: " << default_variant->variant
                              << ", version: " + default_variant->version);

  if (IsEngineLoaded(ne)) {
    CTL_INF("Engine " << ne << " is already loaded, unloading it");
    auto unload_res = UnloadEngine(ne);
    if (unload_res.has_error()) {
      CTL_INF("Failed to unload engine: " << unload_res.error());
      return cpp::fail(unload_res.error());
    } else {
      CTL_INF("Engine " << ne << " unloaded successfully");
    }
  }

  auto latest_version = GetLatestEngineVersion(ne);
  if (latest_version.has_error()) {
    // if can't get latest version, stop
    CTL_INF("Can't get latest version for "
            << ne << " error: " << latest_version.error());
    return cpp::fail("Failed to get latest version: " + latest_version.error());
  }
  CTL_INF("Latest version: " + latest_version.value().name);

  // check if local engines variants if latest version already exist
  auto installed_variants = GetInstalledEngineVariants(ne);

  bool latest_version_installed = false;
  for (const auto& v : installed_variants.value()) {
    CTL_INF("Installed version: " + v.version);
    CTL_INF(json_helper::DumpJsonString(v.ToJson()));
    if (default_variant->variant == v.name &&
        string_utils::RemoveSubstring(v.version, "v") ==
            latest_version.value().name) {
      latest_version_installed = true;
      break;
    }
  }

  if (latest_version_installed) {
    CTL_INF("Engine " + ne + ", " + default_variant->variant +
            " is already up-to-date! Version " +
            latest_version.value().tag_name);
    return cpp::fail("Engine " + ne + ", " + default_variant->variant +
                     " is already up-to-date! Version " +
                     latest_version.value().tag_name);
  }

  CTL_INF("Engine variant "
          << default_variant->variant << " is not up-to-date! Current: "
          << default_variant->version << ", latest: " << latest_version->name);

  auto res = InstallEngineAsync(engine, latest_version->tag_name,
                                default_variant->variant);

  return EngineUpdateResult{.engine = engine,
                            .variant = default_variant->variant,
                            .from = default_variant->version,
                            .to = latest_version->tag_name};
}

cpp::result<std::vector<cortex::db::EngineEntry>, std::string>
EngineService::GetEngines() {
  cortex::db::Engines engines;
  auto get_res = engines.GetEngines();

  if (!get_res.has_value()) {
    return cpp::fail("Failed to get engine entries");
  }

  return get_res.value();
}

cpp::result<cortex::db::EngineEntry, std::string> EngineService::GetEngineById(
    int id) {
  cortex::db::Engines engines;
  auto get_res = engines.GetEngineById(id);

  if (!get_res.has_value()) {
    return cpp::fail("Engine with ID " + std::to_string(id) + " not found");
  }

  return get_res.value();
}

cpp::result<cortex::db::EngineEntry, std::string>
EngineService::GetEngineByNameAndVariant(
    const std::string& engine_name, const std::optional<std::string> variant) {

  cortex::db::Engines engines;
  auto get_res = engines.GetEngineByNameAndVariant(engine_name, variant);

  if (!get_res.has_value()) {
    if (variant.has_value()) {
      return cpp::fail("Variant " + variant.value() + " not found for engine " +
                       engine_name);
    } else {
      return cpp::fail("Engine " + engine_name + " not found");
    }
  }

  return get_res.value();
}

cpp::result<cortex::db::EngineEntry, std::string> EngineService::UpsertEngine(
    const std::string& engine_name, const std::string& type,
    const std::string& api_key, const std::string& url,
    const std::string& version, const std::string& variant,
    const std::string& status, const std::string& metadata) {
  cortex::db::Engines engines;
  auto upsert_res = engines.UpsertEngine(engine_name, type, api_key, url,
                                         version, variant, status, metadata);
  if (upsert_res.has_value()) {
    return upsert_res.value();
  } else {
    return cpp::fail("Failed to upsert engine entry");
  }
}

std::string EngineService::DeleteEngine(int id) {
  cortex::db::Engines engines;
  auto delete_res = engines.DeleteEngineById(id);
  if (delete_res.has_value()) {
    return delete_res.value();
  } else {
    return "";
  }
}

cpp::result<Json::Value, std::string> EngineService::GetRemoteModels(
    const std::string& engine_name) {
  std::lock_guard<std::mutex> lock(engines_mutex_);
  if (auto r = IsEngineReady(engine_name); r.has_error()) {
    return cpp::fail(r.error());
  }

  if (!IsEngineLoaded(engine_name)) {
    auto exist_engine = GetEngineByNameAndVariant(engine_name);
    if (exist_engine.has_error()) {
      return cpp::fail("Remote engine '" + engine_name + "' is not installed");
    }
    if (engine_name == kOpenAiEngine) {
      engines_[engine_name].engine = new remote_engine::OpenAiEngine();
    } else {
      engines_[engine_name].engine = new remote_engine::AnthropicEngine();
    }

    CTL_INF("Loaded engine: " << engine_name);
  }
  auto& e = std::get<RemoteEngineI*>(engines_[engine_name].engine);
  auto res = e->GetRemoteModels();
  if (!res["error"].isNull()) {
    return cpp::fail(res["error"].asString());
  } else {
    return res;
  }
}