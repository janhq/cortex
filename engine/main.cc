#include <drogon/HttpAppFramework.h>
#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>
#include <memory>
#include <mutex>
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
#include "controllers/swagger.h"
#include "controllers/threads.h"
#include "database/database.h"
#include "migrations/migration_manager.h"
#include "repositories/assistant_fs_repository.h"
#include "repositories/file_fs_repository.h"
#include "repositories/message_fs_repository.h"
#include "repositories/thread_fs_repository.h"
#include "services/assistant_service.h"
#include "services/config_service.h"
#include "services/database_service.h"
#include "services/file_watcher_service.h"
#include "services/message_service.h"
#include "services/model_service.h"
#include "services/model_source_service.h"
#include "services/thread_service.h"
#include "utils/archive_utils.h"
#include "utils/cortex_utils.h"
#include "utils/dylib_path_manager.h"
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

// Global var to signal drogon to shutdown
volatile bool shutdown_signal;

void RunServer(std::optional<std::string> host, std::optional<int> port,
               bool ignore_cout) {
#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
  auto signal_handler = +[](int sig) -> void {
    std::cout << "\rCaught interrupt signal, shutting down\n";
    shutdown_signal = true;
  };
  signal(SIGINT, signal_handler);
#elif defined(_WIN32)
  auto console_ctrl_handler = +[](DWORD ctrl_type) -> BOOL {
    if (ctrl_type == CTRL_C_EVENT) {
      std::cout << "\rCaught interrupt signal, shutting down\n";
      shutdown_signal = true;
      return TRUE;
    }
    return FALSE;
  };
  SetConsoleCtrlHandler(
      reinterpret_cast<PHANDLER_ROUTINE>(console_ctrl_handler), TRUE);
#endif
  auto config = file_manager_utils::GetCortexConfig();
  if (host.has_value() || port.has_value()) {
    if (host.has_value() && *host != config.apiServerHost) {
      config.apiServerHost = *host;
    }

    if (port.has_value() && *port != std::stoi(config.apiServerPort)) {
      config.apiServerPort = std::to_string(*port);
    }

    auto config_path = file_manager_utils::GetConfigurationPath();
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

  auto db_service = std::make_shared<DatabaseService>();
  auto hw_service = std::make_shared<HardwareService>(db_service);
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
  // utils
  auto dylib_path_manager = std::make_shared<cortex::DylibPathManager>();

  auto file_repo =
      std::make_shared<FileFsRepository>(data_folder_path, db_service);
  auto msg_repo = std::make_shared<MessageFsRepository>(data_folder_path);
  auto thread_repo = std::make_shared<ThreadFsRepository>(data_folder_path);
  auto assistant_repo =
      std::make_shared<AssistantFsRepository>(data_folder_path);

  auto file_srv = std::make_shared<FileService>(file_repo);
  auto assistant_srv =
      std::make_shared<AssistantService>(thread_repo, assistant_repo);
  auto thread_srv = std::make_shared<ThreadService>(thread_repo);
  auto message_srv = std::make_shared<MessageService>(msg_repo);

  auto model_dir_path = file_manager_utils::GetModelsContainerPath();
  auto config_service = std::make_shared<ConfigService>();
  auto download_service =
      std::make_shared<DownloadService>(event_queue_ptr, config_service);
  auto engine_service = std::make_shared<EngineService>(
      download_service, dylib_path_manager, db_service);
  auto inference_svc = std::make_shared<InferenceService>(engine_service);
  auto model_src_svc = std::make_shared<ModelSourceService>(db_service);
  auto model_service = std::make_shared<ModelService>(
      db_service, hw_service, download_service, inference_svc, engine_service);
  inference_svc->SetModelService(model_service);

  auto file_watcher_srv = std::make_shared<FileWatcherService>(
      model_dir_path.string(), model_service);
  file_watcher_srv->start();

  // initialize custom controllers
  auto swagger_ctl = std::make_shared<SwaggerController>(config.apiServerHost,
                                                         config.apiServerPort);
  auto file_ctl = std::make_shared<Files>(file_srv, message_srv);
  auto assistant_ctl = std::make_shared<Assistants>(assistant_srv);
  auto thread_ctl = std::make_shared<Threads>(thread_srv, message_srv);
  auto message_ctl = std::make_shared<Messages>(message_srv);
  auto engine_ctl = std::make_shared<Engines>(engine_service);
  auto model_ctl = std::make_shared<Models>(db_service, model_service,
                                            engine_service, model_src_svc);
  auto event_ctl = std::make_shared<Events>(event_queue_ptr);
  auto pm_ctl = std::make_shared<ProcessManager>(engine_service);
  auto hw_ctl = std::make_shared<Hardware>(engine_service, hw_service);
  auto server_ctl =
      std::make_shared<inferences::server>(inference_svc, engine_service);
  auto config_ctl = std::make_shared<Configs>(config_service);

  drogon::app().registerController(swagger_ctl);
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

  auto upload_path = std::filesystem::temp_directory_path() / "cortex-uploads";
  drogon::app().setUploadPath(upload_path.string());

#ifndef _WIN32
  drogon::app().enableReusePort();
#else
  drogon::app().enableDateHeader(false);
#endif
  try {
    drogon::app().addListener(config.apiServerHost,
                              std::stoi(config.apiServerPort));
  } catch (const std::exception& e) {
    LOG_ERROR << "Failed to start server: " << e.what();
    return;
  }

  LOG_INFO << "Server started, listening at: " << config.apiServerHost << ":"
           << config.apiServerPort;
  LOG_INFO << "Please load your model";

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

  // Fires up the server in another thread and set the shutdown signal if it somehow dies
  std::thread([] {
    drogon::app().run();
    shutdown_signal = true;
  }).detach();

  // Now this thread can monitor the shutdown signal
  while (!shutdown_signal) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  if (hw_service->ShouldRestart()) {
    CTL_INF("Restart to update hardware configuration");
    hw_service->Restart(config.apiServerHost, std::stoi(config.apiServerPort));
  }
  drogon::app().quit();
}

void print_help() {
  std::cout << "Usage: \ncortex-server [options]\n\n";
  std::cout << "Options:\n";
  std::cout << "  --config_file_path      Path to the config file (default: "
               "~/.cortexrc)\n";
  std::cout << "  --data_folder_path      Path to the data folder (default: "
               "~/cortexcpp)\n";
  std::cout << "  --host                  Host name (default: 127.0.0.1)\n";
  std::cout << "  --port                  Port number (default: 39281)\n";
  std::cout << "  --ignore_cout           Ignore cout output\n";
  std::cout << "  --loglevel              Set log level\n";

  exit(0);
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

  std::optional<std::string> server_host;
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
    } else if (command == L"--host") {
      server_host = cortex::wc::WstringToUtf8(argv[i + 1]);
    } else if (command == L"--port") {
      server_port = std::stoi(argv[i + 1]);
    } else if (command == L"--ignore_cout") {
      ignore_cout_log = true;
    } else if (command == L"--loglevel") {
      std::wstring v = argv[i + 1];
      std::string log_level = cortex::wc::WstringToUtf8(v);
      logging_utils_helper::SetLogLevel(log_level, ignore_cout_log);
    } else if (command == L"--help" || command == L"-h") {
      print_help();
    }
  }
#else
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--config_file_path") == 0) {
      file_manager_utils::cortex_config_file_path = argv[i + 1];
    } else if (strcmp(argv[i], "--data_folder_path") == 0) {
      file_manager_utils::cortex_data_folder_path = argv[i + 1];
    } else if (strcmp(argv[i], "--host") == 0) {
      server_host = argv[i + 1];
    } else if (strcmp(argv[i], "--port") == 0) {
      server_port = std::stoi(argv[i + 1]);
    } else if (strcmp(argv[i], "--ignore_cout") == 0) {
      ignore_cout_log = true;
    } else if (strcmp(argv[i], "--loglevel") == 0) {
      std::string log_level = argv[i + 1];
      logging_utils_helper::SetLogLevel(log_level, ignore_cout_log);
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      print_help();
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

  RunServer(server_host, server_port, ignore_cout_log);
  return 0;
}
