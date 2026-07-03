#include "domain/BackupRules.h"
#include <cstdio>
namespace own {
std::string escapeSqlLiteral(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) { out += c; if (c == '\'') out += '\''; }
    return out;
}
std::string defaultBackupName(int year, int month, int day, int hour, int minute) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "notes-backup-%04d%02d%02d-%02d%02d.db",
                  year, month, day, hour, minute);
    return buf;
}
}
