#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "domain/Hotkey.h"
namespace own { class SettingsStore; }

struct HkBinding {
    int id;
    std::string name;        // settings key 后缀 & 展示名
    std::string defBinding;  // 默认绑定字符串，如 "Ctrl+Alt+N"
    own::Hotkey hk;
    bool registered = false;
};
class HotkeyManager {
public:
    void add(int id, const std::string& name, const std::string& defBinding);
    void loadAndRegister(HWND hwnd, own::SettingsStore& settings);
    void unregisterAll(HWND hwnd);
    const std::vector<HkBinding>& bindings() const { return m_b; }
private:
    std::vector<HkBinding> m_b;
};
