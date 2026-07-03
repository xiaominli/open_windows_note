#pragma once
#include <string>
namespace own {
std::string escapeSqlLiteral(const std::string& s);                       // ' -> ''（SQL 字符串字面量）
std::string defaultBackupName(int year, int month, int day, int hour, int minute);
}
