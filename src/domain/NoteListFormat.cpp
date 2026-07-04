#include "domain/NoteListFormat.h"
#include <algorithm>
#include <ctime>
#include <cstdio>
namespace own {
static std::string firstLine(const std::string& s, size_t maxBytes) {
    size_t nl = s.find('\n');
    std::string line = (nl == std::string::npos) ? s : s.substr(0, nl);
    if (line.size() > maxBytes) line = line.substr(0, maxBytes);
    return line;
}
std::string noteTitleText(const Note& n) {
    if (!n.title.empty()) return n.title;
    if (!n.plainText.empty()) return firstLine(n.plainText, 40);
    return "(\xE6\x97\xA0\xE6\xA0\x87\xE9\xA2\x98)";   // (无标题)
}
std::string noteWindowTitleText(const Note& n, bool rolledUp) {
    if (!n.title.empty()) return n.title;
    if (!rolledUp) return std::string();
    return noteTitleText(n);
}
std::string formatRelativeTime(int64_t nowSec, int64_t thenSec) {
    int64_t d = nowSec - thenSec;
    if (d < 60) return "\xE5\x88\x9A\xE5\x88\x9A";                                  // 刚刚
    if (d < 3600) return std::to_string(d / 60) + "\xE5\x88\x86\xE9\x92\x9F\xE5\x89\x8D";   // 分钟前
    if (d < 86400) return std::to_string(d / 3600) + "\xE5\xB0\x8F\xE6\x97\xB6\xE5\x89\x8D"; // 小时前
    if (d < 7 * 86400) return std::to_string(d / 86400) + "\xE5\xA4\xA9\xE5\x89\x8D";        // 天前
    time_t t = (time_t)thenSec;
    struct tm g;
#if defined(_WIN32)
    gmtime_s(&g, &t);
#else
    g = *gmtime(&t);
#endif
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", g.tm_year + 1900, g.tm_mon + 1, g.tm_mday);
    return std::string(buf);
}
void sortNoteRows(std::vector<Note>& rows, NoteSortKey key, int order) {
    if (order == 0) return;
    bool asc = order > 0;
    std::stable_sort(rows.begin(), rows.end(), [&](const Note& a, const Note& b) {
        if (key == NoteSortKey::Updated)
            return asc ? (a.updatedAt < b.updatedAt) : (a.updatedAt > b.updatedAt);
        std::string ta = noteTitleText(a), tb = noteTitleText(b);
        return asc ? (ta < tb) : (ta > tb);
    });
}
}
