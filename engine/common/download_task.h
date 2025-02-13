#pragma once

#include <json/json.h>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>

enum class DownloadType {
  Model,
  Engine,
  Miscellaneous,
  CudaToolkit,
  Cortex,
  Environments
};

struct DownloadItem {

  std::string id;

  std::string downloadUrl;

  /**
   * An absolute path to where the file is located (locally).
   */
  std::filesystem::path localPath;

  std::optional<std::string> checksum;

  std::optional<uint64_t> bytes;

  std::optional<uint64_t> downloadedBytes;

  std::string ToString() const {
    std::ostringstream output;
    output << "DownloadItem{id: " << id << ", downloadUrl: " << downloadUrl
           << ", localContainerPath: " << localPath
           << ", bytes: " << bytes.value_or(0)
           << ", downloadedBytes: " << downloadedBytes.value_or(0)
           << ", checksum: " << checksum.value_or("N/A") << "}";
    return output.str();
  }
};

inline std::string DownloadTypeToString(DownloadType type) {
  switch (type) {
    case DownloadType::Model:
      return "Model";
    case DownloadType::Engine:
      return "Engine";
    case DownloadType::Miscellaneous:
      return "Miscellaneous";
    case DownloadType::CudaToolkit:
      return "CudaToolkit";
    case DownloadType::Cortex:
      return "Cortex";
    case DownloadType::Environments:
      return "Environments";
    default:
      return "Unknown";
  }
}

inline DownloadType DownloadTypeFromString(const std::string& str) {
  if (str == "Model") {
    return DownloadType::Model;
  } else if (str == "Engine") {
    return DownloadType::Engine;
  } else if (str == "Miscellaneous") {
    return DownloadType::Miscellaneous;
  } else if (str == "CudaToolkit") {
    return DownloadType::CudaToolkit;
  } else if (str == "Cortex") {
    return DownloadType::Cortex;
  } else if (str == "Environments") {
    return DownloadType::Environments;
  } else {
    return DownloadType::Miscellaneous;
  }
}

struct DownloadTask {
  enum class Status { Pending, InProgress, Completed, Cancelled, Error };

  std::string id;

  Status status;

  DownloadType type;

  std::vector<DownloadItem> items;

  bool resume;

  std::string ToString() const {
    std::ostringstream output;
    output << "DownloadTask{id: " << id << ", type: " << static_cast<int>(type)
           << ", items: [";
    for (const auto& item : items) {
      output << item.ToString() << ", ";
    }
    output << "]}";
    return output.str();
  }

  Json::Value ToJsonCpp() const {
    Json::Value root;
    root["id"] = id;
    root["type"] = DownloadTypeToString(type);

    Json::Value itemsArray(Json::arrayValue);
    for (const auto& item : items) {
      Json::Value itemObj;
      itemObj["id"] = item.id;
      itemObj["downloadUrl"] = item.downloadUrl;
      itemObj["localPath"] = item.localPath.string();
      itemObj["checksum"] = item.checksum.value_or("N/A");
      itemObj["bytes"] = Json::Value::UInt64(item.bytes.value_or(0));
      itemObj["downloadedBytes"] =
          Json::Value::UInt64(item.downloadedBytes.value_or(0));
      itemsArray.append(itemObj);
    }
    root["items"] = itemsArray;

    return root;
  }
};

namespace common {
inline DownloadItem GetDownloadItemFromJson(const Json::Value item_json) {
  DownloadItem item;
  if (!item_json["id"].isNull()) {
    item.id = item_json["id"].asString();
  }
  if (!item_json["downloadUrl"].isNull()) {
    item.downloadUrl = item_json["downloadUrl"].asString();
  }

  if (!item_json["localPath"].isNull()) {
    item.localPath = std::filesystem::path(item_json["localPath"].asString());
  }

  if (!item_json["checksum"].isNull()) {
    item.checksum = item_json["checksum"].asString();
  }

  if (!item_json["bytes"].isNull()) {
    item.bytes = item_json["bytes"].asUInt64();
  }

  if (!item_json["downloadedBytes"].isNull()) {
    item.downloadedBytes = item_json["downloadedBytes"].asUInt64();
  }

  return item;
}

inline DownloadTask GetDownloadTaskFromJson(const Json::Value item_json) {
  DownloadTask task;

  if (!item_json["id"].isNull()) {
    task.id = item_json["id"].asString();
  }

  if (!item_json["type"].isNull()) {
    task.type = DownloadTypeFromString(item_json["type"].asString());
  }

  if (!item_json["items"].isNull() && item_json["items"].isArray()) {
    for (auto const& i_json : item_json["items"]) {
      task.items.emplace_back(GetDownloadItemFromJson(i_json));
    }
  }
  return task;
}
}  // namespace common
