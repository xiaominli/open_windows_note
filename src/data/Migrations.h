#pragma once
#include <string>
namespace own {
class Database;
constexpr int kSchemaVersion = 1;
bool migrate(Database& db, std::string* err);
}
