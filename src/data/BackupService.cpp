#include "data/BackupService.h"
#include "data/Database.h"
#include "data/Statement.h"
#include "domain/BackupRules.h"
#include <cstdio>
#include <stdexcept>

namespace own {

bool exportBackup(Database& db, const std::string& destPathU8, std::string* err) {
    std::remove(destPathU8.c_str());                 // VACUUM INTO 要求目标不存在（覆盖已在文件对话框确认）
    return db.exec("VACUUM INTO '" + escapeSqlLiteral(destPathU8) + "';", err);
}

bool validateBackupFile(const std::string& pathU8, std::string* err) {
    { // Database::open 会新建不存在的文件——先探测存在性，避免误报 + 残留空库
        FILE* f = fopen(pathU8.c_str(), "rb");
        if (!f) { if (err) *err = "file not found"; return false; }
        fclose(f);
    }
    // sqlite3_open 对非库文件是惰性的（不解析文件头），真正的 SQLITE_NOTADB 只在首次
    // 执行语句时才会出现；Statement::step() 把该错误作为异常抛出，因此三道闸
    // （integrity_check / user_version / notes 表探测）都要接住异常，一律视为校验失败。
    try {
        Database db;
        if (!db.open(pathU8, err)) return false;
        if (!db.integrityOk()) { if (err) *err = "integrity check failed"; return false; }
        if (db.userVersion() < 1) { if (err) *err = "not an open_windows_note database"; return false; }
        Statement s(db, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='notes';");
        if (!s.step() || s.columnInt64(0) != 1) {
            if (err) *err = "notes table missing";
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        if (err) *err = e.what();
        return false;
    }
}

} // namespace own
