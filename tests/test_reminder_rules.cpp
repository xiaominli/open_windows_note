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

TEST_CASE("computeNextDue monthly characterization: Jan31 +1mo drifts to Mar 3") {
    // P1 遗留决策：addMonthsUtc 溢出规整（2026-01-31 + 1mo → 2026-03-03 UTC）。
    // 1769817600 = 2026-01-31 00:00:00Z, 1772496000 = 2026-03-03 00:00:00Z
    own::Reminder r; r.recurrence = own::Recurrence::Monthly; r.recurInterval = 1;
    r.dueAt = 1769817600;
    CHECK(own::computeNextDue(r, 1769817600) == 1772496000);
}

TEST_CASE("resolveReminderDismiss disables one-shot and clears snooze") {
    own::Reminder r; r.dueAt = 1000; r.snoozeUntil = 500;
    r.recurrence = own::Recurrence::None; r.enabled = true;
    auto x = own::resolveReminderDismiss(r, 2000);
    CHECK_FALSE(x.enabled);
    CHECK(x.snoozeUntil == 0);
    CHECK(x.dueAt == 1000);          // 一次性不动 dueAt
}

TEST_CASE("resolveReminderDismiss advances recurring reminder") {
    own::Reminder r; r.dueAt = 1000; r.snoozeUntil = 999;
    r.recurrence = own::Recurrence::Daily; r.recurInterval = 1; r.enabled = true;
    auto x = own::resolveReminderDismiss(r, 1000);
    CHECK(x.enabled);
    CHECK(x.dueAt == 1000 + 86400);
    CHECK(x.snoozeUntil == 0);
}

TEST_CASE("resolveReminderSnooze only sets snoozeUntil") {
    own::Reminder r; r.dueAt = 1000; r.enabled = true;
    auto x = own::resolveReminderSnooze(r, 2000, 10);
    CHECK(x.snoozeUntil == 2000 + 600);
    CHECK(x.dueAt == 1000);
    CHECK(x.enabled);
}

TEST_CASE("pickDueReminders filters not-due and skip ids") {
    std::vector<own::Reminder> rs(3);
    rs[0].id = 1; rs[0].dueAt = 100;  rs[0].enabled = true;
    rs[1].id = 2; rs[1].dueAt = 100;  rs[1].enabled = true;   // 在 skip 里
    rs[2].id = 3; rs[2].dueAt = 9999; rs[2].enabled = true;   // 未到期
    auto due = own::pickDueReminders(rs, 200, { 2 });
    REQUIRE(due.size() == 1);
    CHECK(due[0].id == 1);
}

TEST_CASE("resolveReminderDismiss disables reminder when recurrence value is corrupt") {
    // DB 腐坏契约：computeNextDue 对未知 recurrence 返回 0 -> 兜底禁用
    own::Reminder r; r.dueAt = 1000; r.enabled = true; r.snoozeUntil = 500;
    r.recurrence = (own::Recurrence)99;
    auto x = own::resolveReminderDismiss(r, 1000);
    CHECK_FALSE(x.enabled);
    CHECK(x.snoozeUntil == 0);
}
