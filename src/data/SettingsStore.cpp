#include "data/SettingsStore.h"
#include "data/Statement.h"
#include <cstdlib>

namespace own {

std::string SettingsStore::getString(const std::string& key, const std::string& def) {
    Statement st(db_, "SELECT value FROM settings WHERE key=?;");
    st.bind(1, key);
    if (st.step() && !st.columnIsNull(0)) {
        return st.columnText(0);
    }
    return def;
}

void SettingsStore::setString(const std::string& key, const std::string& value) {
    Statement st(db_,
        "INSERT INTO settings(key, value) VALUES(?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value;");
    st.bind(1, key);
    st.bind(2, value);
    st.execDone();
}

int SettingsStore::getInt(const std::string& key, int def) {
    Statement st(db_, "SELECT value FROM settings WHERE key=?;");
    st.bind(1, key);
    if (st.step() && !st.columnIsNull(0)) {
        std::string s = st.columnText(0);
        if (!s.empty()) {
            char* end = nullptr;
            long v = std::strtol(s.c_str(), &end, 10);
            if (end == s.c_str() + s.size()) {
                return static_cast<int>(v);
            }
        }
    }
    return def;
}

void SettingsStore::setInt(const std::string& key, int value) {
    setString(key, std::to_string(value));
}

} // namespace own
