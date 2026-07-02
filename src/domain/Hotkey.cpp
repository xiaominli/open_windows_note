#include "domain/Hotkey.h"
#include <vector>
#include <cctype>
namespace own {
static std::string lower(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t"); if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t"); return s.substr(a, b - a + 1);
}
static bool parseKey(const std::string& tokRaw, int& vk) {
    std::string t = trim(tokRaw);
    if (t.empty()) return false;
    if (t.size() == 1) {
        char c = t[0];
        if (c >= 'a' && c <= 'z') { vk = c - 'a' + 'A'; return true; }
        if (c >= 'A' && c <= 'Z') { vk = c; return true; }
        if (c >= '0' && c <= '9') { vk = c; return true; }
        return false;
    }
    std::string lt = lower(t);
    if (lt[0] == 'f' && lt.size() >= 2) {              // F1..F12
        int n = 0; for (size_t i = 1; i < lt.size(); ++i) { if (!isdigit((unsigned char)lt[i])) return false; n = n * 10 + (lt[i] - '0'); }
        if (n >= 1 && n <= 12) { vk = 0x70 + (n - 1); return true; }
    }
    return false;
}
bool parseHotkey(const std::string& s, Hotkey& out) {
    Hotkey h;
    std::vector<std::string> toks; std::string cur;
    for (char c : s) { if (c == '+') { toks.push_back(cur); cur.clear(); } else cur.push_back(c); }
    toks.push_back(cur);
    if (toks.empty()) return false;
    for (size_t i = 0; i < toks.size(); ++i) {
        std::string t = lower(trim(toks[i]));
        bool isLast = (i + 1 == toks.size());
        if (!isLast) {
            if (t == "ctrl" || t == "control") h.mods |= kModCtrl;
            else if (t == "alt") h.mods |= kModAlt;
            else if (t == "shift") h.mods |= kModShift;
            else if (t == "win" || t == "super") h.mods |= kModWin;
            else return false;                          // 非末位必须是修饰符
        } else {
            if (!parseKey(toks[i], h.vk)) return false; // 末位必须是有效键
        }
    }
    if (h.vk == 0) return false;
    out = h; return true;
}
std::string formatHotkey(const Hotkey& h) {
    std::string s;
    if (h.mods & kModCtrl) s += "Ctrl+";
    if (h.mods & kModAlt) s += "Alt+";
    if (h.mods & kModShift) s += "Shift+";
    if (h.mods & kModWin) s += "Win+";
    if ((h.vk >= 'A' && h.vk <= 'Z') || (h.vk >= '0' && h.vk <= '9')) s += (char)h.vk;
    else if (h.vk >= 0x70 && h.vk <= 0x7B) s += "F" + std::to_string(h.vk - 0x70 + 1);
    else s += "?";
    return s;
}
}
