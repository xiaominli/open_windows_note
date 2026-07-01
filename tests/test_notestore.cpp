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

TEST_CASE("insert then get roundtrips all fields") {
    auto db = freshDb();
    own::NoteStore store(db);
    own::Note n;
    n.type = own::NoteType::Checklist;
    n.title = "标题";
    n.contentBlob = {10,20,30};
    n.plainText = "买 牛奶";
    n.rect = {5,6,300,400};
    n.opacity = 128; n.pinned = false; n.rolledUp = true; n.visible = false;
    n.stickTarget = "chrome";
    int64_t id = store.insertNote(n, 1000);
    CHECK(id > 0);
    auto got = store.getNote(id);
    REQUIRE(got.has_value());
    CHECK(got->title == "标题");
    CHECK(got->contentBlob == std::vector<uint8_t>{10,20,30});
    CHECK(got->rect.w == 300);
    CHECK(got->opacity == 128);
    CHECK(got->pinned == false);
    CHECK(got->visible == false);
    CHECK(got->createdAt == 1000);
    CHECK(got->updatedAt == 1000);
    CHECK(static_cast<int>(got->type) == static_cast<int>(own::NoteType::Checklist));
}

TEST_CASE("update refreshes updated_at only") {
    auto db = freshDb(); own::NoteStore store(db);
    own::Note n; n.title = "a";
    int64_t id = store.insertNote(n, 1000);
    auto got = store.getNote(id); got->title = "b";
    REQUIRE(store.updateNote(*got, 2000));
    auto g2 = store.getNote(id);
    CHECK(g2->title == "b");
    CHECK(g2->createdAt == 1000);
    CHECK(g2->updatedAt == 2000);
}

TEST_CASE("delete removes note") {
    auto db = freshDb(); own::NoteStore store(db);
    int64_t id = store.insertNote(own::Note{}, 1000);
    REQUIRE(store.deleteNote(id));
    CHECK_FALSE(store.getNote(id).has_value());
}

TEST_CASE("allNotes sorted by updated_at desc") {
    auto db = freshDb(); own::NoteStore store(db);
    int64_t a = store.insertNote(own::Note{}, 1000);
    int64_t b = store.insertNote(own::Note{}, 3000);
    int64_t c = store.insertNote(own::Note{}, 2000);
    auto all = store.allNotes();
    REQUIRE(all.size() == 3);
    CHECK(all[0].id == b);
    CHECK(all[1].id == c);
    CHECK(all[2].id == a);
}

TEST_CASE("query filters by search substring on plain_text") {
    auto db = freshDb(); own::NoteStore store(db);
    own::Note a; a.plainText = "买 牛奶 面包"; store.insertNote(a, 1000);
    own::Note b; b.plainText = "开会 周一"; store.insertNote(b, 2000);
    own::NoteQuery q; q.search = "牛奶";
    auto r = store.query(q);
    REQUIRE(r.size() == 1);
    CHECK(r[0].plainText.find("牛奶") != std::string::npos);
}

TEST_CASE("query onlyVisible excludes hidden") {
    auto db = freshDb(); own::NoteStore store(db);
    own::Note a; a.visible = true;  store.insertNote(a, 1000);
    own::Note b; b.visible = false; store.insertNote(b, 2000);
    own::NoteQuery q; q.onlyVisible = true;
    CHECK(store.query(q).size() == 1);
}

TEST_CASE("query filters by group") {
    auto db = freshDb(); own::NoteStore store(db);
    own::Note a; a.groupId = 7; store.insertNote(a, 1000);
    own::Note b; b.groupId = 9; store.insertNote(b, 2000);
    own::NoteQuery q; q.groupId = 7;
    auto r = store.query(q);
    REQUIRE(r.size() == 1); CHECK(r[0].groupId == 7);
}

TEST_CASE("upsertTag is idempotent by name") {
    auto db = freshDb(); own::NoteStore s(db);
    int64_t t1 = s.upsertTag("工作");
    int64_t t2 = s.upsertTag("工作");
    CHECK(t1 == t2);
    CHECK(s.allTags().size() == 1);
}

TEST_CASE("tag attach/detach on note") {
    auto db = freshDb(); own::NoteStore s(db);
    int64_t nid = s.insertNote(own::Note{}, 1000);
    int64_t tid = s.upsertTag("紧急");
    REQUIRE(s.addTagToNote(nid, tid));
    REQUIRE(s.tagsOfNote(nid).size() == 1);
    REQUIRE(s.removeTagFromNote(nid, tid));
    CHECK(s.tagsOfNote(nid).empty());
}

TEST_CASE("deleteGroup nulls note.group_id") {
    auto db = freshDb(); own::NoteStore s(db);
    own::Group g; g.name="项目A"; int64_t gid = s.upsertGroup(g);
    own::Note n; n.groupId = gid; int64_t nid = s.insertNote(n, 1000);
    REQUIRE(s.deleteGroup(gid));
    CHECK(s.getNote(nid)->groupId == 0);
}

TEST_CASE("reminder crud and enabled filter") {
    auto db = freshDb(); own::NoteStore s(db);
    int64_t nid = s.insertNote(own::Note{}, 1000);
    own::Reminder r; r.noteId = nid; r.dueAt = 5000; r.enabled = true;
    int64_t rid = s.insertReminder(r);
    CHECK(rid > 0);
    own::Reminder r2; r2.noteId = nid; r2.dueAt = 6000; r2.enabled = false;
    s.insertReminder(r2);
    CHECK(s.remindersOfNote(nid).size() == 2);
    CHECK(s.enabledReminders().size() == 1);
}

TEST_CASE("query filters by tag via note_tags join") {
    auto db = freshDb(); own::NoteStore s(db);
    int64_t n1 = s.insertNote(own::Note{}, 1000);
    int64_t n2 = s.insertNote(own::Note{}, 2000);
    int64_t t = s.upsertTag("紧急");
    REQUIRE(s.addTagToNote(n1, t));
    own::NoteQuery q; q.tagId = t;
    auto r = s.query(q);
    REQUIRE(r.size() == 1);
    CHECK(r[0].id == n1);
}
