#include "services/HotkeyManager.h"
#include "data/SettingsStore.h"

static UINT toWinMods(unsigned m) {
    UINT w = 0;
    if (m & own::kModCtrl)  w |= MOD_CONTROL;
    if (m & own::kModAlt)   w |= MOD_ALT;
    if (m & own::kModShift) w |= MOD_SHIFT;
    if (m & own::kModWin)   w |= MOD_WIN;
    return w;
}
void HotkeyManager::add(int id, const std::string& name, const std::string& defBinding) {
    HkBinding b; b.id = id; b.name = name; b.defBinding = defBinding;
    m_b.push_back(b);
}
void HotkeyManager::loadAndRegister(HWND hwnd, own::SettingsStore& settings) {
    // 1) 解析每个绑定（设置覆盖优先，解析失败回落默认）
    for (auto& b : m_b) {
        std::string s = settings.getString("hotkey." + b.name, b.defBinding);
        if (!own::parseHotkey(s, b.hk))
            own::parseHotkey(b.defBinding, b.hk);
    }
    // 2) 冲突检测：后出现者跳过
    std::vector<own::Hotkey> hs; hs.reserve(m_b.size());
    for (auto& b : m_b) hs.push_back(b.hk);
    std::vector<bool> skip(m_b.size(), false);
    for (auto& pr : own::findHotkeyConflicts(hs)) skip[pr.second] = true;
    // 3) 注册
    for (size_t i = 0; i < m_b.size(); ++i) {
        auto& b = m_b[i];
        if (skip[i]) {
            ::OutputDebugStringA(("[hotkey] skip conflicting binding: " + b.name + "\n").c_str());
            b.registered = false;
            continue;
        }
        b.registered = ::RegisterHotKey(hwnd, b.id, toWinMods(b.hk.mods), (UINT)b.hk.vk) != FALSE;
        if (!b.registered)
            ::OutputDebugStringA(("[hotkey] RegisterHotKey failed: " + b.name + "\n").c_str());
    }
}
void HotkeyManager::unregisterAll(HWND hwnd) {
    for (auto& b : m_b) if (b.registered) { ::UnregisterHotKey(hwnd, b.id); b.registered = false; }
}
