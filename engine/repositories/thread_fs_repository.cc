#include "thread_fs_repository.h"
#include <fstream>
#include <mutex>

cpp::result<std::vector<OpenAi::Thread>, std::string>
ThreadFsRepository::ListThreads(uint8_t limit, const std::string& order,
                                const std::string& after,
                                const std::string& before) const {
  CTL_INF("ListThreads: limit=" + std::to_string(limit) + ", order=" + order +
          ", after=" + after + ", before=" + before);
  std::vector<OpenAi::Thread> threads;

  try {
    auto thread_container_path = data_folder_path_ / kThreadContainerFolderName;
    for (const auto& entry :
         std::filesystem::directory_iterator(thread_container_path)) {
      if (!entry.is_directory())
        continue;

      if (!std::filesystem::exists(entry.path() / kThreadFileName))
        continue;

      auto current_thread_id = entry.path().filename().string();
      CTL_INF("ListThreads: Found thread: " + current_thread_id);
      std::shared_lock thread_lock(GrabThreadMutex(current_thread_id));

      auto thread_result = LoadThread(current_thread_id);
      if (thread_result.has_value()) {
        threads.push_back(std::move(thread_result.value()));
      }

      thread_lock.unlock();
    }

    return threads;
  } catch (const std::exception& e) {
    return cpp::fail(std::string("Failed to list threads: ") + e.what());
  }
}

std::shared_mutex& ThreadFsRepository::GrabThreadMutex(
    const std::string& thread_id) const {
  std::shared_lock map_lock(map_mutex_);
  auto it = thread_mutexes_.find(thread_id);
  if (it != thread_mutexes_.end()) {
    return *it->second;
  }

  map_lock.unlock();
  std::unique_lock map_write_lock(map_mutex_);
  return *thread_mutexes_
              .try_emplace(thread_id, std::make_unique<std::shared_mutex>())
              .first->second;
}

std::filesystem::path ThreadFsRepository::GetThreadPath(
    const std::string& thread_id) const {
  return data_folder_path_ / kThreadContainerFolderName / thread_id;
}

cpp::result<OpenAi::Thread, std::string> ThreadFsRepository::LoadThread(
    const std::string& thread_id) const {
  auto path = GetThreadPath(thread_id) / kThreadFileName;
  if (!std::filesystem::exists(path)) {
    return cpp::fail("Path does not exist: " + path.string());
  }

  try {
    std::ifstream file(path);
    if (!file.is_open()) {
      return cpp::fail("Failed to open file: " + path.string());
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    JSONCPP_STRING errs;

    if (!parseFromStream(builder, file, &root, &errs)) {
      return cpp::fail("Failed to parse JSON: " + errs);
    }

    return OpenAi::Thread::FromJson(root);
  } catch (const std::exception& e) {
    return cpp::fail("Failed to load thread: " + std::string(e.what()));
  }
}

cpp::result<void, std::string> ThreadFsRepository::CreateThread(
    OpenAi::Thread& thread) {
  CTL_INF("CreateThread: " + thread.id);
  std::unique_lock lock(GrabThreadMutex(thread.id));
  auto thread_path = GetThreadPath(thread.id);

  if (std::filesystem::exists(thread_path)) {
    return cpp::fail("Thread exists: " + thread.id);
  }

  std::filesystem::create_directories(thread_path);
  auto thread_file_path = thread_path / kThreadFileName;
  std::ofstream thread_file(thread_file_path);
  thread_file.close();

  return SaveThread(thread);
}

cpp::result<void, std::string> ThreadFsRepository::SaveThread(
    OpenAi::Thread& thread) {
  auto path = GetThreadPath(thread.id) / kThreadFileName;
  if (!std::filesystem::exists(path)) {
    return cpp::fail("Path does not exist: " + path.string());
  }

  std::ofstream file(path);
  try {
    if (!file) {
      return cpp::fail("Failed to open file: " + path.string());
    }
    file << thread.ToJson()->toStyledString();
    file.flush();
    file.close();
    return {};
  } catch (const std::exception& e) {
    file.close();
    return cpp::fail("Failed to save thread: " + std::string(e.what()));
  }
}

cpp::result<OpenAi::Thread, std::string> ThreadFsRepository::RetrieveThread(
    const std::string& thread_id) const {
  std::shared_lock lock(GrabThreadMutex(thread_id));
  return LoadThread(thread_id);
}

cpp::result<void, std::string> ThreadFsRepository::ModifyThread(
    OpenAi::Thread& thread) {
  std::unique_lock lock(GrabThreadMutex(thread.id));
  auto thread_path = GetThreadPath(thread.id);

  if (!std::filesystem::exists(thread_path)) {
    return cpp::fail("Thread doesn't exist: " + thread.id);
  }

  return SaveThread(thread);
}

cpp::result<void, std::string> ThreadFsRepository::DeleteThread(
    const std::string& thread_id) {
  CTL_INF("DeleteThread: " + thread_id);

  {
    std::unique_lock thread_lock(GrabThreadMutex(thread_id));
    auto path = GetThreadPath(thread_id);
    if (!std::filesystem::exists(path)) {
      return cpp::fail("Thread doesn't exist: " + thread_id);
    }
    try {
      std::filesystem::remove_all(path);
    } catch (const std::exception& e) {
      return cpp::fail(std::string("Failed to delete thread: ") + e.what());
    }
  }

  std::unique_lock map_lock(map_mutex_);
  thread_mutexes_.erase(thread_id);
  return {};
}
