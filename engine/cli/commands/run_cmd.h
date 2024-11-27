#pragma once

#include <string>
#include <unordered_map>
#include "services/engine_service.h"
#include "services/model_service.h"

namespace commands {

std::optional<std::string> SelectLocalModel(std::string host, int port,
                                            ModelService& model_service,
                                            const std::string& model_handle);

class RunCmd {
 public:
  explicit RunCmd(std::string host, int port, std::string model_handle,
                  std::shared_ptr<DownloadService> download_service)
      : host_{std::move(host)},
        port_{port},
        model_handle_{std::move(model_handle)},
        download_service_(download_service),
        engine_service_{EngineService(download_service)},
        model_service_{ModelService(download_service)} {};

  void Exec(bool chat_flag,
            const std::unordered_map<std::string, std::string>& options);

 private:
  std::string host_;
  int port_;
  std::string model_handle_;

  std::shared_ptr<DownloadService> download_service_;
  ModelService model_service_;
  EngineService engine_service_;
};
}  // namespace commands
