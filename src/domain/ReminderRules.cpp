#include "domain/ReminderRules.h"
#include <ctime>
namespace own {

bool isDue(const Reminder& r, int64_t now) {
    if (!r.enabled) return false;
    if (r.snoozeUntil > 0) return now >= r.snoozeUntil;
    return now >= r.dueAt;
}

static int64_t addMonthsUtc(int64_t t, int months) {
    time_t tt = (time_t)t;
    std::tm g{};
#if defined(_WIN32)
    gmtime_s(&g, &tt);
#else
    g = *gmtime(&tt);
#endif
    g.tm_mon += months;                 // mktime/ _mkgmtime 会规整溢出月份
#if defined(_WIN32)
    return (int64_t)_mkgmtime(&g);
#else
    return (int64_t)timegm(&g);
#endif
}

int64_t computeNextDue(const Reminder& r, int64_t firedAt) {
    if (r.recurrence == Recurrence::None) return 0;
    int interval = r.recurInterval > 0 ? r.recurInterval : 1;
    int64_t next = r.dueAt;
    int guard = 0;
    while (next <= firedAt && guard++ < 100000) {
        switch (r.recurrence) {
            case Recurrence::Daily:   next += (int64_t)86400 * interval; break;
            case Recurrence::Weekly:  next += (int64_t)7 * 86400 * interval; break;
            case Recurrence::Monthly: next = addMonthsUtc(next, interval); break;
            default: return 0;
        }
    }
    return next;
}

int64_t snooze(int64_t now, int minutes) { return now + (int64_t)minutes * 60; }

} // namespace own
