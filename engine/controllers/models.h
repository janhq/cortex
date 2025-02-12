#pragma once

#include <drogon/HttpController.h>
#include <trantor/utils/Logger.h>
#include "services/engine_service.h"
#include "services/model_service.h"
#include "services/model_source_service.h"

using namespace drogon;

class Models : public drogon::HttpController<Models, false> {
 public:
  METHOD_LIST_BEGIN
  METHOD_ADD(Models::PullModel, "/pull", Options, Post);
  METHOD_ADD(Models::GetModelPullInfo, "/pull/info", Options, Post);
  METHOD_ADD(Models::AbortPullModel, "/pull", Options, Delete);
  METHOD_ADD(Models::ListModel, "", Get);
  METHOD_ADD(Models::GetModel, "/{1}", Get);
  METHOD_ADD(Models::UpdateModel, "/{1}", Options, Patch);
  METHOD_ADD(Models::ImportModel, "/import", Options, Post);
  METHOD_ADD(Models::DeleteModel, "/{1}", Options, Delete);
  METHOD_ADD(Models::StartModel, "/start", Options, Post);
  METHOD_ADD(Models::StopModel, "/stop", Options, Post);
  METHOD_ADD(Models::GetModelStatus, "/status/{1}", Get);
  METHOD_ADD(Models::AddRemoteModel, "/add", Options, Post);
  METHOD_ADD(Models::GetRemoteModels, "/remote/{1}", Get);
  METHOD_ADD(Models::AddModelSource, "/sources", Post);
  METHOD_ADD(Models::DeleteModelSource, "/sources", Delete);
  METHOD_ADD(Models::GetModelSources, "/sources", Get);

  ADD_METHOD_TO(Models::PullModel, "/v1/models/pull", Options, Post);
  ADD_METHOD_TO(Models::AbortPullModel, "/v1/models/pull", Options, Delete);
  ADD_METHOD_TO(Models::ListModel, "/v1/models", Get);
  ADD_METHOD_TO(Models::GetModel, "/v1/models/{1}", Get);
  ADD_METHOD_TO(Models::UpdateModel, "/v1/models/{1}", Options, Patch);
  ADD_METHOD_TO(Models::ImportModel, "/v1/models/import", Options, Post);
  ADD_METHOD_TO(Models::DeleteModel, "/v1/models/{1}", Options, Delete);
  ADD_METHOD_TO(Models::StartModel, "/v1/models/start", Options, Post);
  ADD_METHOD_TO(Models::StopModel, "/v1/models/stop", Options, Post);
  ADD_METHOD_TO(Models::GetModelStatus, "/v1/models/status/{1}", Get);
  ADD_METHOD_TO(Models::AddRemoteModel, "/v1/models/add", Options, Post);
  ADD_METHOD_TO(Models::GetRemoteModels, "/v1/models/remote/{1}", Get);
  ADD_METHOD_TO(Models::AddModelSource, "/v1/models/sources", Post);
  ADD_METHOD_TO(Models::DeleteModelSource, "/v1/models/sources", Delete);
  ADD_METHOD_TO(Models::GetModelSources, "/v1/models/sources", Get);
  ADD_METHOD_TO(Models::GetModelSource, "/v1/models/sources/{src}", Get);
  METHOD_LIST_END

  explicit Models(std::shared_ptr<DatabaseService> db_service,
                  std::shared_ptr<ModelService> model_service,
                  std::shared_ptr<EngineService> engine_service,
                  std::shared_ptr<ModelSourceService> mss)
      : db_service_(db_service),
        model_service_{model_service},
        engine_service_{engine_service},
        model_src_svc_(mss) {}

  void PullModel(const HttpRequestPtr& req,
                 std::function<void(const HttpResponsePtr&)>&& callback);
  void GetModelPullInfo(
      const HttpRequestPtr& req,
      std::function<void(const HttpResponsePtr&)>&& callback) const;
  void AbortPullModel(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback);
  void ListModel(const HttpRequestPtr& req,
                 std::function<void(const HttpResponsePtr&)>&& callback) const;
  void GetModel(const HttpRequestPtr& req,
                std::function<void(const HttpResponsePtr&)>&& callback,
                const std::string& model_id) const;
  void UpdateModel(const HttpRequestPtr& req,
                   std::function<void(const HttpResponsePtr&)>&& callback,
                   const std::string& model_id) const;
  void ImportModel(
      const HttpRequestPtr& req,
      std::function<void(const HttpResponsePtr&)>&& callback) const;
  void AddRemoteModel(
      const HttpRequestPtr& req,
      std::function<void(const HttpResponsePtr&)>&& callback) const;
  void DeleteModel(const HttpRequestPtr& req,
                   std::function<void(const HttpResponsePtr&)>&& callback,
                   const std::string& model_id);
  void SetModelAlias(
      const HttpRequestPtr& req,
      std::function<void(const HttpResponsePtr&)>&& callback) const;

  void StartModel(const HttpRequestPtr& req,
                  std::function<void(const HttpResponsePtr&)>&& callback);

  void StopModel(const HttpRequestPtr& req,
                 std::function<void(const HttpResponsePtr&)>&& callback);

  void GetModelStatus(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback,
                      const std::string& model_id);

  void GetRemoteModels(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback,
                       const std::string& engine_id);

  void AddModelSource(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback);

  void DeleteModelSource(
      const HttpRequestPtr& req,
      std::function<void(const HttpResponsePtr&)>&& callback);

  void GetModelSources(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback);

  void GetModelSource(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback,
                      const std::string& src);

 private:
  std::shared_ptr<DatabaseService> db_service_;
  std::shared_ptr<ModelService> model_service_;
  std::shared_ptr<EngineService> engine_service_;
  std::shared_ptr<ModelSourceService> model_src_svc_;
};
