#pragma once
#include <string>
#include <vector>
#include <utility>
namespace own {
enum HotkeyMods { kModCtrl = 1, kModAlt = 2, kModShift = 4, kModWin = 8 };
struct Hotkey { unsigned mods = 0; int vk = 0; };
bool parseHotkey(const std::string& s, Hotkey& out);
std::string formatHotkey(const Hotkey& h);
std::vector<std::pair<int,int>> findHotkeyConflicts(const std::vector<Hotkey>& hs);
}
