#pragma once
#include <windows.h>
#include <shellapi.h>
class TrayIcon {
public:
    bool add(HWND owner, UINT callbackMsg, UINT id, HICON icon, const wchar_t* tip);
    void modifyTip(const wchar_t* tip);
    void reAdd();
    void remove();
    bool added() const { return m_added; }
private:
    NOTIFYICONDATAW m_nid{};
    bool m_added = false;
};
