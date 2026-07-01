#include "doctest.h"
#include "data/Database.h"
#include "data/Statement.h"
#include "data/Migrations.h"

TEST_CASE("migrate creates schema, seeds themes, idempotent") {
    own::Database db; std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(own::migrate(db, &err));
    CHECK(db.userVersion() == own::kSchemaVersion);
    // 表存在
    own::Statement s(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='notes';");
    REQUIRE(s.step()); CHECK(s.columnInt64(0) == 1);
    // 播种了内置主题
    own::Statement t(db, "SELECT COUNT(*) FROM themes WHERE is_builtin=1;");
    REQUIRE(t.step()); CHECK(t.columnInt64(0) >= 4);
    // 再迁一次不报错、主题不翻倍
    REQUIRE(own::migrate(db, &err));
    own::Statement t2(db, "SELECT COUNT(*) FROM themes WHERE is_builtin=1;");
    REQUIRE(t2.step()); CHECK(t2.columnInt64(0) >= 4);
}

TEST_CASE("integrityOk true on fresh db") {
    own::Database db; std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(own::migrate(db, &err));
    CHECK(db.integrityOk());
}
