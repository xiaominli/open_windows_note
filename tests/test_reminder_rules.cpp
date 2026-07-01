#include "doctest.h"
#include "domain/ReminderRules.h"

TEST_CASE("isDue respects dueAt and snooze") {
    own::Reminder r; r.enabled = true; r.dueAt = 1000; r.snoozeUntil = 0;
    CHECK_FALSE(own::isDue(r, 999));
    CHECK(own::isDue(r, 1000));
    r.snoozeUntil = 5000;
    CHECK_FALSE(own::isDue(r, 1000));   // 被贪睡压住
    CHECK(own::isDue(r, 5000));
    r.enabled = false;
    CHECK_FALSE(own::isDue(r, 9999));
}

TEST_CASE("computeNextDue none returns 0") {
    own::Reminder r; r.recurrence = own::Recurrence::None; r.dueAt = 1000;
    CHECK(own::computeNextDue(r, 1000) == 0);
}

TEST_CASE("computeNextDue daily rolls past firedAt") {
    own::Reminder r; r.recurrence = own::Recurrence::Daily; r.recurInterval = 1; r.dueAt = 1000;
    // 一天=86400。firedAt=1000 → 次日 87400
    CHECK(own::computeNextDue(r, 1000) == 1000 + 86400);
    // 已过好几天：firedAt=1000+86400*3+5 → 下一个是 1000+86400*4
    CHECK(own::computeNextDue(r, 1000 + 86400*3 + 5) == 1000 + 86400*4);
}

TEST_CASE("computeNextDue weekly interval 2") {
    own::Reminder r; r.recurrence = own::Recurrence::Weekly; r.recurInterval = 2; r.dueAt = 1000;
    CHECK(own::computeNextDue(r, 1000) == 1000 + 14*86400);
}

TEST_CASE("snooze adds minutes") {
    CHECK(own::snooze(1000, 10) == 1000 + 600);
}
