#include "server_start_cmd.h"
#include "commands/cortex_upd_cmd.h"
#include "services/engine_service.h"
#include "utils/cortex_utils.h"
#include "utils/file_manager_utils.h"
#include "utils/process/utils.h"

#if defined(_WIN32) || defined(_WIN64)
#include "utils/widechar_conv.h"
#endif

namespace commands {

namespace {
bool TryConnectToServer(const std::string& host, int port) {
  constexpr const auto kMaxRetry = 4u;
  auto count = 0u;
  // Check if server is started
  while (true) {
    if (IsServerAlive(host, port))
      break;
    // Wait for server up
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (count++ == kMaxRetry) {
      std::cerr << "Could not start server" << std::endl;
      return false;
    }
  }
  return true;
}
}  // namespace

bool ServerStartCmd::Exec(const std::string& host, int port,
                          const std::optional<std::string>& log_level) {
  if (IsServerAlive(host, port)) {
    CLI_LOG("The server has already started");
    return true;
  }
  std::string log_level_;
  if (!log_level.has_value()) {
    log_level_ = "INFO";
  } else {
    log_level_ = log_level.value();
  }
  auto exe = commands::GetCortexServerBinary();
  auto get_config_file_path = []() -> std::string {
    if (file_manager_utils::cortex_config_file_path.empty()) {
      return file_manager_utils::GetConfigurationPath().string();
    }
    return file_manager_utils::cortex_config_file_path;
  };

  auto get_data_folder_path = []() -> std::string {
    if (file_manager_utils::cortex_data_folder_path.empty()) {
      return file_manager_utils::GetCortexDataPath().string();
    }
    return file_manager_utils::cortex_data_folder_path;
  };

#if defined(_WIN32) || defined(_WIN64)
  // Windows-specific code to create a new process
  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  ZeroMemory(&pi, sizeof(pi));
  std::wstring params = L"--start-server";
  params += L" --config_file_path " +
            file_manager_utils::GetConfigurationPath().wstring();
  params += L" --data_folder_path " +
            file_manager_utils::GetCortexDataPath().wstring();
  params += L" --loglevel " + cortex::wc::Utf8ToWstring(log_level_);
  std::wstring exe_w = cortex::wc::Utf8ToWstring(exe);
  std::wstring current_path_w =
      file_manager_utils::GetExecutableFolderContainerPath().wstring();
  std::wstring wcmds = current_path_w + L"/" + exe_w + L" " + params;
  CTL_DBG("wcmds: " << wcmds);
  std::vector<wchar_t> mutable_cmds(wcmds.begin(), wcmds.end());
  mutable_cmds.push_back(L'\0');
  // Create child process
  if (!CreateProcess(
          NULL,                 // No module name (use command line)
          mutable_cmds
              .data(),          // Command line (replace with your actual executable)
          NULL,                 // Process handle not inheritable
          NULL,                 // Thread handle not inheritable
          FALSE,                // Set handle inheritance
          CREATE_NO_WINDOW,     // No new console
          NULL,                 // Use parent's environment block
          NULL,                 // Use parent's starting directory
          &si,                  // Pointer to STARTUPINFO structure
          &pi))                 // Pointer to PROCESS_INFORMATION structure
  {
    std::cout << "Could not start server: " << GetLastError() << std::endl;
    return false;
  } else {
    if (!TryConnectToServer(host, port)) {
      return false;
    }
    std::cout << "Server started" << std::endl;
    std::cout << "API Documentation available at: http://" << host << ":"
              << port << std::endl;
  }

#else
  std::vector<std::string> commands;
  // Some engines requires to add lib search path before process being created
  auto download_srv = std::make_shared<DownloadService>();
  auto dylib_path_mng = std::make_shared<cortex::DylibPathManager>();
  auto db_srv = std::make_shared<DatabaseService>();
  EngineService(download_srv, dylib_path_mng, db_srv).RegisterEngineLibPath();

  std::string p = cortex_utils::GetCurrentPath() + "/" + exe;
  commands.push_back(p);
  commands.push_back("--config_file_path");
  commands.push_back(get_config_file_path());
  commands.push_back("--data_folder_path");
  commands.push_back(get_data_folder_path());
  commands.push_back("--loglevel");
  commands.push_back(log_level_);
  auto pid = cortex::process::SpawnProcess(commands);
  if (pid < 0) {
    // Fork failed
    std::cerr << "Could not start server: " << std::endl;
    return false;
  } else {
    // Parent process
    if (!TryConnectToServer(host, port)) {
      return false;
    }
    std::cout << "Server started" << std::endl;
    std::cout << "API Documentation available at: http://" << host << ":"
              << port << std::endl;
  }
#endif
  return true;
}
};  // namespace commands
