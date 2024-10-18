#include <memory>
#include "command_line_parser.h"
#include "commands/cortex_upd_cmd.h"
#include "cortex-common/cortexpythoni.h"
#include "services/download_service.h"
#include "services/model_service.h"
#include "utils/archive_utils.h"
#include "utils/cortex_utils.h"
#include "utils/dylib.h"
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
#undef max
#else
#error "Unsupported platform!"
#endif

void RemoveBinaryTempFileIfExists() {
  auto temp =
      file_manager_utils::GetExecutableFolderContainerPath() / "cortex_temp";
  if (std::filesystem::exists(temp)) {
    try {
      std::filesystem::remove(temp);
    } catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
    }
  }
}

void SetupLogger(trantor::FileLogger& async_logger, bool verbose) {
  if (!verbose) {
    auto config = file_manager_utils::GetCortexConfig();
    std::filesystem::create_directories(
        std::filesystem::path(config.logFolderPath) /
        std::filesystem::path(cortex_utils::logs_folder));
    async_logger.setFileName(config.logFolderPath + "/" +
                                cortex_utils::logs_cli_base_name);
    async_logger.setMaxLines(config.maxLogLines);  // Keep last 100000 lines
    async_logger.startLogging();
    trantor::Logger::setOutputFunction(
        [&](const char* msg, const uint64_t len) {
          async_logger.output_(msg, len);
        },
        [&]() { async_logger.flush(); });
  }
}

void InstallServer() {
#if !defined(_WIN32)
  if (getuid()) {
    CLI_LOG("Error: Not root user. Please run with sudo.");
    return;
  }
#endif
  auto cuc = commands::CortexUpdCmd(std::make_shared<DownloadService>());
  cuc.Exec({}, true /*force*/);
}

int main(int argc, char* argv[]) {
  // Stop the program if the system is not supported
  auto system_info = system_info_utils::GetSystemInfo();
  if (system_info->arch == system_info_utils::kUnsupported ||
      system_info->os == system_info_utils::kUnsupported) {
    CTL_ERR("Unsupported OS or architecture: " << system_info->os << ", "
                                               << system_info->arch);
    return 1;
  }

  bool should_install_server = false;
  bool verbose = false;
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--config_file_path") == 0) {
      file_manager_utils::cortex_config_file_path = argv[i + 1];

    } else if (strcmp(argv[i], "--data_folder_path") == 0) {
      file_manager_utils::cortex_data_folder_path = argv[i + 1];
    } else if ((strcmp(argv[i], "--server") == 0) &&
               (strcmp(argv[i - 1], "update") == 0)) {
      should_install_server = true;
    } else if (strcmp(argv[i], "--verbose") == 0) {
      verbose = true;
    }
  }

  { file_manager_utils::CreateConfigFileIfNotExist(); }

  RemoveBinaryTempFileIfExists();

  trantor::FileLogger async_file_logger;
  SetupLogger(async_file_logger, verbose);

  if (should_install_server) {
    InstallServer(); 
    return 0;
  }

  // Check if server exists, if not notify to user to install server
  auto exe = commands::GetCortexServerBinary();
  auto server_binary_path =
      std::filesystem::path(cortex_utils::GetCurrentPath()) /
      std::filesystem::path(exe);
  if (!std::filesystem::exists(server_binary_path)) {
    std::cout << CORTEX_CPP_VERSION
              << " requires server binary, to install server, run: "
              << commands::GetRole() << commands::GetCortexBinary()
              << " update --server" << std::endl;
    return 0;
  }

  CommandLineParser clp;
  clp.SetupCommand(argc, argv);
  return 0;
}
