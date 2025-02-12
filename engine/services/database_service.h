#pragma once
#include "database/engines.h"
#include "database/file.h"
#include "database/hardware.h"
#include "database/models.h"

using EngineEntry = cortex::db::EngineEntry;
using HardwareEntry = cortex::db::HardwareEntry;
using ModelEntry = cortex::db::ModelEntry;

class DatabaseService {
 public:
  // engines
  std::optional<EngineEntry> UpsertEngine(
      const std::string& engine_name, const std::string& type,
      const std::string& api_key, const std::string& url,
      const std::string& version, const std::string& variant,
      const std::string& status, const std::string& metadata);

  std::optional<std::vector<EngineEntry>> GetEngines() const;
  std::optional<EngineEntry> GetEngineById(int id) const;
  std::optional<EngineEntry> GetEngineByNameAndVariant(
      const std::string& engine_name,
      const std::optional<std::string> variant = std::nullopt) const;

  std::optional<std::string> DeleteEngineById(int id);

  // file
  cpp::result<std::vector<OpenAi::File>, std::string> GetFileList() const;

  cpp::result<OpenAi::File, std::string> GetFileById(
      const std::string& file_id) const;

  cpp::result<void, std::string> AddFileEntry(OpenAi::File& file);

  cpp::result<void, std::string> DeleteFileEntry(const std::string& file_id);

  // hardware
  cpp::result<std::vector<HardwareEntry>, std::string> LoadHardwareList() const;
  cpp::result<bool, std::string> AddHardwareEntry(
      const HardwareEntry& new_entry);
  cpp::result<bool, std::string> UpdateHardwareEntry(
      const std::string& id, const HardwareEntry& updated_entry);
  cpp::result<bool, std::string> DeleteHardwareEntry(const std::string& id);

  // models
  cpp::result<std::vector<ModelEntry>, std::string> LoadModelList() const;
  cpp::result<ModelEntry, std::string> GetModelInfo(
      const std::string& identifier) const;
  void PrintModelInfo(const ModelEntry& entry) const;
  cpp::result<bool, std::string> AddModelEntry(ModelEntry new_entry);
  cpp::result<bool, std::string> UpdateModelEntry(
      const std::string& identifier, const ModelEntry& updated_entry);
  cpp::result<bool, std::string> DeleteModelEntry(
      const std::string& identifier);
  cpp::result<bool, std::string> DeleteModelEntryWithOrg(
      const std::string& src);
  cpp::result<bool, std::string> DeleteModelEntryWithRepo(
      const std::string& src);
  cpp::result<std::vector<std::string>, std::string> FindRelatedModel(
      const std::string& identifier) const;
  bool HasModel(const std::string& identifier) const;
  cpp::result<std::vector<ModelEntry>, std::string> GetModels(
      const std::string& model_src) const;
  cpp::result<std::vector<ModelEntry>, std::string> GetModelSources()
      const;

 private:
};