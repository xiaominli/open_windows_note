#include "services/StickyWindowWatcher.h"

static StickyWindowWatcher* s_inst = nullptr;   // 单实例：WinEvent 回调无用户指针

static std::string wToU8(const wchar_t* w, int len) {
    if (len <= 0) return std::string();
    int n = ::WideCharToMultiByte(CP_UTF8, 0, w, len, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n : 0, '\0');
    if (n > 0) ::WideCharToMultiByte(CP_UTF8, 0, w, len, &s[0], n, nullptr, nullptr);
    return s;
}

bool StickyWindowWatcher::start() {
    if (m_hook) return true;
    m_hook = ::SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                               nullptr, proc, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (m_hook) s_inst = this;           // 钩上才登记：s_inst 非空 <=> 钩子在
    return m_hook != nullptr;   // 失败=优雅降级：贴窗不工作，其余功能不受影响
}
void StickyWindowWatcher::stop() {
    if (m_hook) { ::UnhookWinEvent(m_hook); m_hook = nullptr; }
    if (s_inst == this) s_inst = nullptr;
}
void CALLBACK StickyWindowWatcher::proc(HWINEVENTHOOK, DWORD, HWND hwnd, LONG, LONG, DWORD, DWORD) {
    if (!hwnd || !s_inst || !s_inst->onForeground) return;
    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    if (pid == ::GetCurrentProcessId()) return;   // 自家窗口置前不算
    wchar_t title[256]{}; int tl = ::GetWindowTextW(hwnd, title, 256);
    wchar_t cls[256]{};   int cl = ::GetClassNameW(hwnd, cls, 256);
    s_inst->onForeground(wToU8(title, tl), wToU8(cls, cl));
}
