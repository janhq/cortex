#pragma once

#include <drogon/HttpController.h>
#include <trantor/utils/Logger.h>
#include "services/message_service.h"
#include "services/thread_service.h"

using namespace drogon;

class Threads : public drogon::HttpController<Threads, false> {
 public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(Threads::CreateThread, "/v1/threads", Options, Post);

  ADD_METHOD_TO(Threads::ListThreads,
                "/v1/"
                "threads?limit={limit}&order={order}&after={after}&before={"
                "before}",
                Get);

  ADD_METHOD_TO(Threads::RetrieveThread, "/v1/threads/{thread_id}", Get);
  ADD_METHOD_TO(Threads::ModifyThread, "/v1/threads/{thread_id}", Options,
                Patch);
  ADD_METHOD_TO(Threads::DeleteThread, "/v1/threads/{thread_id}", Options,
                Delete);
  METHOD_LIST_END

  explicit Threads(std::shared_ptr<ThreadService> thread_srv,
                   std::shared_ptr<MessageService> msg_srv)
      : thread_service_{thread_srv}, message_service_{msg_srv} {}

  void CreateThread(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback);

  void ListThreads(const HttpRequestPtr& req,
                   std::function<void(const HttpResponsePtr&)>&& callback,
                   std::optional<std::string> limit,
                   std::optional<std::string> order,
                   std::optional<std::string> after,
                   std::optional<std::string> before) const;

  void RetrieveThread(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback,
                      const std::string& thread_id) const;

  void ModifyThread(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback,
                    const std::string& thread_id);

  void DeleteThread(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback,
                    const std::string& thread_id);

 private:
  std::shared_ptr<ThreadService> thread_service_;
  std::shared_ptr<MessageService> message_service_;
};
