#pragma once
#include <string>

namespace own {

class Database;

// Key/value application settings backed by the `settings` table.
class SettingsStore {
public:
    explicit SettingsStore(Database& db) : db_(db) {}
    std::string getString(const std::string& key, const std::string& def);
    void setString(const std::string& key, const std::string& value);
    int getInt(const std::string& key, int def);
    void setInt(const std::string& key, int value);
private:
    Database& db_;
};

} // namespace own
