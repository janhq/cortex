#include "database_service.h"

// begin engines
std::optional<EngineEntry> DatabaseService::UpsertEngine(
    const std::string& engine_name, const std::string& type,
    const std::string& api_key, const std::string& url,
    const std::string& version, const std::string& variant,
    const std::string& status, const std::string& metadata) {
  return cortex::db::Engines().UpsertEngine(engine_name, type, api_key, url,
                                            version, variant, status, metadata);
}

std::optional<std::vector<EngineEntry>> DatabaseService::GetEngines() const {
  return cortex::db::Engines().GetEngines();
}

std::optional<EngineEntry> DatabaseService::GetEngineById(int id) const {
  return cortex::db::Engines().GetEngineById(id);
}

std::optional<EngineEntry> DatabaseService::GetEngineByNameAndVariant(
    const std::string& engine_name,
    const std::optional<std::string> variant) const {
  return cortex::db::Engines().GetEngineByNameAndVariant(engine_name, variant);
}

std::optional<std::string> DatabaseService::DeleteEngineById(int id) {
  return cortex::db::Engines().DeleteEngineById(id);
}
// end engines

// begin file
cpp::result<std::vector<OpenAi::File>, std::string>
DatabaseService::GetFileList() const {
  return cortex::db::File().GetFileList();
}

cpp::result<OpenAi::File, std::string> DatabaseService::GetFileById(
    const std::string& file_id) const {
  return cortex::db::File().GetFileById(file_id);
}

cpp::result<void, std::string> DatabaseService::AddFileEntry(
    OpenAi::File& file) {
  return cortex::db::File().AddFileEntry(file);
}

cpp::result<void, std::string> DatabaseService::DeleteFileEntry(
    const std::string& file_id) {
  return cortex::db::File().DeleteFileEntry(file_id);
}
// end file

// begin hardware
cpp::result<std::vector<HardwareEntry>, std::string>
DatabaseService::LoadHardwareList() const {
  return cortex::db::Hardware().LoadHardwareList();
}

cpp::result<bool, std::string> DatabaseService::AddHardwareEntry(
    const HardwareEntry& new_entry) {
  return cortex::db::Hardware().AddHardwareEntry(new_entry);
}

cpp::result<bool, std::string> DatabaseService::UpdateHardwareEntry(
    const std::string& id, const HardwareEntry& updated_entry) {
  return cortex::db::Hardware().UpdateHardwareEntry(id, updated_entry);
}

cpp::result<bool, std::string> DatabaseService::DeleteHardwareEntry(
    const std::string& id) {
  return cortex::db::Hardware().DeleteHardwareEntry(id);
}
// end hardware

// begin models
cpp::result<std::vector<ModelEntry>, std::string>
DatabaseService::LoadModelList() const {
  return cortex::db::Models().LoadModelList();
}

cpp::result<ModelEntry, std::string> DatabaseService::GetModelInfo(
    const std::string& identifier) const {
  return cortex::db::Models().GetModelInfo(identifier);
}

cpp::result<bool, std::string> DatabaseService::AddModelEntry(
    ModelEntry new_entry) {
  return cortex::db::Models().AddModelEntry(new_entry);
}

cpp::result<bool, std::string> DatabaseService::UpdateModelEntry(
    const std::string& identifier, const ModelEntry& updated_entry) {
  return cortex::db::Models().UpdateModelEntry(identifier, updated_entry);
}

cpp::result<bool, std::string> DatabaseService::DeleteModelEntry(
    const std::string& identifier) {
  return cortex::db::Models().DeleteModelEntry(identifier);
}

cpp::result<bool, std::string> DatabaseService::DeleteModelEntryWithOrg(
    const std::string& src) {
  return cortex::db::Models().DeleteModelEntryWithOrg(src);
}

cpp::result<bool, std::string> DatabaseService::DeleteModelEntryWithRepo(
    const std::string& src) {
  return cortex::db::Models().DeleteModelEntryWithRepo(src);
}

cpp::result<std::vector<std::string>, std::string>
DatabaseService::FindRelatedModel(const std::string& identifier) const {
  return cortex::db::Models().FindRelatedModel(identifier);
}

bool DatabaseService::HasModel(const std::string& identifier) const {
  return cortex::db::Models().HasModel(identifier);
}

cpp::result<std::vector<ModelEntry>, std::string> DatabaseService::GetModels(
    const std::string& model_src) const {
  return cortex::db::Models().GetModels(model_src);
}

cpp::result<std::vector<ModelEntry>, std::string>
DatabaseService::GetModelSources() const {
  return cortex::db::Models().GetModelSources();
}
// end models