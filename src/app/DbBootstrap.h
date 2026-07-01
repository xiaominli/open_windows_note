#pragma once
#include <string>
namespace own {
class Database;
bool openDatabaseAtPath(const std::string& path, Database& outDb, std::string* err);
}
