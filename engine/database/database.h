#pragma once

#include <string>
#include "SQLiteCpp/SQLiteCpp.h"
#include "utils/file_manager_utils.h"

namespace cortex::db {
const std::string kDefaultDbPath =
    file_manager_utils::GetCortexDataPath().string() + "/cortex.db";
class Database {
 public:
  Database(Database const&) = delete;
  Database& operator=(Database const&) = delete;
  ~Database() {}

  static Database& GetInstance() {
    static Database db;
    return db;
  }

  SQLite::Database& db() { return db_; }

 private:
  Database()
      : db_(kDefaultDbPath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {}
  SQLite::Database db_;
};
}  // namespace cortex::db
