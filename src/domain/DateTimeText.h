#pragma once
#include <cstdint>
#include <string>
namespace own {
// 本地时区 "YYYY-MM-DD HH:MM" ↔ Unix 秒（提醒编辑的文本格式）
bool parseLocalDateTime(const std::string& s, int64_t& out);
std::string formatLocalDateTime(int64_t t);
int64_t nextDayAt(int64_t now, int hour, int minute);   // 明天 hour:minute（本地）
}
