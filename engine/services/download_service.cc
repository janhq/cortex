#include "download_service.h"
#include <curl/curl.h>
#include <stdio.h>
#include <filesystem>
#include <mutex>
#include <optional>
#include <ostream>
#include <utility>
#include "utils/curl_utils.h"
#include "utils/format_utils.h"
#include "utils/logging_utils.h"
#include "utils/result.hpp"
#include "utils/string_utils.h"

namespace {
size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  size_t written = fwrite(ptr, size, nmemb, (FILE*)userdata);
  return written;
}

cpp::result<void, std::string> ProcessCompletedTransfers(CURLM* multi_handle) {
  CURLMsg* msg;
  int msgs_left;
  while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
    if (msg->msg == CURLMSG_DONE) {
      auto handle = msg->easy_handle;
      auto result = msg->data.result;

      char* url = nullptr;
      curl_easy_getinfo(handle, CURLINFO_EFFECTIVE_URL, &url);

      if (result != CURLE_OK) {
        CTL_ERR("Transfer failed for URL: " << url << " Error: "
                                            << curl_easy_strerror(result));
        // download failed
        return cpp::fail("Transfer failed for URL: " + std::string(url) +
                         " Error: " + curl_easy_strerror(result));
      } else {
        long response_code;
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code == 200) {
          CTL_INF("Transfer completed for URL: " << url);
        } else if (response_code == 206) {
          CTL_INF("Transfer partially for URL: " << url);
        } else {
          CTL_ERR("Transfer failed with HTTP code: " << response_code
                                                     << " for URL: " << url);
          // download failed
          return cpp::fail("Transfer failed with HTTP code: " +
                           std::to_string(response_code) +
                           " for URL: " + std::string(url));
        }
      }
    }
  }
  return {};
}

void SetUpProxy(CURL* handle, std::shared_ptr<ConfigService> config_service) {
  auto configuration = config_service->GetApiServerConfiguration();
  if (configuration.has_value()) {
    if (!configuration->proxy_url.empty()) {
      auto proxy_url = configuration->proxy_url;
      auto verify_proxy_ssl = configuration->verify_proxy_ssl;
      auto verify_proxy_host_ssl = configuration->verify_proxy_host_ssl;

      auto verify_ssl = configuration->verify_peer_ssl;
      auto verify_host_ssl = configuration->verify_host_ssl;

      auto proxy_username = configuration->proxy_username;
      auto proxy_password = configuration->proxy_password;

      CTL_INF("=== Proxy configuration ===");
      CTL_INF("Proxy url: " << proxy_url);
      CTL_INF("Verify proxy ssl: " << verify_proxy_ssl);
      CTL_INF("Verify proxy host ssl: " << verify_proxy_host_ssl);
      CTL_INF("Verify ssl: " << verify_ssl);
      CTL_INF("Verify host ssl: " << verify_host_ssl);

      curl_easy_setopt(handle, CURLOPT_PROXY, proxy_url.c_str());
      if (string_utils::StartsWith(proxy_url, "https")) {
        curl_easy_setopt(handle, CURLOPT_PROXYTYPE, CURLPROXY_HTTPS);
      }

      curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, verify_ssl ? 1L : 0L);
      curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST,
                       verify_host_ssl ? 2L : 0L);

      curl_easy_setopt(handle, CURLOPT_PROXY_SSL_VERIFYPEER,
                       verify_proxy_ssl ? 1L : 0L);
      curl_easy_setopt(handle, CURLOPT_PROXY_SSL_VERIFYHOST,
                       verify_proxy_host_ssl ? 2L : 0L);

      auto proxy_auth = proxy_username + ":" + proxy_password;
      curl_easy_setopt(handle, CURLOPT_PROXYUSERPWD, proxy_auth.c_str());

      curl_easy_setopt(handle, CURLOPT_NOPROXY,
                       configuration->no_proxy.c_str());
    }
  } else {
    CTL_ERR("Failed to get configuration");
  }
}
}  // namespace

cpp::result<bool, std::string> DownloadService::AddDownloadTask(
    DownloadTask& task, std::optional<OnDownloadTaskSuccessfully> callback,
    bool show_progress) noexcept {
  std::optional<std::string> dl_err_msg = std::nullopt;
  bool has_task_done = false;
  for (const auto& item : task.items) {
    CLI_LOG("Start downloading: " + item.localPath.filename().string());
    auto result = Download(task.id, item, show_progress);
    if (result.has_error()) {
      dl_err_msg = result.error();
      break;
    } else if (result) {
      has_task_done |= result.value();
    }
  }
  if (dl_err_msg.has_value()) {
    return cpp::fail(dl_err_msg.value());
  }

  if (callback.has_value()) {
    callback.value()(task);
  }
  return has_task_done;
}

cpp::result<uint64_t, std::string> DownloadService::GetFileSize(
    const std::string& url) const noexcept {

  auto curl = curl_easy_init();
  if (!curl) {
    return cpp::fail(static_cast<std::string>("Failed to init CURL"));
  }

  SetUpProxy(curl, config_service_);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

  auto headers = curl_utils::GetHeaders(url);
  if (headers) {
    curl_slist* curl_headers = nullptr;

    for (const auto& [key, value] : headers->m) {
      auto header = key + ": " + value;
      curl_headers = curl_slist_append(curl_headers, header.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
  }
  auto res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    return cpp::fail(static_cast<std::string>(
        "CURL failed: " + std::string(curl_easy_strerror(res))));
  }

  curl_off_t content_length = 0;
  res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,
                          &content_length);
  return content_length;
}

cpp::result<bool, std::string> DownloadService::Download(
    const std::string& download_id, const DownloadItem& download_item,
    bool show_progress) noexcept {
  CTL_INF("Absolute file output: " << download_item.localPath.string());

  auto curl = curl_easy_init();
  if (!curl) {
    return cpp::fail(static_cast<std::string>("Failed to init CURL"));
  }

  std::string mode = "wb";
  if (std::filesystem::exists(download_item.localPath) &&
      download_item.bytes.has_value()) {
    try {
      curl_off_t existing_file_size =
          std::filesystem::file_size(download_item.localPath);

      CTL_INF("Existing file size: " << download_item.downloadUrl << " - "
                                     << download_item.localPath.string()
                                     << " - " << existing_file_size);
      CTL_INF("Download item size: " << download_item.bytes.value());
      auto missing_bytes = download_item.bytes.value() - existing_file_size;
      if (missing_bytes > 0 &&
          download_item.localPath.extension().string() != ".yaml" &&
          download_item.localPath.extension().string() != ".yml") {
        CLI_LOG("Found unfinished download! Additional "
                << format_utils::BytesToHumanReadable(missing_bytes)
                << " need to be downloaded.");
        std::cout << "Continue download [Y/n]: " << std::flush;
        std::string answer{""};
        std::getline(std::cin, answer);
        if (answer == "Y" || answer == "y" || answer.empty()) {
          mode = "ab";
          CLI_LOG("Resuming download..");
        } else {
          CLI_LOG("Start over..");
        }
      } else {
        CLI_LOG(download_item.localPath.filename().string()
                << " is already downloaded!");
        std::cout << "Re-download? [Y/n]: " << std::flush;

        std::string answer = "";
        std::getline(std::cin, answer);
        if (answer == "Y" || answer == "y" || answer.empty()) {
          CLI_LOG("Re-downloading..");
        } else {
          return false;
        }
      }
    } catch (const std::filesystem::filesystem_error& e) {
      CLI_LOG("Cannot get file size: "
              << e.what() << download_item.localPath.string() << "\n");
    }
  }

  auto file = fopen(download_item.localPath.string().c_str(), mode.c_str());
  if (!file) {
    return cpp::fail("Failed to open output file " +
                     download_item.localPath.string());
  }

  curl_easy_setopt(curl, CURLOPT_URL, download_item.downloadUrl.c_str());
  auto headers = curl_utils::GetHeaders(download_item.downloadUrl);
  if (headers) {
    curl_slist* curl_headers = nullptr;

    for (const auto& [key, value] : headers->m) {
      auto header = key + ": " + value;
      curl_headers = curl_slist_append(curl_headers, header.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
  }

  SetUpProxy(curl, config_service_);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
  if (show_progress) {
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  }
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  if (mode == "ab") {
    try {
      curl_off_t local_file_size =
          std::filesystem::file_size(download_item.localPath);
      curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, local_file_size);
    } catch (const std::filesystem::filesystem_error& e) {
      CTL_ERR("Cannot get file size: " << e.what() << '\n');
    }
  }

  auto res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    return cpp::fail("Download failed! Error: " +
                     static_cast<std::string>(curl_easy_strerror(res)));
  }

  fclose(file);
  curl_easy_cleanup(curl);
  return true;
}

cpp::result<std::string, std::string> DownloadService::StopTask(
    const std::string& task_id) {
  // First try to cancel in queue
  CTL_INF("Stopping task: " << task_id);
  auto cancelled = task_queue_.cancelTask(task_id);
  if (cancelled) {
    return task_id;
  }
  CTL_INF("Not found in pending task, try to find task " + task_id +
          " in active tasks");
  // Check if task is currently being processed
  std::lock_guard<std::mutex> lock(active_tasks_mutex_);
  if (auto it = active_tasks_.find(task_id); it != active_tasks_.end()) {
    CTL_INF("Found task " + task_id + " in active tasks");
    it->second->status = DownloadTask::Status::Cancelled;
    {
      std::lock_guard<std::mutex> lock(stop_mutex_);
      tasks_to_stop_.insert(task_id);
    }
    return task_id;
  }

  CTL_WRN("Task not found");
  return cpp::fail("Task not found");
}

void DownloadService::InitializeWorkers() {
  for (auto i = 0; i < MAX_CONCURRENT_TASKS; ++i) {
    auto worker_data = std::make_unique<WorkerData>();
    worker_data->multi_handle = curl_multi_init();
    worker_data_.push_back(std::move(worker_data));

    worker_threads_.emplace_back([this, i]() { this->WorkerThread(i); });
    CTL_INF("Starting worker thread: " << i);
  }
}

void DownloadService::Shutdown() {
  stop_flag_ = true;
  task_cv_.notify_all();  // Wake up all waiting threads

  for (auto& thread : worker_threads_) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  for (auto& worker_data : worker_data_) {
    curl_multi_cleanup(worker_data->multi_handle);
  }

  // Clean up any remaining callbacks
  std::lock_guard<std::mutex> lock(callbacks_mutex_);
  callbacks_.clear();
}

void DownloadService::WorkerThread(int worker_id) {
  auto& worker_data = worker_data_[worker_id];

  while (!stop_flag_) {
    std::unique_lock<std::mutex> lock(task_mutex_);

    // Wait for a task or stop signal
    task_cv_.wait(lock, [this] {
      auto pending_task = task_queue_.getNextPendingTask();
      return pending_task.has_value() || stop_flag_;
    });

    if (stop_flag_) {
      break;
    }

    auto maybe_task = task_queue_.pop();
    lock.unlock();

    if (!maybe_task || maybe_task->status == DownloadTask::Status::Cancelled) {
      continue;
    }

    auto task = std::move(maybe_task.value());

    // Register active task
    {
      std::lock_guard<std::mutex> active_lock(active_tasks_mutex_);
      active_tasks_[task.id] = std::make_shared<DownloadTask>(task);
    }

    ProcessTask(task, worker_id);

    // Remove from active tasks
    {
      std::lock_guard<std::mutex> active_lock(active_tasks_mutex_);
      active_tasks_.erase(task.id);
    }
  }
}

void DownloadService::ProcessTask(DownloadTask& task, int worker_id) {
  auto& worker_data = worker_data_[worker_id];
  std::vector<std::pair<CURL*, FILE*>> task_handles;

  task.status = DownloadTask::Status::InProgress;
  std::unordered_map<std::string, uint64_t> download_bytes;
  for (const auto& item : task.items) {
    auto handle = curl_easy_init();
    if (!handle) {
      CTL_ERR("Failed to init curl!");
      return;
    }
    download_bytes[item.id] = 0u;
    std::string mode = "wb";
    LOG_INFO << item.localPath.string() << " " << item.bytes.has_value();
    if (std::filesystem::exists(item.localPath) && item.bytes.has_value()) {
      try {
        curl_off_t existing_file_size =
            std::filesystem::file_size(item.localPath);

        CTL_INF("Existing file size: " << item.downloadUrl << " - "
                                       << item.localPath.string() << " - "
                                       << existing_file_size);
        CTL_INF("Download item size: " << item.bytes.value());
        auto missing_bytes = item.bytes.value() - existing_file_size;
        if (missing_bytes > 0 &&
            item.localPath.extension().string() != ".yaml" &&
            item.localPath.extension().string() != ".yml") {
          CTL_INF("Found unfinished download! Additional "
                  << format_utils::BytesToHumanReadable(missing_bytes)
                  << " need to be downloaded.");
          if (task.resume) {
            mode = "ab";
            download_bytes[item.id] = existing_file_size;
            CTL_INF("Resuming download..");
          } else {
            CTL_INF("Start over..");
          }
        } else {
          CTL_INF(item.localPath.filename().string()
                  << " is already downloaded! - Re-downloading..");
        }
      } catch (const std::filesystem::filesystem_error& e) {
        CTL_INF("Cannot get file size: " << e.what() << item.localPath.string()
                                         << "\n");
      }
    }
    auto file = fopen(item.localPath.string().c_str(), mode.c_str());
    if (!file) {
      CTL_ERR("Failed to open output file " + item.localPath.string());
      curl_easy_cleanup(handle);
      return;
    }
    auto dl_data_ptr = std::make_shared<DownloadingData>(
        DownloadingData{.task_id = task.id,
                        .item_id = item.id,
                        .download_service = this,
                        .download_bytes = download_bytes});
    worker_data->downloading_data_map[item.id] = dl_data_ptr;

    SetUpCurlHandle(handle, item, file, dl_data_ptr.get(),
                    mode == "ab" /*resume_download*/);
    curl_multi_add_handle(worker_data->multi_handle, handle);
    task_handles.push_back(std::make_pair(handle, file));
  }

  EmitTaskStarted(task);

  auto result =
      ProcessMultiDownload(task, worker_data->multi_handle, task_handles);

  // clean up
  for (auto& [handle, file] : task_handles) {
    curl_multi_remove_handle(worker_data->multi_handle, handle);
    curl_easy_cleanup(handle);
    fclose(file);
  }

  if (result.has_error()) {
    if (result.error().type == DownloadEventType::DownloadStopped) {
      RemoveTaskFromStopList(task.id);
      EmitTaskStopped(task.id);
    } else {
      EmitTaskError(task.id);
    }
  } else {
    // success
    // if the download has error, we are not run the callback
    ExecuteCallback(task);
    EmitTaskCompleted(task.id);
    {
      std::lock_guard<std::mutex> lock(event_emit_map_mutex);
      event_emit_map_.erase(task.id);
    }
  }

  worker_data->downloading_data_map.clear();
}

cpp::result<void, ProcessDownloadFailed> DownloadService::ProcessMultiDownload(
    DownloadTask& task, CURLM* multi_handle,
    const std::vector<std::pair<CURL*, FILE*>>& handles) {
  auto still_running = 0;
  do {
    curl_multi_perform(multi_handle, &still_running);
    curl_multi_wait(multi_handle, nullptr, 0, MAX_WAIT_MSECS, nullptr);

    auto result = ProcessCompletedTransfers(multi_handle);
    if (result.has_error()) {
      return cpp::fail(ProcessDownloadFailed{
          .message = result.error(),
          .task_id = task.id,
          .type = DownloadEventType::DownloadError,
      });
    }

    if (IsTaskTerminated(task.id) || stop_flag_) {
      CTL_INF("IsTaskTerminated " + std::to_string(IsTaskTerminated(task.id)));
      CTL_INF("stop_flag_ " + std::to_string(stop_flag_));
      return cpp::fail(ProcessDownloadFailed{
          .message = result.error(),
          .task_id = task.id,
          .type = DownloadEventType::DownloadStopped,
      });
    }
  } while (still_running);
  return {};
}

void DownloadService::SetUpCurlHandle(CURL* handle, const DownloadItem& item,
                                      FILE* file, DownloadingData* dl_data,
                                      bool resume_download) {
  SetUpProxy(handle, config_service_);
  curl_easy_setopt(handle, CURLOPT_URL, item.downloadUrl.c_str());
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, file);
  curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
  curl_easy_setopt(handle, CURLOPT_XFERINFODATA, dl_data);
  if (resume_download) {
    try {
      curl_off_t local_file_size = std::filesystem::file_size(item.localPath);
      curl_easy_setopt(handle, CURLOPT_RESUME_FROM_LARGE, local_file_size);
    } catch (const std::filesystem::filesystem_error& e) {
      CTL_ERR("Cannot get file size: " << e.what() << '\n');
    }
  }

  auto headers = curl_utils::GetHeaders(item.downloadUrl);
  if (headers) {
    curl_slist* curl_headers = nullptr;
    for (const auto& [key, value] : headers->m) {
      curl_headers =
          curl_slist_append(curl_headers, (key + ": " + value).c_str());
    }
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, curl_headers);
  }
}

cpp::result<DownloadTask, std::string> DownloadService::AddTask(
    DownloadTask& task, std::function<void(const DownloadTask&)> callback) {
  {  // adding item to callback map
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    callbacks_[task.id] = std::move(callback);
  }

  {  // adding task to queue
    std::lock_guard<std::mutex> lock(queue_mutex_);
    task_queue_.push(task);
    CTL_INF("Task added to queue: " << task.id);
  }

  task_cv_.notify_one();
  return task;
}

bool DownloadService::IsTaskTerminated(const std::string& task_id) {
  // can use shared mutex lock here?
  std::lock_guard<std::mutex> lock(stop_mutex_);
  return tasks_to_stop_.find(task_id) != tasks_to_stop_.end();
}

void DownloadService::RemoveTaskFromStopList(const std::string& task_id) {
  std::lock_guard<std::mutex> lock(stop_mutex_);
  tasks_to_stop_.erase(task_id);
}

void DownloadService::EmitTaskStarted(const DownloadTask& task) {
  event_queue_->enqueue(
      EventType::DownloadEvent,
      DownloadEvent{.type_ = DownloadEventType::DownloadStarted,
                    .download_task_ = task});
}

void DownloadService::EmitTaskStopped(const std::string& task_id) {
  if (auto it = active_tasks_.find(task_id); it != active_tasks_.end()) {
    event_queue_->enqueue(
        EventType::DownloadEvent,
        DownloadEvent{.type_ = DownloadEventType::DownloadStopped,
                      .download_task_ = *it->second});
  }
}

void DownloadService::EmitTaskError(const std::string& task_id) {
  if (auto it = active_tasks_.find(task_id); it != active_tasks_.end()) {
    event_queue_->enqueue(
        EventType::DownloadEvent,
        DownloadEvent{.type_ = DownloadEventType::DownloadError,
                      .download_task_ = *it->second});
  }
}

void DownloadService::EmitTaskCompleted(const std::string& task_id) {
  std::lock_guard<std::mutex> lock(active_tasks_mutex_);
  if (auto it = active_tasks_.find(task_id); it != active_tasks_.end()) {
    for (auto& item : it->second->items) {
      item.downloadedBytes = item.bytes;
    }
    event_queue_->enqueue(
        EventType::DownloadEvent,
        DownloadEvent{.type_ = DownloadEventType::DownloadSuccess,
                      .download_task_ = *it->second});
  }
}

void DownloadService::ExecuteCallback(const DownloadTask& task) {
  std::lock_guard<std::mutex> active_task_lock(active_tasks_mutex_);
  if (auto it = active_tasks_.find(task.id); it != active_tasks_.end()) {
    for (auto& item : it->second->items) {
      item.downloadedBytes = item.bytes;
    }
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    auto callback = callbacks_.find(task.id);
    if (callback != callbacks_.end()) {
      callback->second(*it->second);
      callbacks_.erase(callback);
    }
  }
}
