#include "app/DbBootstrap.h"
#include "data/Database.h"
#include "data/Migrations.h"
#include <cstdio>
#include <string>
namespace own {
static void backupCorrupt(const std::string& path) {
    for (int i = 0; i < 1000; ++i) {
        std::string cand = path + ".corrupt." + std::to_string(i);
        if (std::rename(path.c_str(), cand.c_str()) == 0) return;  // 改名成功即备份
    }
    std::remove(path.c_str());   // 实在改不动就删掉，宁可重建也不卡启动
}
bool openDatabaseAtPath(const std::string& path, Database& outDb, std::string* err) {
    if (path.empty()) { if (err) *err = "empty db path"; return false; }
    if (!outDb.open(path, err)) return false;
    if (path != ":memory:" && !outDb.integrityOk()) {   // 损坏：备份+重建
        outDb.close();
        backupCorrupt(path);
        if (!outDb.open(path, err)) return false;
    }
    return migrate(outDb, err);
}
}
