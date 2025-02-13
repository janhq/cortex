#include "engines.h"
#include "services/engine_service.h"
#include "utils/archive_utils.h"
#include "utils/cortex_utils.h"
#include "utils/engine_constants.h"
#include "utils/http_util.h"
#include "utils/logging_utils.h"
#include "utils/scope_exit.h"
#include "utils/string_utils.h"

namespace {
// Need to change this after we rename repositories
std::string NormalizeEngine(const std::string& engine) {
  if (engine == kLlamaEngine) {
    return kLlamaRepo;
  }
  return engine;
};
}  // namespace

void Engines::ListEngine(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) const {
  Json::Value ret;
  auto engines = engine_service_->GetSupportedEngineNames().value();
  for (const auto& engine : engines) {
    auto installed_engines =
        engine_service_->GetInstalledEngineVariants(engine);
    if (installed_engines.has_error()) {
      continue;
    }
    Json::Value variants(Json::arrayValue);
    for (const auto& variant : installed_engines.value()) {
      variants.append(variant.ToJson());
    }
    ret[engine] = variants;
  }

  // Add remote engine
  auto remote_engines = engine_service_->GetEngines();
  if (remote_engines.has_value()) {
    for (auto engine : remote_engines.value()) {
      if (engine.type == "remote") {
        auto engine_json = engine.ToJson();
        Json::Value list_engine(Json::arrayValue);
        list_engine.append(engine_json);
        ret[engine.engine_name] = list_engine;
      }
    }
  }
  auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
  resp->setStatusCode(k200OK);
  callback(resp);
}

void Engines::UninstallEngine(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& engine) {
  std::optional<std::string> norm_variant = std::nullopt;
  std::optional<std::string> norm_version = std::nullopt;
  if (req->getJsonObject() != nullptr) {
    auto variant = (*(req->getJsonObject())).get("variant", "").asString();
    auto version =
        (*(req->getJsonObject())).get("version", "latest").asString();

    if (!variant.empty()) {
      norm_variant = variant;
    }
    if (!version.empty()) {
      norm_version = version;
    }
  }

  auto result = engine_service_->UninstallEngineVariant(engine, norm_version,
                                                        norm_variant);

  Json::Value ret;
  if (result.has_error()) {
    CTL_INF(result.error());
    ret["message"] = result.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  } else {
    CTL_INF("Engine uninstalled successfully");
    ret["message"] = "Engine " + engine + " uninstalled successfully!";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}

void Engines::GetEngineReleases(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& engine) const {
  if (engine.empty()) {
    Json::Value res;
    res["message"] = "Engine name is required";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  // TODO: namh support pagination
  auto result = engine_service_->GetEngineReleases(engine);
  if (result.has_error()) {
    Json::Value res;
    res["message"] = "Failed to get engine releases: " + result.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  Json::Value releases(Json::arrayValue);
  for (const auto& release : result.value()) {
    releases.append(release.ToApiJson());
  }
  auto resp = cortex_utils::CreateCortexHttpJsonResponse(releases);
  resp->setStatusCode(k200OK);
  callback(resp);
}

void Engines::GetEngineVariants(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& engine, const std::string& version,
    std::optional<std::string> show) const {
  if (engine.empty()) {
    Json::Value res;
    res["message"] = "Engine name is required";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
    resp->setStatusCode(k409Conflict);
    callback(resp);
    LOG_WARN << "No engine field in path param";
    return;
  }

  auto show_value = show.value_or("all");
  if (show_value != "all" && show_value != "compatible") {
    Json::Value res;
    res["message"] = "Invalid show value. Can either be `all` or `compatible`";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  auto result = engine_service_->GetEngineVariants(engine, version,
                                                   show_value == "compatible");

  auto normalize_version = string_utils::RemoveSubstring(version, "v");
  Json::Value releases(Json::arrayValue);
  for (const auto& release : result.value()) {
    auto json = release.ToApiJson(NormalizeEngine(engine), normalize_version);
    if (json != std::nullopt) {
      releases.append(json.value());
    }
  }
  auto resp = cortex_utils::CreateCortexHttpJsonResponse(releases);
  resp->setStatusCode(k200OK);
  callback(resp);
}

void Engines::InstallEngine(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& engine) {
  std::optional<std::string> norm_variant = std::nullopt;
  std::string norm_version{"latest"};

  if (req->getJsonObject() != nullptr) {
    auto variant = (*(req->getJsonObject())).get("variant", "").asString();
    auto version =
        (*(req->getJsonObject())).get("version", "latest").asString();

    if (!variant.empty()) {
      norm_variant = variant;
    }
    norm_version = version;
  }

  auto result =
      engine_service_->InstallEngineAsync(engine, norm_version, norm_variant);
  if (result.has_error()) {
    Json::Value res;
    res["message"] = result.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
    resp->setStatusCode(k400BadRequest);
    CTL_INF("Error: " << result.error());
    callback(resp);
  } else {
    Json::Value res;
    res["message"] = "Engine starts installing!";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
    resp->setStatusCode(k200OK);
    CTL_INF("Engine starts installing!");
    callback(resp);
  }
}

void Engines::InstallRemoteEngine(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {
  if (!http_util::HasFieldInReq(req, callback, "engine")) {
    return;
  }
  std::optional<std::string> norm_variant = std::nullopt;
  std::string norm_version{"latest"};

  if (req->getJsonObject() != nullptr) {
    auto variant = (*(req->getJsonObject())).get("variant", "").asString();
    auto version =
        (*(req->getJsonObject())).get("version", "latest").asString();

    if (!variant.empty()) {
      norm_variant = variant;
    }
    norm_version = version;
  }

  std::string engine;
  if (auto o = req->getJsonObject(); o) {
    engine = (*o).get("engine", "").asString();
    auto type = (*o).get("type", "").asString();
    auto api_key = (*o).get("api_key", "").asString();
    auto url = (*o).get("url", "").asString();
    auto variant = norm_variant.value_or("all-platforms");
    auto status = (*o).get("status", "Default").asString();
    std::string metadata;
    if ((*o).isMember("metadata") && (*o)["metadata"].isObject()) {
      metadata =
          (*o).get("metadata", Json::Value(Json::objectValue)).toStyledString();
    } else if ((*o).isMember("metadata") && !(*o)["metadata"].isObject()) {
      Json::Value res;
      res["message"] = "metadata must be object";
      auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
      return;
    }

    auto get_models_url = (*o).get("metadata", Json::Value(Json::objectValue))
                              .get("get_models_url", "")
                              .asString();

    if (engine.empty() || type.empty()) {
      Json::Value res;
      res["message"] = "Engine name, type are required";
      auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
      return;
    }
    auto exist_engine = engine_service_->GetEngineByNameAndVariant(engine);
    // only allow 1 variant 1 version of a remote engine name
    if (exist_engine.has_value()) {
      Json::Value res;
      if (get_models_url.empty()) {
        res["warning"] =
            "'get_models_url' not found in metadata, You'll not able to search "
            "remote models with this engine";
      }
      res["message"] = "Engine '" + engine + "' already exists";
      auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
      return;
    }

    auto result = engine_service_->UpsertEngine(
        engine, type, api_key, url, norm_version, variant, status, metadata);
    if (result.has_error()) {
      Json::Value res;
      if (get_models_url.empty()) {
        res["warning"] =
            "'get_models_url' not found in metadata, You'll not able to search "
            "remote models with this engine";
      }
      res["message"] = result.error();
      auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
    } else {
      Json::Value res;
      if (get_models_url.empty()) {
        res["warning"] =
            "'get_models_url' not found in metadata, You'll not able to search "
            "remote models with this engine";
      }
      res["message"] = "Remote Engine install successfully!";
      auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
      resp->setStatusCode(k200OK);
      callback(resp);
    }
  }
}

void Engines::GetInstalledEngineVariants(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& engine) const {

  if (engine_service_->IsRemoteEngine(engine)) {
    auto remote_engines = engine_service_->GetEngines();
    Json::Value releases(Json::arrayValue);
    if (remote_engines.has_value()) {
      for (auto e : remote_engines.value()) {
        if (e.type == kRemote && e.engine_name == engine) {
          releases.append(e.ToJson());
          break;
        }
      }
    }
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(releases);
    resp->setStatusCode(k200OK);
    callback(resp);
    return;
  }

  auto result = engine_service_->GetInstalledEngineVariants(engine);
  if (result.has_error()) {
    Json::Value res;
    res["message"] = result.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }
  Json::Value releases(Json::arrayValue);
  for (const auto& variant : result.value()) {
    releases.append(variant.ToJson());
  }
  auto resp = cortex_utils::CreateCortexHttpJsonResponse(releases);
  resp->setStatusCode(k200OK);
  callback(resp);
}

void Engines::UpdateEngine(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& engine) {

  if (engine_service_->IsRemoteEngine(engine)) {
    auto exist_engine = engine_service_->GetEngineByNameAndVariant(engine);
    // only allow 1 variant 1 version of a remote engine name
    if (!exist_engine) {
      Json::Value res;
      res["message"] = "Remote engine '" + engine + "' is not installed";
      auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
    } else {
      if (auto o = req->getJsonObject(); o) {
        auto type = (*o).get("type", (*exist_engine).type).asString();
        auto api_key = (*o).get("api_key", (*exist_engine).api_key).asString();
        auto url = (*o).get("url", (*exist_engine).url).asString();
        auto status = (*o).get("status", (*exist_engine).status).asString();
        auto version = (*o).get("version", "latest").asString();
        std::string metadata;
        if ((*o).isMember("metadata") && (*o)["metadata"].isObject()) {
          metadata = (*o).get("metadata", Json::Value(Json::objectValue))
                         .toStyledString();
        } else if ((*o).isMember("metadata") && !(*o)["metadata"].isObject()) {
          Json::Value res;
          res["message"] = "metadata must be object";
          auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
          resp->setStatusCode(k400BadRequest);
          callback(resp);
          return;
        } else {
          metadata = (*exist_engine).metadata;
        }

        auto upd_res =
            engine_service_->UpsertEngine(engine, type, api_key, url, version,
                                          "all-platforms", status, metadata);
        if (upd_res.has_error()) {
          Json::Value res;
          res["message"] = upd_res.error();
          auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
          resp->setStatusCode(k400BadRequest);
          callback(resp);
        } else {
          Json::Value res;
          res["message"] = "Remote Engine update successfully!";
          auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
          resp->setStatusCode(k200OK);
          callback(resp);
        }
      } else {
        Json::Value res;
        res["message"] = "Request body is empty!";
        auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
      }
    }
    return;
  }

  auto result = engine_service_->UpdateEngine(engine);
  if (result.has_error()) {
    Json::Value res;
    res["message"] = result.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  } else {
    auto resp =
        cortex_utils::CreateCortexHttpJsonResponse(result.value().ToJson());
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}

void Engines::GetLatestEngineVersion(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& engine) {
  auto result = engine_service_->GetLatestEngineVersion(engine);
  if (result.has_error()) {
    Json::Value res;
    res["message"] = result.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  } else {
    auto resp =
        cortex_utils::CreateCortexHttpJsonResponse(result.value().ToApiJson());
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}

void Engines::SetDefaultEngineVariant(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& engine) {
  auto json_obj = req->getJsonObject();
  if (json_obj == nullptr) {
    Json::Value res;
    res["message"] = "Request body is required";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  auto variant = (*(req->getJsonObject())).get("variant", "").asString();
  if (variant.empty()) {
    Json::Value ret;
    ret["message"] = "Variant is required";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }
  auto version = (*(req->getJsonObject())).get("version", "").asString();
  if (version.empty()) {
    Json::Value ret;
    ret["message"] = "Version is required";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(ret);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  auto result =
      engine_service_->SetDefaultEngineVariant(engine, version, variant);
  if (result.has_error()) {
    Json::Value res;
    res["message"] = result.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  } else {
    Json::Value res;
    res["message"] = "Engine " + result.value().variant + " " +
                     result.value().version + " set as default";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}

void Engines::GetDefaultEngineVariant(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& engine) const {
  auto result = engine_service_->GetDefaultEngineVariant(engine);
  if (result.has_error()) {
    Json::Value res;
    res["message"] = result.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  } else {
    auto resp =
        cortex_utils::CreateCortexHttpJsonResponse(result.value().ToJson());
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}

void Engines::LoadEngine(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback,
                         const std::string& engine) {
  auto result = engine_service_->LoadEngine(engine);
  if (result.has_error()) {
    Json::Value res;
    res["message"] = result.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  } else {
    Json::Value res;
    res["message"] = "Engine " + engine + " loaded successfully!";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}

void Engines::UnloadEngine(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    const std::string& engine) {
  auto result = engine_service_->UnloadEngine(engine);
  if (result.has_error()) {
    Json::Value res;
    res["message"] = result.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  } else {
    Json::Value res;
    res["message"] = "Engine " + engine + " unloaded successfully!";
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
    resp->setStatusCode(k200OK);
    callback(resp);
  }
}
