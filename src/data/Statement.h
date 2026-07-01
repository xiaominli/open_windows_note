#pragma once
#include <string>
#include <vector>
#include <cstdint>
struct sqlite3_stmt;
namespace own {
class Database;
class Statement {
public:
    Statement(Database& db, const std::string& sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    void bind(int i, int64_t v);
    void bind(int i, const std::string& v);
    void bindBlob(int i, const uint8_t* p, size_t n);
    void bindNull(int i);
    bool step();
    void reset();
    void execDone();
    int64_t columnInt64(int c);
    std::string columnText(int c);
    std::vector<uint8_t> columnBlob(int c);
    bool columnIsNull(int c);
private:
    sqlite3_stmt* st_ = nullptr;
};
class Transaction {
public:
    explicit Transaction(Database& db);
    ~Transaction();
    void commit();
private:
    Database& db_;
    bool active_ = true;
};
} // namespace own

