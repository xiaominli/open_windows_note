#include "doctest.h"
#include "data/Database.h"
#include "data/Migrations.h"
#include "data/NoteStore.h"

static own::Database freshDb() {
    own::Database db; std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(own::migrate(db, &err));
    return db;
}
TEST_CASE("updateContent rewrites blob/plain_text/updated_at only") {
    auto db = freshDb(); own::NoteStore store(db);
    own::Note n; n.plainText = "old"; n.contentBlob = {1,2,3};
    int64_t id = store.insertNote(n, 1000);
    std::vector<uint8_t> blob = {9,8,7,6};
    CHECK(store.updateContent(id, blob, "new text", 2000));
    auto got = store.getNote(id);
    REQUIRE(got.has_value());
    CHECK(got->contentBlob == blob);
    CHECK(got->plainText == "new text");
    CHECK(got->updatedAt == 2000);
    CHECK(got->rect.w == n.rect.w);   // 其它字段不动
}
TEST_CASE("updateContent accepts empty blob") {
    auto db = freshDb(); own::NoteStore store(db);
    own::Note n; int64_t id = store.insertNote(n, 1000);
    std::vector<uint8_t> empty;
    CHECK(store.updateContent(id, empty, "", 3000));
    auto got = store.getNote(id);
    REQUIRE(got.has_value());
    CHECK(got->contentBlob.empty());
    CHECK(got->plainText == "");
}
