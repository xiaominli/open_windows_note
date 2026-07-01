#include "doctest.h"
#include "data/Database.h"
#include "data/Migrations.h"
#include "data/SettingsStore.h"

static own::Database freshDb() {
    own::Database db; std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(own::migrate(db, &err));
    return db;
}

TEST_CASE("settings string get default then set/get") {
    auto db = freshDb(); own::SettingsStore s(db);
    CHECK(s.getString("theme", "\xE9\xBB\x84") == "\xE9\xBB\x84");   // 黄
    s.setString("theme", "\xE8\x93\x9D");                            // 蓝
    CHECK(s.getString("theme", "\xE9\xBB\x84") == "\xE8\x93\x9D");
    s.setString("theme", "\xE7\xBB\xBF");                            // 绿 (upsert)
    CHECK(s.getString("theme", "\xE9\xBB\x84") == "\xE7\xBB\xBF");
}

TEST_CASE("settings int roundtrip and default on missing/garbage") {
    auto db = freshDb(); own::SettingsStore s(db);
    CHECK(s.getInt("opacity", 255) == 255);
    s.setInt("opacity", 128);
    CHECK(s.getInt("opacity", 255) == 128);
    s.setString("opacity", "notanumber");
    CHECK(s.getInt("opacity", 255) == 255);   // parse failure -> default
}
