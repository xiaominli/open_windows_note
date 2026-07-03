#include "doctest.h"
#include "domain/DateTimeText.h"

TEST_CASE("parse/format local datetime roundtrip") {
    int64_t t = 0;
    REQUIRE(own::parseLocalDateTime("2026-07-03 18:30", t));
    CHECK(own::formatLocalDateTime(t) == "2026-07-03 18:30");
    int64_t t2 = 0;
    REQUIRE(own::parseLocalDateTime("2026-07-03 18:31", t2));
    CHECK(t2 - t == 60);
}

TEST_CASE("parseLocalDateTime rejects malformed input") {
    int64_t t = 0;
    CHECK_FALSE(own::parseLocalDateTime("", t));
    CHECK_FALSE(own::parseLocalDateTime("2026-07-03", t));           // 缺时间
    CHECK_FALSE(own::parseLocalDateTime("2026-7-3 18:30", t));       // 位数不足
    CHECK_FALSE(own::parseLocalDateTime("2026-07-03 18:30:00", t));  // 尾随秒
    CHECK_FALSE(own::parseLocalDateTime("2026-07-03T18:30", t));     // 非空格分隔
    CHECK_FALSE(own::parseLocalDateTime("2026-13-01 10:00", t));     // 月越界
    CHECK_FALSE(own::parseLocalDateTime("2026-07-03 24:00", t));     // 时越界
    CHECK_FALSE(own::parseLocalDateTime("2026-07-03 10:60", t));     // 分越界
}

TEST_CASE("nextDayAt lands on tomorrow at given local time") {
    int64_t now = 0;
    REQUIRE(own::parseLocalDateTime("2026-07-03 23:59", now));
    CHECK(own::formatLocalDateTime(own::nextDayAt(now, 9, 0)) == "2026-07-04 09:00");
    REQUIRE(own::parseLocalDateTime("2026-07-03 01:00", now));
    CHECK(own::formatLocalDateTime(own::nextDayAt(now, 9, 0)) == "2026-07-04 09:00");
}
