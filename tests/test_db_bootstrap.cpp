#include "doctest.h"
#include "app/DbBootstrap.h"
#include "data/Database.h"
#include "data/NoteStore.h"
#include "data/Statement.h"
#include <cstdio>

TEST_CASE("openDatabaseAtPath creates and migrates a fresh file") {
    std::string path = "test_boot_tmp.db";
    std::remove(path.c_str());
    own::Database db; std::string err;
    REQUIRE(own::openDatabaseAtPath(path, db, &err));
    // migrate 建了 themes（内置>=4）
    own::Statement s(db, "SELECT COUNT(*) FROM themes WHERE is_builtin=1;");
    REQUIRE(s.step()); CHECK(s.columnInt64(0) >= 4);
    db.close();
    std::remove(path.c_str());
}
