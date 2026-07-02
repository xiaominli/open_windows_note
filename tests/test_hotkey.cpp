#include "doctest.h"
#include "domain/Hotkey.h"
using own::Hotkey; using own::parseHotkey; using own::formatHotkey;

TEST_CASE("parseHotkey letters/digits/function keys") {
    Hotkey h;
    CHECK(parseHotkey("Ctrl+Alt+N", h));
    CHECK(h.mods == (own::kModCtrl | own::kModAlt));
    CHECK(h.vk == 'N');
    CHECK(parseHotkey("ctrl+shift+F5", h));
    CHECK(h.mods == (own::kModCtrl | own::kModShift));
    CHECK(h.vk == 0x74);                 // VK_F5
    CHECK(parseHotkey("Alt+9", h));
    CHECK(h.vk == '9');
    CHECK(parseHotkey("Win+M", h));
    CHECK(h.mods == own::kModWin);
}
TEST_CASE("parseHotkey rejects missing/unknown key") {
    Hotkey h;
    CHECK_FALSE(parseHotkey("", h));
    CHECK_FALSE(parseHotkey("Ctrl", h));        // 只有修饰符
    CHECK_FALSE(parseHotkey("Ctrl+Alt+", h));
    CHECK_FALSE(parseHotkey("Ctrl+Foo", h));
}
TEST_CASE("formatHotkey fixed order + roundtrip") {
    CHECK(formatHotkey(Hotkey{own::kModCtrl | own::kModAlt, 'N'}) == "Ctrl+Alt+N");
    CHECK(formatHotkey(Hotkey{own::kModCtrl | own::kModShift, 0x74}) == "Ctrl+Shift+F5");
    Hotkey h; REQUIRE(parseHotkey("Win+Shift+F12", h));
    CHECK(formatHotkey(h) == "Shift+Win+F12");   // 固定顺序：Ctrl,Alt,Shift,Win
}
TEST_CASE("findHotkeyConflicts reports duplicate (mods,vk) pairs") {
    using own::findHotkeyConflicts;
    std::vector<Hotkey> hs = {
        {own::kModCtrl, 'N'},           // 0
        {own::kModAlt,  'N'},           // 1  (mods 不同，不冲突)
        {own::kModCtrl, 'N'},           // 2  与 0 冲突
        {own::kModCtrl | own::kModAlt, 'M'}, // 3
    };
    auto c = findHotkeyConflicts(hs);
    REQUIRE(c.size() == 1);
    CHECK(c[0].first == 0);
    CHECK(c[0].second == 2);
}
TEST_CASE("findHotkeyConflicts empty when all unique") {
    using own::findHotkeyConflicts;
    std::vector<Hotkey> hs = { {own::kModCtrl,'A'}, {own::kModCtrl,'B'} };
    CHECK(findHotkeyConflicts(hs).empty());
}
