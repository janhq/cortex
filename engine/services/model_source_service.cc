#include "model_source_service.h"
#include <chrono>
#include <unordered_set>
#include "database/models.h"
#include "json/json.h"
#include "utils/curl_utils.h"
#include "utils/file_manager_utils.h"
#include "utils/huggingface_utils.h"
#include "utils/logging_utils.h"
#include "utils/string_utils.h"
#include "utils/url_parser.h"

namespace hu = huggingface_utils;

namespace {
struct ModelInfo {
  std::string id;
  int likes;
  int trending_score;
  bool is_private;
  int downloads;
  std::vector<std::string> tags;
  std::string created_at;
  std::string model_id;
};

std::vector<ModelInfo> ParseJsonString(const std::string& json_str) {
  std::vector<ModelInfo> models;

  // Parse the JSON string
  Json::Value root;
  Json::Reader reader;
  bool parsing_successful = reader.parse(json_str, root);

  if (!parsing_successful) {
    std::cerr << "Failed to parse JSON" << std::endl;
    return models;
  }

  // Iterate over the JSON array
  for (const auto& model : root) {
    ModelInfo info;
    info.id = model["id"].asString();
    info.likes = model["likes"].asInt();
    info.trending_score = model["trendingScore"].asInt();
    info.is_private = model["private"].asBool();
    info.downloads = model["downloads"].asInt();

    const Json::Value& tags = model["tags"];
    for (const auto& tag : tags) {
      info.tags.push_back(tag.asString());
    }

    info.created_at = model["createdAt"].asString();
    info.model_id = model["modelId"].asString();
    models.push_back(info);
  }

  return models;
}

}  // namespace

ModelSourceService::ModelSourceService(
    std::shared_ptr<DatabaseService> db_service)
    : db_service_(db_service) {
  sync_db_thread_ = std::thread(&ModelSourceService::SyncModelSource, this);
  running_ = true;
}

ModelSourceService::~ModelSourceService() {
  running_ = false;
  if (sync_db_thread_.joinable()) {
    sync_db_thread_.join();
  }
  CTL_INF("Done cleanup thread");
}

cpp::result<bool, std::string> ModelSourceService::AddModelSource(
    const std::string& model_source) {
  auto res = url_parser::FromUrlString(model_source);
  if (res.has_error()) {
    return cpp::fail(res.error());
  } else {
    auto& r = res.value();
    if (r.pathParams.empty() || r.pathParams.size() > 2) {
      return cpp::fail("Invalid model source url: " + model_source);
    }

    if (auto is_org = r.pathParams.size() == 1; is_org) {
      auto& author = r.pathParams[0];
      if (author == "cortexso") {
        return AddCortexsoOrg(model_source);
      } else {
        return AddHfOrg(model_source, author);
      }
    } else {  // Repo
      auto const& author = r.pathParams[0];
      auto const& model_name = r.pathParams[1];
      if (r.pathParams[0] == "cortexso") {
        return AddCortexsoRepo(model_source, author, model_name);
      } else {
        return AddHfRepo(model_source, author, model_name);
      }
    }
  }
  return true;
}

cpp::result<bool, std::string> ModelSourceService::RemoveModelSource(
    const std::string& model_source) {
  auto srcs = db_service_->GetModelSources();
  if (srcs.has_error()) {
    return cpp::fail(srcs.error());
  } else {
    auto& v = srcs.value();
    auto exists = [&v, &model_source]() {
      for (auto const& m : v) {
        if (m.model_source == model_source)
          return true;
      }
      return false;
    }();
    if (!exists) {
      return cpp::fail("Model source does not exist: " + model_source);
    }
  }
  CTL_INF("Remove model source: " << model_source);
  auto res = url_parser::FromUrlString(model_source);
  if (res.has_error()) {
    return cpp::fail(res.error());
  } else {
    auto& r = res.value();
    if (r.pathParams.empty() || r.pathParams.size() > 2) {
      return cpp::fail("Invalid model source url: " + model_source);
    }

    if (r.pathParams.size() == 1) {
      if (auto del_res = db_service_->DeleteModelEntryWithOrg(model_source);
          del_res.has_error()) {
        CTL_INF(del_res.error());
        return cpp::fail(del_res.error());
      }
    } else {
      if (auto del_res = db_service_->DeleteModelEntryWithRepo(model_source);
          del_res.has_error()) {
        CTL_INF(del_res.error());
        return cpp::fail(del_res.error());
      }
    }
  }
  return true;
}

cpp::result<std::unordered_map<std::string, ModelSource>, std::string>
ModelSourceService::GetModelSources() {
  auto res = db_service_->GetModelSources();
  if (res.has_error()) {
    return cpp::fail(res.error());
  }
  auto& models = res.value();
  std::unordered_map<std::string, ModelSource> ms;
  for (auto const& m : models) {
    auto meta_json = json_helper::ParseJsonString(m.metadata);
    ms[m.model_source].models.push_back(
        {m.model, meta_json["size"].asUInt64()});
    meta_json.removeMember("size");
    if (ms[m.model_source].metadata.isNull()) {
      ms[m.model_source].metadata = meta_json;
    }
    ms[m.model_source].id = m.model_source;
    ms[m.model_source].author = m.author_repo_id;
    LOG_DEBUG << m.model;
  }
  return ms;
}

cpp::result<ModelSource, std::string> ModelSourceService::GetModelSource(
    const std::string& src) {
  auto res = db_service_->GetModels(src);
  if (res.has_error()) {
    return cpp::fail(res.error());
  }

  auto& models = res.value();
  ModelSource ms;
  for (auto const& m : models) {
    auto meta_json = json_helper::ParseJsonString(m.metadata);
    ms.models.push_back({m.model, meta_json["size"].asUInt64()});
    meta_json.removeMember("size");
    if (ms.metadata.isNull()) {
      ms.metadata = meta_json;
    }
    ms.id = m.model_source;
    ms.author = m.author_repo_id;
    LOG_INFO << m.model;
  }
  return ms;
}

cpp::result<bool, std::string> ModelSourceService::AddHfOrg(
    const std::string& model_source, const std::string& author) {
  auto res = curl_utils::SimpleGet("https://huggingface.co/api/models?author=" +
                                   author);
  if (res.has_value()) {
    auto models = ParseJsonString(res.value());
    // Add new models
    for (auto const& m : models) {
      CTL_DBG(m.id);

      auto author_model = string_utils::SplitBy(m.id, "/");
      if (author_model.size() == 2) {
        auto const& author = author_model[0];
        auto const& model_name = author_model[1];
        auto r = AddHfRepo(model_source + "/" + model_name, author, model_name);
        if (r.has_error()) {
          CTL_WRN(r.error());
        }
      }
    }
  } else {
    return cpp::fail(res.error());
  }
  return true;
}

cpp::result<bool, std::string> ModelSourceService::AddHfRepo(
    const std::string& model_source, const std::string& author,
    const std::string& model_name) {
  // Get models from db

  auto model_list_before = db_service_->GetModels(model_source)
                               .value_or(std::vector<cortex::db::ModelEntry>{});
  std::unordered_set<std::string> updated_model_list;
  auto add_res = AddRepoSiblings(model_source, author, model_name);
  if (add_res.has_error()) {
    return cpp::fail(add_res.error());
  } else {
    updated_model_list = add_res.value();
  }
  for (auto const& mid : model_list_before) {
    if (updated_model_list.find(mid.model) == updated_model_list.end()) {
      if (auto del_res = db_service_->DeleteModelEntry(mid.model);
          del_res.has_error()) {
        CTL_INF(del_res.error());
      }
    }
  }
  return true;
}

cpp::result<std::unordered_set<std::string>, std::string>
ModelSourceService::AddRepoSiblings(const std::string& model_source,
                                    const std::string& author,
                                    const std::string& model_name) {
  std::unordered_set<std::string> res;
  auto repo_info = hu::GetHuggingFaceModelRepoInfo(author, model_name);
  if (repo_info.has_error()) {
    return cpp::fail(repo_info.error());
  }

  if (!repo_info->gguf.has_value()) {
    return cpp::fail(
        "Not a GGUF model. Currently, only GGUF single file is "
        "supported.");
  }

  auto siblings_fs = hu::GetSiblingsFileSize(author, model_name);

  if (siblings_fs.has_error()) {
    return cpp::fail("Could not get siblings file size: " + author + "/" +
                     model_name);
  }

  auto readme = hu::GetReadMe(author, model_name);
  std::string desc;
  if (!readme.has_error()) {
    desc = readme.value();
  }

  auto meta_json = json_helper::ParseJsonString(repo_info->metadata);
  auto& siblings_fs_v = siblings_fs.value();
  for (auto& m : meta_json["siblings"]) {
    auto r_file = m["rfilename"].asString();
    if (siblings_fs_v.file_sizes.find(r_file) !=
        siblings_fs_v.file_sizes.end()) {
      m["size"] = siblings_fs_v.file_sizes.at(r_file).size_in_bytes;
    }
  }
  meta_json["description"] = desc;
  LOG_DEBUG << meta_json.toStyledString();

  for (const auto& sibling : repo_info->siblings) {
    if (string_utils::EndsWith(sibling.rfilename, ".gguf")) {
      if (siblings_fs_v.file_sizes.find(sibling.rfilename) !=
          siblings_fs_v.file_sizes.end()) {
        meta_json["size"] =
            siblings_fs_v.file_sizes.at(sibling.rfilename).size_in_bytes;
      }
      std::string model_id =
          author + ":" + model_name + ":" + sibling.rfilename;
      cortex::db::ModelEntry e = {
          .model = model_id,
          .author_repo_id = author,
          .branch_name = "main",
          .path_to_model_yaml = "",
          .model_alias = "",
          .model_format = "hf-gguf",
          .model_source = model_source,
          .status = cortex::db::ModelStatus::Downloadable,
          .engine = "llama-cpp",
          .metadata = json_helper::DumpJsonString(meta_json)};
      if (!db_service_->HasModel(model_id)) {
        if (auto add_res = db_service_->AddModelEntry(e); add_res.has_error()) {
          CTL_INF(add_res.error());
        }
      } else {
        if (auto m = db_service_->GetModelInfo(model_id);
            m.has_value() &&
            m->status == cortex::db::ModelStatus::Downloadable) {
          if (auto upd_res = db_service_->UpdateModelEntry(model_id, e);
              upd_res.has_error()) {
            CTL_INF(upd_res.error());
          }
        }
      }
      res.insert(model_id);
    }
  }

  return res;
}

cpp::result<bool, std::string> ModelSourceService::AddCortexsoOrg(
    const std::string& model_source) {
  auto res = curl_utils::SimpleGet(
      "https://huggingface.co/api/models?author=cortexso");
  if (res.has_value()) {
    auto models = ParseJsonString(res.value());
    for (auto const& m : models) {
      CTL_INF(m.id);
      auto author_model = string_utils::SplitBy(m.id, "/");
      if (author_model.size() == 2) {
        auto const& author = author_model[0];
        auto const& model_name = author_model[1];
        auto r = AddCortexsoRepo(model_source + "/" + model_name, author,
                                 model_name);
        if (r.has_error()) {
          CTL_WRN(r.error());
        }
      }
    }
  } else {
    return cpp::fail(res.error());
  }

  return true;
}

cpp::result<bool, std::string> ModelSourceService::AddCortexsoRepo(
    const std::string& model_source, const std::string& author,
    const std::string& model_name) {
  auto branches =
      huggingface_utils::GetModelRepositoryBranches("cortexso", model_name);
  if (branches.has_error()) {
    return cpp::fail(branches.error());
  }

  auto repo_info = hu::GetHuggingFaceModelRepoInfo(author, model_name);
  if (repo_info.has_error()) {
    return cpp::fail(repo_info.error());
  }

  auto readme = hu::GetReadMe(author, model_name);
  std::string desc;
  if (!readme.has_error()) {
    desc = readme.value();
  }
  // Get models from db

  auto model_list_before = db_service_->GetModels(model_source)
                               .value_or(std::vector<cortex::db::ModelEntry>{});
  std::unordered_set<std::string> updated_model_list;

  for (auto const& [branch, _] : branches.value()) {
    CTL_INF(branch);
    auto add_res = AddCortexsoRepoBranch(model_source, author, model_name,
                                         branch, repo_info->metadata, desc)
                       .value_or(std::unordered_set<std::string>{});
    for (auto const& a : add_res) {
      updated_model_list.insert(a);
    }
  }

  // Clean up
  for (auto const& mid : model_list_before) {
    if (updated_model_list.find(mid.model) == updated_model_list.end()) {
      if (auto del_res = db_service_->DeleteModelEntry(mid.model);
          del_res.has_error()) {
        CTL_INF(del_res.error());
      }
    }
  }
  return true;
}

cpp::result<std::unordered_set<std::string>, std::string>
ModelSourceService::AddCortexsoRepoBranch(const std::string& model_source,
                                          const std::string& author,
                                          const std::string& model_name,
                                          const std::string& branch,
                                          const std::string& metadata,
                                          const std::string& desc) {
  std::unordered_set<std::string> res;

  url_parser::Url url = {
      .protocol = "https",
      .host = kHuggingFaceHost,
      .pathParams = {"api", "models", "cortexso", model_name, "tree", branch},
  };

  auto result = curl_utils::SimpleGetJson(url.ToFullPath());
  if (result.has_error()) {
    return cpp::fail("Model " + model_name + " not found");
  }

  bool has_gguf = false;
  uint64_t model_size = 0;
  for (const auto& value : result.value()) {
    auto path = value["path"].asString();
    if (path.find(".gguf") != std::string::npos) {
      has_gguf = true;
      model_size = value["size"].asUInt64();
    }
  }
  if (!has_gguf) {
    CTL_INF("Only support gguf file format! - branch: " << branch);
    return {};
  } else {
    auto meta_json = json_helper::ParseJsonString(metadata);
    meta_json["size"] = model_size;
    meta_json["description"] = desc;
    std::string model_id = model_name + ":" + branch;
    cortex::db::ModelEntry e = {
        .model = model_id,
        .author_repo_id = author,
        .branch_name = branch,
        .path_to_model_yaml = "",
        .model_alias = "",
        .model_format = "cortexso",
        .model_source = model_source,
        .status = cortex::db::ModelStatus::Downloadable,
        .engine = "llama-cpp",
        .metadata = json_helper::DumpJsonString(meta_json)};
    if (!db_service_->HasModel(model_id)) {
      CTL_INF("Adding model to db: " << model_name << ":" << branch);
      if (auto res = db_service_->AddModelEntry(e);
          res.has_error() || !res.value()) {
        CTL_DBG("Cannot add model to db: " << model_id);
      }
    } else {
      if (auto m = db_service_->GetModelInfo(model_id);
          m.has_value() && m->status == cortex::db::ModelStatus::Downloadable) {
        if (auto upd_res = db_service_->UpdateModelEntry(model_id, e);
            upd_res.has_error()) {
          CTL_INF(upd_res.error());
        }
      }
    }
    res.insert(model_id);
  }
  return res;
}

void ModelSourceService::SyncModelSource() {
  while (running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto now = std::chrono::system_clock::now();
    auto config = file_manager_utils::GetCortexConfig();
    auto last_check =
        std::chrono::system_clock::time_point(
            std::chrono::milliseconds(config.checkedForSyncHubAt)) +
        std::chrono::hours(1);

    if (now > last_check) {
      CTL_DBG("Start to sync cortex.db");

      auto res = db_service_->GetModelSources();
      if (res.has_error()) {
        CTL_INF(res.error());
      } else {
        for (auto const& src : res.value()) {
          CTL_DBG(src.model_source);
        }

        std::unordered_set<std::string> orgs;
        std::vector<std::string> repos;
        for (auto const& src : res.value()) {
          auto url_res = url_parser::FromUrlString(src.model_source);
          if (url_res.has_value()) {
            if (url_res->pathParams.size() == 1) {
              orgs.insert(src.model_source);
            } else if (url_res->pathParams.size() == 2) {
              repos.push_back(src.model_source);
            }
          }
        }

        // Get list to update
        std::vector<std::string> update_cand(orgs.begin(), orgs.end());
        auto get_org = [](const std::string& rp) {
          return rp.substr(0, rp.find_last_of("/"));
        };
        for (auto const& repo : repos) {
          if (orgs.find(get_org(repo)) != orgs.end()) {
            update_cand.push_back(repo);
          }
        }

        // Sync cortex.db with the upstream data
        for (auto const& c : update_cand) {
          if (auto res = AddModelSource(c); res.has_error()) {
            CTL_INF(res.error();)
          }
        }
      }

      CTL_DBG("Done sync cortex.db");

      auto now = std::chrono::system_clock::now();
      auto config = file_manager_utils::GetCortexConfig();
      config.checkedForSyncHubAt =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              now.time_since_epoch())
              .count();

      auto upd_config_res =
          config_yaml_utils::CortexConfigMgr::GetInstance().DumpYamlConfig(
              config, file_manager_utils::GetConfigurationPath().string());
      if (upd_config_res.has_error()) {
        CTL_ERR("Failed to update config file: " << upd_config_res.error());
      }
    }
  }
}
