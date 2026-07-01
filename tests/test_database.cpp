#include "doctest.h"
#include "data/Database.h"
#include "data/Statement.h"

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

TEST_CASE("statement bind/step roundtrip") {
    own::Database db; std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(db.exec("CREATE TABLE t(id INTEGER PRIMARY KEY, s TEXT, b BLOB);", &err));
    {
        own::Statement ins(db, "INSERT INTO t(s,b) VALUES(?,?);");
        ins.bind(1, std::string("abc"));
        std::vector<uint8_t> blob{1,2,3};
        ins.bindBlob(2, blob.data(), blob.size());
        ins.execDone();
    }
    own::Statement sel(db, "SELECT s,b FROM t WHERE id=1;");
    REQUIRE(sel.step());
    CHECK(sel.columnText(0) == "abc");
    CHECK(sel.columnBlob(1) == std::vector<uint8_t>{1,2,3});
}

TEST_CASE("transaction rollback on scope exit without commit") {
    own::Database db; std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(db.exec("CREATE TABLE t(id INTEGER PRIMARY KEY);", &err));
    { own::Transaction tx(db); db.exec("INSERT INTO t DEFAULT VALUES;", &err); }
    own::Statement cnt(db, "SELECT COUNT(*) FROM t;");
    REQUIRE(cnt.step());
    CHECK(cnt.columnInt64(0) == 0);
}
