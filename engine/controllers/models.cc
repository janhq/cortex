#include "database/models.h"
#include <drogon/HttpTypes.h>
#include <filesystem>
#include <optional>
#include "config/gguf_parser.h"
#include "config/yaml_config.h"
#include "models.h"
#include "trantor/utils/Logger.h"
#include "utils/cortex_utils.h"
#include "utils/engine_constants.h"
#include "utils/file_manager_utils.h"
#include "utils/http_util.h"
#include "utils/logging_utils.h"
#include "utils/string_utils.h"

namespace {
std::string ToJsonStringWithPrecision(Json::Value& input, int precision = 2) {
  Json::StreamWriterBuilder wbuilder;
  wbuilder.settings_["precision"] = 2;
  return Json::writeString(wbuilder, input);
}
}  // namespace

void Models::PullModel(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback) {
  if (!http_util::HasFieldInReq(req, callback, "model")) {
    return;
  }

  auto model_handle = (*(req->getJsonObject())).get("model", "").asString();
  if (model_handle.empty()) {
    Json::Value ret;
    ret["result"] = "Bad Request";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  std::optional<std::string> desired_model_id = std::nullopt;
  auto id = (*(req->getJsonObject())).get("id", "").asString();
  if (!id.empty()) {
    desired_model_id = id;
  }

  std::optional<std::string> desired_model_name = std::nullopt;
  auto name_value = (*(req->getJsonObject())).get("name", "").asString();

  if (!name_value.empty()) {
    desired_model_name = name_value;
  }

  auto handle_model_input =
      [&, model_handle]() -> cpp::result<DownloadTask, std::string> {
    CTL_INF("Handle model input, model handle: " + model_handle);
    if (string_utils::StartsWith(model_handle, "https")) {
      return model_service_->HandleDownloadUrlAsync(
          model_handle, desired_model_id, desired_model_name);
    } else if (model_handle.find(":") != std::string::npos) {
      auto model_and_branch = string_utils::SplitBy(model_handle, ":");
      return model_service_->DownloadModelFromCortexsoAsync(
          model_and_branch[0], model_and_branch[1], desired_model_id);
    }

    return cpp::fail("Invalid model handle or not supported!");
  };

  auto result = handle_model_input();
  if (result.has_error()) {
    Json::Value ret;
    ret["message"] = result.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  } else {
    Json::Value ret;
    ret["message"] = "Model start downloading!";
    ret["task"] = result.value().ToJsonCpp();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}

void Models::GetModelPullInfo(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) const {
  if (!http_util::HasFieldInReq(req, callback, "model")) {
    return;
  }

  auto model_handle = (*(req->getJsonObject())).get("model", "").asString();
  auto res = model_service_->GetModelPullInfo(model_handle);
  if (res.has_error()) {
    Json::Value ret;
    ret["message"] = res.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  } else {
    auto const& info = res.value();
    Json::Value ret;
    Json::Value downloaded(Json::arrayValue);
    for (auto const& s : info.downloaded_models) {
      downloaded.append(s);
    }
    Json::Value avails(Json::arrayValue);
    for (auto const& s : info.available_models) {
      avails.append(s);
    }
    ret["id"] = info.id;
    ret["modelSource"] = info.model_source;
    ret["defaultBranch"] = info.default_branch;
    ret["message"] = "Get model pull information successfully";
    ret["downloadedModels"] = downloaded;
    ret["availableModels"] = avails;
    ret["downloadUrl"] = info.download_url;
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}

void Models::AbortPullModel(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {
  if (!http_util::HasFieldInReq(req, callback, "taskId")) {
    return;
  }
  auto task_id = (*(req->getJsonObject())).get("taskId", "").asString();
  if (task_id.empty()) {
    Json::Value ret;
    ret["result"] = "Bad Request";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  auto result = model_service_->AbortDownloadModel(task_id);
  if (result.has_error()) {
    Json::Value ret;
    ret["message"] = result.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  } else {
    Json::Value ret;
    ret["message"] = "Download stopped successfully";
    ret["taskId"] = result.value();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}

void Models::ListModel(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) const {
  namespace fs = std::filesystem;
  namespace fmu = file_manager_utils;
  Json::Value ret;
  ret["object"] = "list";
  Json::Value data(Json::arrayValue);
  model_service_->ForceIndexingModelList();
  // Iterate through directory

  config::YamlHandler yaml_handler;

  auto list_entry = db_service_->LoadModelList();
  if (list_entry) {
    for (const auto& model_entry : list_entry.value()) {
      try {
        if (model_entry.status == cortex::db::ModelStatus::Downloadable) {
          Json::Value obj;
          obj["id"] = model_entry.model;
          obj["model"] = model_entry.model;
          auto status_to_string = [](cortex::db::ModelStatus status) {
            switch (status) {
              case cortex::db::ModelStatus::Remote:
                return "remote";
              case cortex::db::ModelStatus::Downloaded:
                return "downloaded";
              case cortex::db::ModelStatus::Downloadable:
                return "downloadable";
            }
            return "unknown";
          };
          obj["modelSource"] = model_entry.model_source;
          obj["status"] = status_to_string(model_entry.status);
          obj["engine"] = model_entry.engine;
          obj["metadata"] = model_entry.metadata;
          data.append(std::move(obj));
          continue;
        }
        yaml_handler.ModelConfigFromFile(
            fmu::ToAbsoluteCortexDataPath(
                fs::path(model_entry.path_to_model_yaml))
                .string());
        auto model_config = yaml_handler.GetModelConfig();

        if (!engine_service_->IsRemoteEngine(model_config.engine)) {
          Json::Value obj = model_config.ToJson();
          obj["id"] = model_entry.model;
          obj["model"] = model_entry.model;
          obj["status"] = "downloaded";
          auto es = model_service_->GetEstimation(model_entry.model);
          if (es.has_value() && !!es.value()) {
            obj["recommendation"] = hardware::ToJson(*(es.value()));
          }
          data.append(std::move(obj));
          yaml_handler.Reset();
        } else if (model_config.engine == kPythonEngine) {
          config::PythonModelConfig python_model_config;
          python_model_config.ReadFromYaml(
              fmu::ToAbsoluteCortexDataPath(
                  fs::path(model_entry.path_to_model_yaml))
                  .string());
          Json::Value obj = python_model_config.ToJson();
          obj["id"] = model_entry.model;
          obj["model"] = model_entry.model;
          data.append(std::move(obj));
        } else {
          config::RemoteModelConfig remote_model_config;
          remote_model_config.LoadFromYamlFile(
              fmu::ToAbsoluteCortexDataPath(
                  fs::path(model_entry.path_to_model_yaml))
                  .string());
          Json::Value obj = remote_model_config.ToJson();
          obj["id"] = model_entry.model;
          obj["model"] = model_entry.model;
          data.append(std::move(obj));
        }
      } catch (const std::exception& e) {
        LOG_ERROR << "Failed to load yaml file for model: "
                  << model_entry.path_to_model_yaml << ", error: " << e.what();
      }
    }

    ret["data"] = data;
    ret["result"] = "OK";
    auto ret_str = ToJsonStringWithPrecision(ret);
    auto resp = cortex_utils::CreateCortexHttpTextAsJsonResponse(ret_str);
    resp->setStatusCode(k200OK);
    callback(resp);
  } else {
    std::string message = "Fail to get list model information: " +
                          std::string(list_entry.error());
    LOG_ERROR << message;
    ret["data"] = data;
    ret["result"] = "Fail to get list model information";
    ret["message"] = message;
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  }
}

void Models::GetModel(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback,
                      const std::string& model_id) const {
  namespace fs = std::filesystem;
  namespace fmu = file_manager_utils;
  LOG_DEBUG << "GetModel, Model handle: " << model_id;
  Json::Value ret;

  try {
    config::YamlHandler yaml_handler;
    auto model_entry = db_service_->GetModelInfo(model_id);
    if (model_entry.has_error()) {
      ret["id"] = model_id;
      ret["object"] = "model";
      ret["result"] = "Fail to get model information";
      ret["message"] = "Error: " + model_entry.error();
      auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
      return;
    }

    yaml_handler.ModelConfigFromFile(
        fmu::ToAbsoluteCortexDataPath(
            fs::path(model_entry.value().path_to_model_yaml))
            .string());
    auto model_config = yaml_handler.GetModelConfig();
    if (model_config.engine == kOnnxEngine ||
        model_config.engine == kLlamaEngine ||
        model_config.engine == kTrtLlmEngine) {
      auto ret = model_config.ToJsonString();
      auto resp = cortex_utils::CreateCortexHttpTextAsJsonResponse(ret);
      resp->setStatusCode(drogon::k200OK);
      callback(resp);
    } else if (model_config.engine == kPythonEngine) {
      config::PythonModelConfig python_model_config;
      python_model_config.ReadFromYaml(
          fmu::ToAbsoluteCortexDataPath(
              fs::path(model_entry.value().path_to_model_yaml))
              .string());
      ret = python_model_config.ToJson();
      ret["id"] = python_model_config.model;
      ret["object"] = "model";
      ret["result"] = "OK";
      auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
      resp->setStatusCode(k200OK);
      callback(resp);
    } else {
      config::RemoteModelConfig remote_model_config;
      remote_model_config.LoadFromYamlFile(
          fmu::ToAbsoluteCortexDataPath(
              fs::path(model_entry.value().path_to_model_yaml))
              .string());
      ret = remote_model_config.ToJson();
      ret["id"] = remote_model_config.model;
      ret["object"] = "model";
      ret["result"] = "OK";
      auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
      resp->setStatusCode(k200OK);
      callback(resp);
    }

  } catch (const std::exception& e) {
    std::string message =
        "Fail to get model information with ID '" + model_id + "': " + e.what();
    LOG_ERROR << message;
    ret["id"] = model_id;
    ret["object"] = "model";
    ret["result"] = "Fail to get model information";
    ret["message"] = message;
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  }
}

void Models::DeleteModel(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback,
                         const std::string& model_id) {
  auto result = model_service_->DeleteModel(model_id);
  if (result.has_error()) {
    Json::Value ret;
    ret["message"] = result.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
  } else {
    Json::Value ret;
    ret["message"] = "Deleted successfully!";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}

void Models::UpdateModel(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback,
                         const std::string& model_id) const {
  namespace fs = std::filesystem;
  namespace fmu = file_manager_utils;
  auto json_body = *(req->getJsonObject());
  try {
    auto model_entry = db_service_->GetModelInfo(model_id);
    config::YamlHandler yaml_handler;
    auto yaml_fp = fmu::ToAbsoluteCortexDataPath(
        fs::path(model_entry.value().path_to_model_yaml));
    yaml_handler.ModelConfigFromFile(yaml_fp.string());
    config::ModelConfig model_config = yaml_handler.GetModelConfig();
    std::string message;
    if (model_config.engine == kOnnxEngine ||
        model_config.engine == kLlamaEngine ||
        model_config.engine == kTrtLlmEngine) {
      model_config.FromJson(json_body);
      yaml_handler.UpdateModelConfig(model_config);
      yaml_handler.WriteYamlFile(yaml_fp.string());
      message = "Successfully update model ID '" + model_id +
                "': " + json_body.toStyledString();
    } else if (model_config.engine == kPythonEngine) {
      config::PythonModelConfig python_model_config;
      python_model_config.ReadFromYaml(yaml_fp.string());
      python_model_config.FromJson(json_body);
      python_model_config.ToYaml(yaml_fp.string());
      message = "Successfully update model ID '" + model_id +
                "': " + json_body.toStyledString();
    } else {
      config::RemoteModelConfig remote_model_config;
      remote_model_config.LoadFromYamlFile(yaml_fp.string());
      remote_model_config.LoadFromJson(json_body);
      remote_model_config.SaveToYamlFile(yaml_fp.string());
      message = "Successfully update model ID '" + model_id +
                "': " + json_body.toStyledString();
    }
    LOG_INFO << message;
    Json::Value ret;
    ret["result"] = "Updated successfully!";
    ret["modelHandle"] = model_id;
    ret["message"] = message;

    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k200OK);
    callback(resp);

  } catch (const std::exception& e) {
    std::string error_message =
        "Error updating with model_id '" + model_id + "': " + e.what();
    LOG_ERROR << error_message;
    Json::Value ret;
    ret["result"] = "Updated failed!";
    ret["modelHandle"] = model_id;
    ret["message"] = error_message;

    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  }
}

void Models::ImportModel(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) const {
  namespace fs = std::filesystem;
  namespace fmu = file_manager_utils;
  if (!http_util::HasFieldInReq(req, callback, "model") ||
      !http_util::HasFieldInReq(req, callback, "modelPath")) {
    return;
  }
  auto modelHandle = (*(req->getJsonObject())).get("model", "").asString();
  auto modelPath = (*(req->getJsonObject())).get("modelPath", "").asString();
  auto modelName = (*(req->getJsonObject())).get("name", "").asString();
  auto option = (*(req->getJsonObject())).get("option", "symlink").asString();
  config::GGUFHandler gguf_handler;
  config::YamlHandler yaml_handler;
  std::string model_yaml_path = (file_manager_utils::GetModelsContainerPath() /
                                 std::filesystem::path("imported") /
                                 std::filesystem::path(modelHandle + ".yml"))
                                    .string();

  try {
    // Use relative path for model_yaml_path. In case of import, we use absolute path for model
    auto yaml_rel_path =
        fmu::ToRelativeCortexDataPath(fs::path(model_yaml_path));
    cortex::db::ModelEntry model_entry{
        modelHandle, "",      "",         yaml_rel_path.string(),
        modelHandle, "local", "imported", cortex::db::ModelStatus::Downloaded,
        ""};

    std::filesystem::create_directories(
        std::filesystem::path(model_yaml_path).parent_path());
    gguf_handler.Parse(modelPath);
    config::ModelConfig model_config = gguf_handler.GetModelConfig();
    // There are 2 options: symlink and copy
    if (option == "copy") {
      // Copy GGUF file to the destination path
      std::filesystem::path file_path =
          std::filesystem::path(model_yaml_path).parent_path() /
          std::filesystem::path(modelPath).filename();
      std::filesystem::copy_file(
          modelPath, file_path, std::filesystem::copy_options::update_existing);
      model_config.files.push_back(file_path.string());
      auto size = std::filesystem::file_size(file_path);
      model_config.size = size;
    } else {
      model_config.files.push_back(modelPath);
      auto size = std::filesystem::file_size(modelPath);
      model_config.size = size;

      // set this so that it doesn't nuke the original file on model deletion
      model_entry.branch_name = "imported";
    }
    model_config.model = modelHandle;
    model_config.name = modelName.empty() ? model_config.name : modelName;
    yaml_handler.UpdateModelConfig(model_config);

    if (db_service_->AddModelEntry(model_entry).value()) {
      yaml_handler.WriteYamlFile(model_yaml_path);
      std::string success_message = "Model is imported successfully!";
      LOG_INFO << success_message;
      Json::Value ret;
      ret["result"] = "OK";
      ret["modelHandle"] = modelHandle;
      ret["message"] = success_message;
      auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
      resp->setStatusCode(k200OK);
      callback(resp);

    } else {
      std::string error_message = "Fail to import model, model_id '" +
                                  modelHandle + "' already exists!";
      LOG_ERROR << error_message;
      Json::Value ret;
      ret["result"] = "Import failed!";
      ret["modelHandle"] = modelHandle;
      ret["message"] = error_message;

      auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
    }

  } catch (const std::exception& e) {
    std::string error_message = "Error importing model path '" + modelPath +
                                "' with model_id '" + modelHandle +
                                "': " + e.what();
    LOG_ERROR << error_message;
    Json::Value ret;
    ret["result"] = "Import failed!";
    ret["modelHandle"] = modelHandle;
    ret["message"] = error_message;

    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  }
}

void Models::StartModel(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {
  if (!http_util::HasFieldInReq(req, callback, "model"))
    return;
  auto model_handle = (*(req->getJsonObject())).get("model", "").asString();

  std::optional<std::string> mmproj;
  if (auto& o = (*(req->getJsonObject()))["mmproj"]; !o.isNull()) {
    mmproj = o.asString();
  }

  auto bypass_llama_model_path = false;
  // Support both llama_model_path and model_path for backward compatible
  // model_path has higher priority
  if (auto& o = (*(req->getJsonObject()))["llama_model_path"]; !o.isNull()) {
    auto model_path = o.asString();
    if (auto& mp = (*(req->getJsonObject()))["model_path"]; mp.isNull()) {
      mp = model_path;
      // Bypass if model does not exist in DB and llama_model_path exists
      if (std::filesystem::exists(model_path) &&
          !model_service_->HasModel(model_handle)) {
        CTL_INF("llama_model_path exists, bypass check model id");
        bypass_llama_model_path = true;
      }
    }
  }

  auto bypass_model_check = (mmproj.has_value() || bypass_llama_model_path);

  auto model_entry = model_service_->GetDownloadedModel(model_handle);
  if (!model_entry.has_value() && !bypass_model_check) {
    Json::Value ret;
    ret["message"] = "Cannot find model: " + model_handle;
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
    return;
  }
  std::string engine_name =
      bypass_model_check ? kLlamaEngine : model_entry.value().engine;
  auto engine_validate = engine_service_->IsEngineReady(engine_name);
  if (engine_validate.has_error()) {
    Json::Value ret;
    ret["message"] = engine_validate.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
    return;
  }
  if (!engine_validate.value()) {
    Json::Value ret;
    ret["message"] = "Engine is not ready! Please install first!";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
    return;
  }

  auto result = model_service_->StartModel(
      model_handle, *(req->getJsonObject()) /*params_override*/,
      bypass_model_check);
  if (result.has_error()) {
    Json::Value ret;
    ret["message"] = result.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
  } else {
    auto& v = result.value();
    Json::Value ret;
    ret["message"] = "Started successfully!";
    if (v.warning) {
      ret["warning"] = *(v.warning);
    }
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}

void Models::StopModel(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback) {
  if (!http_util::HasFieldInReq(req, callback, "model")) {
    return;
  }

  auto model_handle = (*(req->getJsonObject())).get("model", "").asString();
  auto result = model_service_->StopModel(model_handle);
  if (result.has_error()) {
    Json::Value ret;
    ret["message"] = result.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
  } else {
    Json::Value ret;
    ret["message"] = "Stopped successfully!";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}

void Models::GetModelStatus(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& model_id) {
  auto result = model_service_->GetModelStatus(model_id);
  if (result.has_error()) {
    Json::Value ret;
    ret["message"] = result.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
  } else {
    Json::Value ret;
    ret["message"] = "Model is running";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}

void Models::GetRemoteModels(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& engine_id) {
  if (!engine_service_->IsRemoteEngine(engine_id)) {
    Json::Value ret;
    ret["message"] = "Not a remote engine: " + engine_id;
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
    return;
  }

  auto result = engine_service_->GetRemoteModels(engine_id);

  if (result.has_error()) {
    Json::Value ret;
    ret["message"] = result.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
  } else {
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(result.value());
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}

void Models::AddRemoteModel(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) const {
  namespace fs = std::filesystem;
  namespace fmu = file_manager_utils;
  if (!http_util::HasFieldInReq(req, callback, "model") ||
      !http_util::HasFieldInReq(req, callback, "engine")) {
    return;
  }

  auto model_handle = (*(req->getJsonObject())).get("model", "").asString();
  auto engine_name = (*(req->getJsonObject())).get("engine", "").asString();

  auto engine_validate = engine_service_->IsEngineReady(engine_name);
  if (engine_validate.has_error()) {
    Json::Value ret;
    ret["message"] = engine_validate.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
    return;
  }

  if (!engine_validate.value()) {
    Json::Value ret;
    ret["message"] = "Engine is not ready! Please install first!";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(drogon::k400BadRequest);
    callback(resp);
    return;
  }

  config::RemoteModelConfig model_config;
  model_config.LoadFromJson(*(req->getJsonObject()));
  std::string model_yaml_path = (file_manager_utils::GetModelsContainerPath() /
                                 std::filesystem::path("remote") /
                                 std::filesystem::path(model_handle + ".yml"))
                                    .string();
  try {
    // Use relative path for model_yaml_path. In case of import, we use absolute path for model
    auto yaml_rel_path =
        fmu::ToRelativeCortexDataPath(fs::path(model_yaml_path));
    cortex::db::ModelEntry model_entry{
        model_handle, "",       "",         yaml_rel_path.string(),
        model_handle, "remote", "imported", cortex::db::ModelStatus::Remote,
        engine_name};
    std::filesystem::create_directories(
        std::filesystem::path(model_yaml_path).parent_path());
    if (db_service_->AddModelEntry(model_entry).value()) {
      model_config.SaveToYamlFile(model_yaml_path);
      std::string success_message = "Model is imported successfully!";
      LOG_INFO << success_message;
      Json::Value ret;
      ret["result"] = "OK";
      ret["modelHandle"] = model_handle;
      ret["message"] = success_message;
      auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
      resp->setStatusCode(k200OK);
      callback(resp);

    } else {
      std::string error_message = "Fail to import model, model_id '" +
                                  model_handle + "' already exists!";
      LOG_ERROR << error_message;
      Json::Value ret;
      ret["result"] = "Import failed!";
      ret["modelHandle"] = model_handle;
      ret["message"] = error_message;

      auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
    }
  } catch (const std::exception& e) {
    std::string error_message =
        "Error while adding Remote model with model_id '" + model_handle +
        "': " + e.what();
    LOG_ERROR << error_message;
    Json::Value ret;
    ret["result"] = "Add failed!";
    ret["modelHandle"] = model_handle;
    ret["message"] = error_message;

    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  }
}

void Models::AddModelSource(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {
  if (!http_util::HasFieldInReq(req, callback, "source")) {
    return;
  }

  auto model_source = (*(req->getJsonObject())).get("source", "").asString();
  auto res = model_src_svc_->AddModelSource(model_source);
  if (res.has_error()) {
    Json::Value ret;
    ret["message"] = res.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  } else {
    auto const& info = res.value();
    Json::Value ret;
    ret["message"] = "Model source is added successfully!";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}

void Models::DeleteModelSource(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {
  if (!http_util::HasFieldInReq(req, callback, "source")) {
    return;
  }

  auto model_source = (*(req->getJsonObject())).get("source", "").asString();
  auto res = model_src_svc_->RemoveModelSource(model_source);
  if (res.has_error()) {
    Json::Value ret;
    ret["message"] = res.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  } else {
    auto const& info = res.value();
    Json::Value ret;
    ret["message"] = "Model source is deleted successfully!";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}

void Models::GetModelSources(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {
  auto res = model_src_svc_->GetModelSources();
  if (res.has_error()) {
    Json::Value ret;
    ret["message"] = res.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  } else {
    auto const& info = res.value();
    Json::Value ret;
    Json::Value data(Json::arrayValue);
    for (auto const& i : info) {
      data.append(i);
    }
    ret["data"] = data;
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}