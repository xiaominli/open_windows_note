#include "doctest.h"
#include "domain/NoteListFormat.h"
using own::Note;

TEST_CASE("noteTitleText prefers title, falls back to plainText, then placeholder") {
    Note a; a.title = "Hello";
    CHECK(own::noteTitleText(a) == "Hello");
    Note b; b.plainText = "buy milk";
    CHECK(own::noteTitleText(b) == "buy milk");
    Note c;
    CHECK(own::noteTitleText(c) == "(\xE6\x97\xA0\xE6\xA0\x87\xE9\xA2\x98)"); // (无标题)
}

TEST_CASE("noteTitleText uses first line only") {
    Note d; d.plainText = "line1\nline2";
    CHECK(own::noteTitleText(d) == "line1");
}

TEST_CASE("formatRelativeTime buckets") {
    CHECK(own::formatRelativeTime(1000, 1000) == "\xE5\x88\x9A\xE5\x88\x9A");          // 刚刚
    CHECK(own::formatRelativeTime(1000, 990)  == "\xE5\x88\x9A\xE5\x88\x9A");          // 未来/极近 → 刚刚
    CHECK(own::formatRelativeTime(1000 + 120, 1000) == "2\xE5\x88\x86\xE9\x92\x9F\xE5\x89\x8D");   // 2分钟前
    CHECK(own::formatRelativeTime(1000 + 2*3600, 1000) == "2\xE5\xB0\x8F\xE6\x97\xB6\xE5\x89\x8D"); // 2小时前
    CHECK(own::formatRelativeTime(1000 + 3*86400, 1000) == "3\xE5\xA4\xA9\xE5\x89\x8D");            // 3天前
    CHECK(own::formatRelativeTime(100*86400, 0) == "1970-01-01");                       // 绝对日期(UTC)
}

TEST_CASE("sortNoteRows by updated and title") {
    Note a; a.title="b"; a.updatedAt=200;
    Note b; b.title="a"; b.updatedAt=100;
    std::vector<Note> v = { a, b };
    own::sortNoteRows(v, own::NoteSortKey::Updated, 1);   // 升序
    CHECK(v[0].updatedAt == 100);
    own::sortNoteRows(v, own::NoteSortKey::Updated, -1);  // 降序
    CHECK(v[0].updatedAt == 200);
    own::sortNoteRows(v, own::NoteSortKey::Title, 1);
    CHECK(v[0].title == "a");
}
