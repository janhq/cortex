#include "model_list_cmd.h"
#include <json/reader.h>
#include <json/value.h>
#include <iostream>

#include <vector>
#include "httplib.h"
#include "server_start_cmd.h"
#include "utils/logging_utils.h"
#include "utils/string_utils.h"
// clang-format off
#include <tabulate/table.hpp>
// clang-format on

namespace commands {
using namespace tabulate;
using Row_t =
    std::vector<variant<std::string, const char*, string_view, Table>>;

void ModelListCmd::Exec(const std::string& host, int port,
                        const std::string& filter, bool display_engine,
                        bool display_version, bool display_cpu_mode,
                        bool display_gpu_mode) {
  // Start server if server is not started yet
  if (!commands::IsServerAlive(host, port)) {
    CLI_LOG("Starting server ...");
    commands::ServerStartCmd ssc;
    if (!ssc.Exec(host, port)) {
      return;
    }
  }

  Table table;
  std::vector<std::string> column_headers{"(Index)", "ID"};
  if (display_engine) {
    column_headers.push_back("Engine");
  }
  if (display_version) {
    column_headers.push_back("Version");
  }

  if (display_cpu_mode) {
    column_headers.push_back("CPU Mode");
  }
  if (display_gpu_mode) {
    column_headers.push_back("GPU Mode");
  }
  Row_t header{column_headers.begin(), column_headers.end()};
  table.add_row(header);
  table.format().font_color(Color::green);
  int count = 0;
  // Iterate through directory

  httplib::Client cli(host + ":" + std::to_string(port));
  auto res = cli.Get("/v1/models");
  if (res) {
    if (res->status == httplib::StatusCode::OK_200) {
      Json::Value body;
      Json::Reader reader;
      reader.parse(res->body, body);
      if (!body["data"].isNull()) {
        for (auto const& v : body["data"]) {
          auto model_id = v["model"].asString();
          if (!filter.empty() &&
              !string_utils::StringContainsIgnoreCase(model_id, filter)) {
            continue;
          }

          count += 1;

          std::vector<std::string> row = {std::to_string(count),
                                          v["model"].asString()};
          if (display_engine) {
            row.push_back(v["engine"].asString());
          }
          if (display_version) {
            row.push_back(v["version"].asString());
          }

          if (auto& r = v["recommendation"]; !r.isNull()) {
            if (display_cpu_mode) {
              if (!r["cpu_mode"].isNull()) {
                row.push_back("RAM: " + r["cpu_mode"]["ram"].asString() +
                              " MiB");
              }
            }

            if (display_gpu_mode) {
              if (!r["gpu_mode"].isNull()) {
                std::string s;
                s += "ngl: " + r["gpu_mode"][0]["ngl"].asString() + " - ";
                s += "context: " +
                     r["gpu_mode"][0]["context_length"].asString() + " - ";
                s += "RAM: " + r["gpu_mode"][0]["ram"].asString() + " MiB - ";
                s += "VRAM: " + r["gpu_mode"][0]["vram"].asString() + " MiB - ";
                s += "recommend ngl: " +
                     r["gpu_mode"][0]["recommend_ngl"].asString();
                row.push_back(s);
              }
            }
          }

          table.add_row({row.begin(), row.end()});
        }
      }
    } else {
      CTL_ERR("Failed to get model list with status code: " << res->status);
      return;
    }
  } else {
    auto err = res.error();
    CTL_ERR("HTTP error: " << httplib::to_string(err));
    return;
  }

  std::cout << table << std::endl;
}
};  // namespace commands
