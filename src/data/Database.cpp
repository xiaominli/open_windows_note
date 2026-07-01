#include "data/Database.h"
#include "sqlite3.h"

namespace own {

Database::~Database() { close(); }
Database::Database(Database&& o) noexcept : db_(o.db_) { o.db_ = nullptr; }
Database& Database::operator=(Database&& o) noexcept {
    if (this != &o) { close(); db_ = o.db_; o.db_ = nullptr; }
    return *this;
}
void Database::close() { if (db_) { sqlite3_close(db_); db_ = nullptr; } }

bool Database::open(const std::string& path, std::string* err) {
    close();
    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) { if (err) *err = db_ ? sqlite3_errmsg(db_) : "open failed"; return false; }
    sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
    return true;
}

bool Database::exec(const std::string& sql, std::string* err) {
    char* msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &msg);
    if (rc != SQLITE_OK) { if (err && msg) *err = msg; if (msg) sqlite3_free(msg); return false; }
    return true;
}

int64_t Database::lastInsertRowId() const { return sqlite3_last_insert_rowid(db_); }

} // namespace own
