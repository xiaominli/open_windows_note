#include "doctest.h"
#include "data/Database.h"

TEST_CASE("open in-memory and exec create table") {
    own::Database db;
    std::string err;
    REQUIRE(db.open(":memory:", &err));
    CHECK(db.exec("CREATE TABLE t(id INTEGER PRIMARY KEY, v TEXT);", &err));
    CHECK(db.exec("INSERT INTO t(v) VALUES('hello');", &err));
    CHECK(db.lastInsertRowId() == 1);
}

TEST_CASE("exec on bad sql reports error") {
    own::Database db; std::string err;
    REQUIRE(db.open(":memory:", &err));
    CHECK_FALSE(db.exec("NOT VALID SQL;", &err));
    CHECK_FALSE(err.empty());
}
