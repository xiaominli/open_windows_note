#include "data/Statement.h"
#include "data/Database.h"
#include "sqlite3.h"
#include <stdexcept>

namespace own {

Statement::Statement(Database& db, const std::string& sql) {
    if (sqlite3_prepare_v2(db.handle(), sql.c_str(), -1, &st_, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db.handle()));
}
Statement::~Statement() { if (st_) sqlite3_finalize(st_); }
void Statement::bind(int i, int64_t v) { sqlite3_bind_int64(st_, i, v); }
void Statement::bind(int i, const std::string& v) {
    sqlite3_bind_text(st_, i, v.c_str(), (int)v.size(), SQLITE_TRANSIENT);
}
void Statement::bindBlob(int i, const uint8_t* p, size_t n) {
    sqlite3_bind_blob(st_, i, p, (int)n, SQLITE_TRANSIENT);
}
void Statement::bindNull(int i) { sqlite3_bind_null(st_, i); }
bool Statement::step() {
    int rc = sqlite3_step(st_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw std::runtime_error(sqlite3_errmsg(sqlite3_db_handle(st_)));
}
void Statement::reset() { sqlite3_reset(st_); }
void Statement::execDone() { while (step()) {} }
int64_t Statement::columnInt64(int c) { return sqlite3_column_int64(st_, c); }
std::string Statement::columnText(int c) {
    const unsigned char* p = sqlite3_column_text(st_, c);
    int n = sqlite3_column_bytes(st_, c);
    return p ? std::string(reinterpret_cast<const char*>(p), n) : std::string();
}
std::vector<uint8_t> Statement::columnBlob(int c) {
    const void* p = sqlite3_column_blob(st_, c);
    int n = sqlite3_column_bytes(st_, c);
    const uint8_t* b = static_cast<const uint8_t*>(p);
    return b ? std::vector<uint8_t>(b, b + n) : std::vector<uint8_t>();
}
bool Statement::columnIsNull(int c) { return sqlite3_column_type(st_, c) == SQLITE_NULL; }

Transaction::Transaction(Database& db) : db_(db) {
    std::string e; db_.exec("BEGIN;", &e);
}
Transaction::~Transaction() { if (active_) { std::string e; db_.exec("ROLLBACK;", &e); } }
void Transaction::commit() { if (active_) { std::string e; db_.exec("COMMIT;", &e); active_ = false; } }

} // namespace own
