#pragma once
#include <string>
namespace own {
class Database;
// 备份：导出 = VACUUM INTO（原子、可在开库状态执行）；校验 = 只读开库 + integrity_check + schema 探测
// exportBackup 要求 destPathU8 尚不存在；覆盖前的删除由调用方用宽字符 API 完成（中文路径安全）
bool exportBackup(Database& db, const std::string& destPathU8, std::string* err);
bool validateBackupFile(const std::string& pathU8, std::string* err);
}
