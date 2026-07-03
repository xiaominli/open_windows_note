#include "data/BackupService.h"
#include "data/Database.h"
#include "data/Statement.h"
#include "domain/BackupRules.h"
#include <stdexcept>

namespace own {

bool exportBackup(Database& db, const std::string& destPathU8, std::string* err) {
    // VACUUM INTO 要求目标不存在；覆盖前的删除由调用方用宽字符 API 完成（ANSI CRT remove 在中文路径下不可靠）
    return db.exec("VACUUM INTO '" + escapeSqlLiteral(destPathU8) + "';", err);
}

bool validateBackupFile(const std::string& pathU8, std::string* err) {
    // 只读打开：文件不存在/无权限时 sqlite3_open_v2(..., SQLITE_OPEN_READONLY) 直接失败，
    // 不会像可写 open() 那样新建空库；避免了 ANSI fopen 探测在中文路径下的乱码问题。
    // sqlite3_open 对非库文件是惰性的（不解析文件头），真正的 SQLITE_NOTADB 只在首次
    // 执行语句时才会出现；Statement::step() 把该错误作为异常抛出，因此三道闸
    // （integrity_check / user_version / notes 表探测）都要接住异常，一律视为校验失败。
    try {
        Database db;
        if (!db.openReadonly(pathU8, err)) return false;
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
