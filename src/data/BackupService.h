#pragma once
#include <string>
namespace own {
class Database;
// 备份：导出 = VACUUM INTO（原子、可在开库状态执行）；校验 = 开库 + integrity_check + schema 探测
bool exportBackup(Database& db, const std::string& destPathU8, std::string* err);
bool validateBackupFile(const std::string& pathU8, std::string* err);
}
