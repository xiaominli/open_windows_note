#include "domain/DateTimeText.h"
#include <ctime>
#include <cstdio>
namespace own {

static bool readInt(const char*& p, int digits, int& out) {
    out = 0;
    for (int i = 0; i < digits; ++i) {
        if (p[i] < '0' || p[i] > '9') return false;
        out = out * 10 + (p[i] - '0');
    }
    p += digits;
    return true;
}
static bool localTm(int64_t t, std::tm& out) {
    time_t tt = (time_t)t;
#if defined(_WIN32)
    return localtime_s(&out, &tt) == 0;
#else
    std::tm* p = localtime(&tt); if (!p) return false; out = *p; return true;
#endif
}

bool parseLocalDateTime(const std::string& s, int64_t& out) {
    const char* p = s.c_str();
    int y, mo, d, h, mi;
    if (!readInt(p, 4, y)  || *p++ != '-' ||
        !readInt(p, 2, mo) || *p++ != '-' ||
        !readInt(p, 2, d)  || *p++ != ' ' ||
        !readInt(p, 2, h)  || *p++ != ':' ||
        !readInt(p, 2, mi) || *p != '\0') return false;
    if (y < 1970 || mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59) return false;
    std::tm g{};
    g.tm_year = y - 1900; g.tm_mon = mo - 1; g.tm_mday = d;
    g.tm_hour = h; g.tm_min = mi; g.tm_isdst = -1;
    time_t t = mktime(&g);
    if (t == (time_t)-1) return false;
    out = (int64_t)t;
    return true;
}

std::string formatLocalDateTime(int64_t t) {
    std::tm g{};
    if (!localTm(t, g)) return "";
    char buf[20];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d",
             g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min);
    return buf;
}

int64_t nextDayAt(int64_t now, int hour, int minute) {
    std::tm g{};
    if (!localTm(now, g)) return now + 86400;
    g.tm_mday += 1; g.tm_hour = hour; g.tm_min = minute; g.tm_sec = 0; g.tm_isdst = -1;
    time_t t = mktime(&g);
    return t == (time_t)-1 ? now + 86400 : (int64_t)t;
}

} // namespace own
