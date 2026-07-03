#pragma once
#include <windows.h>
#include <functional>
#include <string>
// 前台窗口切换监听（SetWinEventHook, OUTOFCONTEXT：回调走本线程消息循环）。
// 本进程自己的窗口成为前台时不回调——否则点便签会触发「不匹配→隐藏自己」。
class StickyWindowWatcher {
public:
    ~StickyWindowWatcher() { stop(); }
    bool start();
    void stop();
    std::function<void(const std::string& titleU8, const std::string& classU8)> onForeground;
private:
    static void CALLBACK proc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG, LONG, DWORD, DWORD);
    HWINEVENTHOOK m_hook = nullptr;
};
