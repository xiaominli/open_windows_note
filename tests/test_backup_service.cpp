#include "doctest.h"
#include "data/BackupService.h"
#include "data/Database.h"
#include "data/Migrations.h"
#include "data/NoteStore.h"
#include <cstdio>

static const char* kTmp = "test_backup_tmp.db";

TEST_CASE("exportBackup writes a valid, openable backup with data") {
    std::remove(kTmp);
    own::Database db;
    std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(own::migrate(db, &err));
    own::NoteStore s(db);
    own::Note n; n.plainText = "backup me";
    int64_t id = s.insertNote(n, 1000);

    CHECK(own::exportBackup(db, kTmp, &err));
    CHECK(own::validateBackupFile(kTmp, &err));

    own::Database back;
    REQUIRE(back.open(kTmp, &err));
    own::NoteStore bs(back);
    auto got = bs.getNote(id);
    REQUIRE(got.has_value());
    CHECK(got->plainText == "backup me");
    back.close();
    std::remove(kTmp);
}
TEST_CASE("exportBackup requires a non-existing destination") {
    std::remove(kTmp);
    { FILE* f = fopen(kTmp, "wb"); REQUIRE(f); fputs("junk", f); fclose(f); }
    own::Database db;
    std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(own::migrate(db, &err));
    CHECK_FALSE(own::exportBackup(db, kTmp, &err));   // dest exists -> VACUUM INTO fails
    std::remove(kTmp);
    CHECK(own::exportBackup(db, kTmp, &err));         // caller removed it -> succeeds
    CHECK(own::validateBackupFile(kTmp, &err));
    std::remove(kTmp);
}
TEST_CASE("validateBackupFile rejects missing and junk files") {
    std::string err;
    CHECK_FALSE(own::validateBackupFile("no_such_file_here.db", &err));
    std::remove(kTmp);
    { FILE* f = fopen(kTmp, "wb"); REQUIRE(f); fputs("this is not sqlite", f); fclose(f); }
    CHECK_FALSE(own::validateBackupFile(kTmp, &err));
    std::remove(kTmp);
}
