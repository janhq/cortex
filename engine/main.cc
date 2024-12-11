#include <drogon/HttpAppFramework.h>
#include <drogon/drogon.h>
#include <memory>
#include "controllers/assistants.h"
#include "controllers/configs.h"
#include "controllers/engines.h"
#include "controllers/events.h"
#include "controllers/files.h"
#include "controllers/hardware.h"
#include "controllers/messages.h"
#include "controllers/models.h"
#include "controllers/process_manager.h"
#include "controllers/server.h"
#include "controllers/threads.h"
#include "database/database.h"
#include "migrations/migration_manager.h"
#include "repositories/file_fs_repository.h"
#include "repositories/message_fs_repository.h"
#include "repositories/thread_fs_repository.h"
#include "services/assistant_service.h"
#include "services/config_service.h"
#include "services/file_watcher_service.h"
#include "services/message_service.h"
#include "services/model_service.h"
#include "services/thread_service.h"
#include "utils/archive_utils.h"
#include "utils/cortex_utils.h"
#include "utils/event_processor.h"
#include "utils/file_logger.h"
#include "utils/file_manager_utils.h"
#include "utils/logging_utils.h"
#include "utils/system_info_utils.h"

#if defined(__APPLE__) && defined(__MACH__)
#include <libgen.h>  // for dirname()
#include <mach-o/dyld.h>
#include <sys/types.h>
#elif defined(__linux__)
#include <libgen.h>  // for dirname()
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>  // for readlink()
#elif defined(_WIN32)
#include <windows.h>
#include "utils/widechar_conv.h"
#undef max
#else
#error "Unsupported platform!"
#endif

void RunServer(std::optional<int> port, bool ignore_cout) {
#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
  signal(SIGINT, SIG_IGN);
#elif defined(_WIN32)
  auto console_ctrl_handler = +[](DWORD ctrl_type) -> BOOL {
    return (ctrl_type == CTRL_C_EVENT) ? true : false;
  };
  SetConsoleCtrlHandler(
      reinterpret_cast<PHANDLER_ROUTINE>(console_ctrl_handler), true);
#endif
  auto config = file_manager_utils::GetCortexConfig();
  if (port.has_value() && *port != std::stoi(config.apiServerPort)) {
    auto config_path = file_manager_utils::GetConfigurationPath();
    config.apiServerPort = std::to_string(*port);
    auto result =
        config_yaml_utils::CortexConfigMgr::GetInstance().DumpYamlConfig(
            config, config_path.string());
    if (result.has_error()) {
      CTL_ERR("Error update " << config_path.string() << result.error());
    }
  }
  if (!ignore_cout) {
    std::cout << "Host: " << config.apiServerHost
              << " Port: " << config.apiServerPort << "\n";
  }
  // Create logs/ folder and setup log to file
  std::filesystem::create_directories(
#if defined(_WIN32)
      std::filesystem::u8path(config.logFolderPath) /
#else
      std::filesystem::path(config.logFolderPath) /
#endif
      std::filesystem::path(cortex_utils::logs_folder));
  static trantor::FileLogger asyncFileLogger;
  asyncFileLogger.setFileName(
      (std::filesystem::path(config.logFolderPath) /
       std::filesystem::path(cortex_utils::logs_base_name))
          .string());
  asyncFileLogger.setMaxLines(config.maxLogLines);  // Keep last 100000 lines
  asyncFileLogger.startLogging();
  trantor::Logger::setOutputFunction(
      [&](const char* msg, const uint64_t len) {
        asyncFileLogger.output_(msg, len);
      },
      [&]() { asyncFileLogger.flush(); });
  LOG_INFO << "Host: " << config.apiServerHost
           << " Port: " << config.apiServerPort << "\n";

  int thread_num = 1;

  int logical_cores = std::thread::hardware_concurrency();
  int drogon_thread_num = std::max(thread_num, logical_cores);

#ifdef CORTEX_CPP_VERSION
  LOG_INFO << "cortex.cpp version: " << CORTEX_CPP_VERSION;
#else
  LOG_INFO << "cortex.cpp version: undefined";
#endif

  auto hw_service = std::make_shared<services::HardwareService>();
  hw_service->UpdateHardwareInfos();
  if (hw_service->ShouldRestart()) {
    CTL_INF("Restart to update hardware configuration");
    hw_service->Restart(config.apiServerHost, std::stoi(config.apiServerPort));
    return;
  }

  using Event = cortex::event::Event;
  using EventQueue =
      eventpp::EventQueue<EventType,
                          void(const eventpp::AnyData<eventMaxSize>&)>;

  auto event_queue_ptr = std::make_shared<EventQueue>();
  cortex::event::EventProcessor event_processor(event_queue_ptr);

  auto data_folder_path = file_manager_utils::GetCortexDataPath();

  auto file_repo = std::make_shared<FileFsRepository>(data_folder_path);
  auto msg_repo = std::make_shared<MessageFsRepository>(data_folder_path);
  auto thread_repo = std::make_shared<ThreadFsRepository>(data_folder_path);

  auto file_srv = std::make_shared<FileService>(file_repo);
  auto assistant_srv = std::make_shared<AssistantService>(thread_repo);
  auto thread_srv = std::make_shared<ThreadService>(thread_repo);
  auto message_srv = std::make_shared<MessageService>(msg_repo);

  auto model_dir_path = file_manager_utils::GetModelsContainerPath();
  auto config_service = std::make_shared<ConfigService>();
  auto download_service =
      std::make_shared<DownloadService>(event_queue_ptr, config_service);
  auto engine_service = std::make_shared<EngineService>(download_service);
  auto inference_svc =
      std::make_shared<services::InferenceService>(engine_service);
  auto model_service = std::make_shared<ModelService>(
      download_service, inference_svc, engine_service);

  auto file_watcher_srv = std::make_shared<FileWatcherService>(
      model_dir_path.string(), model_service);
  file_watcher_srv->start();

  // initialize custom controllers
  auto file_ctl = std::make_shared<Files>(file_srv, message_srv);
  auto assistant_ctl = std::make_shared<Assistants>(assistant_srv);
  auto thread_ctl = std::make_shared<Threads>(thread_srv, message_srv);
  auto message_ctl = std::make_shared<Messages>(message_srv);
  auto engine_ctl = std::make_shared<Engines>(engine_service);
  auto model_ctl = std::make_shared<Models>(model_service, engine_service);
  auto event_ctl = std::make_shared<Events>(event_queue_ptr);
  auto pm_ctl = std::make_shared<ProcessManager>();
  auto hw_ctl = std::make_shared<Hardware>(engine_service, hw_service);
  auto server_ctl =
      std::make_shared<inferences::server>(inference_svc, engine_service);
  auto config_ctl = std::make_shared<Configs>(config_service);

  drogon::app().registerController(file_ctl);
  drogon::app().registerController(assistant_ctl);
  drogon::app().registerController(thread_ctl);
  drogon::app().registerController(message_ctl);
  drogon::app().registerController(engine_ctl);
  drogon::app().registerController(model_ctl);
  drogon::app().registerController(event_ctl);
  drogon::app().registerController(pm_ctl);
  drogon::app().registerController(server_ctl);
  drogon::app().registerController(hw_ctl);
  drogon::app().registerController(config_ctl);

  LOG_INFO << "Server started, listening at: " << config.apiServerHost << ":"
           << config.apiServerPort;
  LOG_INFO << "Please load your model";
#ifndef _WIN32
  drogon::app().enableReusePort();
#else
  drogon::app().enableDateHeader(false);
#endif
  drogon::app().addListener(config.apiServerHost,
                            std::stoi(config.apiServerPort));
  drogon::app().setThreadNum(drogon_thread_num);
  LOG_INFO << "Number of thread is:" << drogon::app().getThreadNum();
  drogon::app().disableSigtermHandling();

  // file upload
  drogon::app()
      .enableCompressedRequest(true)
      .setClientMaxBodySize(256 * 1024 * 1024)   // Max 256MiB body size
      .setClientMaxMemoryBodySize(1024 * 1024);  // 1MiB before writing to disk

  // CORS
  drogon::app().registerPostHandlingAdvice(
      [config_service](const drogon::HttpRequestPtr& req,
                       const drogon::HttpResponsePtr& resp) {
        if (!config_service->GetApiServerConfiguration()->cors) {
          CTL_INF("CORS is disabled!");
          return;
        }

        const std::string& origin = req->getHeader("Origin");
        CTL_INF("Origin: " << origin);

        auto allowed_origins =
            config_service->GetApiServerConfiguration()->allowed_origins;

        auto is_contains_asterisk =
            std::find(allowed_origins.begin(), allowed_origins.end(), "*");
        if (is_contains_asterisk != allowed_origins.end()) {
          resp->addHeader("Access-Control-Allow-Origin", "*");
          resp->addHeader("Access-Control-Allow-Methods", "*");
          return;
        }

        // Check if the origin is in our allowed list
        auto it =
            std::find(allowed_origins.begin(), allowed_origins.end(), origin);
        if (it != allowed_origins.end()) {
          resp->addHeader("Access-Control-Allow-Origin", origin);
        } else if (allowed_origins.empty()) {
          resp->addHeader("Access-Control-Allow-Origin", "*");
        }
        resp->addHeader("Access-Control-Allow-Methods", "*");
      });

  // ssl
  auto ssl_cert_path = config.sslCertPath;
  auto ssl_key_path = config.sslKeyPath;

  if (!ssl_cert_path.empty() && !ssl_key_path.empty()) {
    CTL_INF("SSL cert path: " << ssl_cert_path);
    CTL_INF("SSL key path: " << ssl_key_path);

    if (!std::filesystem::exists(ssl_cert_path) ||
        !std::filesystem::exists(ssl_key_path)) {
      CTL_ERR("SSL cert or key file not exist at specified path! Ignore..");
      return;
    }

    drogon::app().setSSLFiles(ssl_cert_path, ssl_key_path);
    drogon::app().addListener(config.apiServerHost, 443, true);
  }

  drogon::app().run();
  if (hw_service->ShouldRestart()) {
    CTL_INF("Restart to update hardware configuration");
    hw_service->Restart(config.apiServerHost, std::stoi(config.apiServerPort));
  }
}

#if defined(_WIN32)
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
  // Stop the program if the system is not supported
  auto system_info = system_info_utils::GetSystemInfo();
  if (system_info->arch == system_info_utils::kUnsupported ||
      system_info->os == system_info_utils::kUnsupported) {
    CLI_LOG_ERROR("Unsupported OS or architecture: " << system_info->os << ", "
                                                     << system_info->arch);
    return 1;
  }

  curl_global_init(CURL_GLOBAL_DEFAULT);

  // avoid printing logs to terminal
  is_server = true;

  std::optional<int> server_port;
  bool ignore_cout_log = false;
#if defined(_WIN32)
  for (int i = 0; i < argc; i++) {
    std::wstring command = argv[i];
    if (command == L"--config_file_path") {
      std::wstring v = argv[i + 1];
      file_manager_utils::cortex_config_file_path =
          cortex::wc::WstringToUtf8(v);
    } else if (command == L"--data_folder_path") {
      std::wstring v = argv[i + 1];
      file_manager_utils::cortex_data_folder_path =
          cortex::wc::WstringToUtf8(v);
    } else if (command == L"--port") {
      server_port = std::stoi(argv[i + 1]);
    } else if (command == L"--ignore_cout") {
      ignore_cout_log = true;
    } else if (command == L"--loglevel") {
      std::wstring v = argv[i + 1];
      std::string log_level = cortex::wc::WstringToUtf8(v);
      logging_utils_helper::SetLogLevel(log_level, ignore_cout_log);
    }
  }
#else
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--config_file_path") == 0) {
      file_manager_utils::cortex_config_file_path = argv[i + 1];
    } else if (strcmp(argv[i], "--data_folder_path") == 0) {
      file_manager_utils::cortex_data_folder_path = argv[i + 1];
    } else if (strcmp(argv[i], "--port") == 0) {
      server_port = std::stoi(argv[i + 1]);
    } else if (strcmp(argv[i], "--ignore_cout") == 0) {
      ignore_cout_log = true;
    } else if (strcmp(argv[i], "--loglevel") == 0) {
      std::string log_level = argv[i + 1];
      logging_utils_helper::SetLogLevel(log_level, ignore_cout_log);
    }
  }
#endif

  {
    auto result = file_manager_utils::CreateConfigFileIfNotExist();
    if (result.has_error()) {
      LOG_ERROR << "Error creating config file: " << result.error();
    }
    namespace fmu = file_manager_utils;
    // Override data folder path if it is configured and changed
    if (!fmu::cortex_data_folder_path.empty()) {
      auto cfg = file_manager_utils::GetCortexConfig();
      if (cfg.dataFolderPath != fmu::cortex_data_folder_path ||
          cfg.logFolderPath != fmu::cortex_data_folder_path) {
        cfg.dataFolderPath = fmu::cortex_data_folder_path;
        cfg.logFolderPath = fmu::cortex_data_folder_path;
        auto config_path = file_manager_utils::GetConfigurationPath();
        auto result =
            config_yaml_utils::CortexConfigMgr::GetInstance().DumpYamlConfig(
                cfg, config_path.string());
        if (result.has_error()) {
          CTL_ERR("Error update " << config_path.string() << result.error());
        }
      }
    }
  }

  // check if migration is needed
  if (auto res = cortex::migr::MigrationManager(
                     cortex::db::Database::GetInstance().db())
                     .Migrate();
      res.has_error()) {
    CLI_LOG("Error: " << res.error());
    return 1;
  }

  // Delete temporary file if it exists
  auto temp =
      file_manager_utils::GetExecutableFolderContainerPath() / "cortex_temp";
  if (std::filesystem::exists(temp)) {
    try {
      std::filesystem::remove(temp);
    } catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
    }
  }

  RunServer(server_port, ignore_cout_log);
  return 0;
}
