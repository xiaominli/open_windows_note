#pragma once
#include <string>
#include <cstdint>
struct sqlite3;
namespace own {
class Database {
public:
    Database() = default;
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&& o) noexcept;
    Database& operator=(Database&& o) noexcept;

    bool open(const std::string& path, std::string* err);
    bool openReadonly(const std::string& path, std::string* err);
    bool exec(const std::string& sql, std::string* err);
    int64_t lastInsertRowId() const;
    sqlite3* handle() const { return db_; }
    void close();

    int userVersion();
    void setUserVersion(int v);
    bool integrityOk();
private:
    sqlite3* db_ = nullptr;
};
} // namespace own
