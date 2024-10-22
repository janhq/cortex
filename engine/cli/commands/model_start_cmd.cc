#include "model_start_cmd.h"
#include "config/yaml_config.h"
#include "cortex_upd_cmd.h"
#include "database/models.h"
#include "httplib.h"
#include "run_cmd.h"
#include "server_start_cmd.h"
#include "utils/cli_selection_utils.h"
#include "utils/json_helper.h"
#include "utils/logging_utils.h"

namespace commands {
bool ModelStartCmd::Exec(const std::string& host, int port,
                         const std::string& model_handle) {
  std::optional<std::string> model_id =
      SelectLocalModel(model_service_, model_handle);

  if (!model_id.has_value()) {
    return false;
  }

  // Start server if server is not started yet
  if (!commands::IsServerAlive(host, port)) {
    CLI_LOG("Starting server ...");
    commands::ServerStartCmd ssc;
    if (!ssc.Exec(host, port)) {
      return false;
    }
  }
  // Call API to start model
  httplib::Client cli(host + ":" + std::to_string(port));
  Json::Value json_data;
  json_data["model"] = model_id.value();
  auto data_str = json_data.toStyledString();
  cli.set_read_timeout(std::chrono::seconds(60));
  auto res = cli.Post("/v1/models/start", httplib::Headers(), data_str.data(),
                      data_str.size(), "application/json");
  if (res) {
    if (res->status == httplib::StatusCode::OK_200) {
      CLI_LOG(model_id.value() << " model started successfully. Use `"
                               << commands::GetCortexBinary() << " run "
                               << *model_id << "` for interactive chat shell");
      return true;
    } else {
      auto root = json_helper::ParseJsonString(res->body);
      CLI_LOG(root["message"].asString());
      return false;
    }
  } else {
    auto err = res.error();
    CTL_ERR("HTTP error: " << httplib::to_string(err));
    return false;
  }
}

};  // namespace commands
