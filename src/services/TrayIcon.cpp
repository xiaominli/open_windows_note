#include "services/TrayIcon.h"
#include <wchar.h>

bool TrayIcon::add(HWND owner, UINT callbackMsg, UINT id, HICON icon, const wchar_t* tip) {
    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(m_nid);
    m_nid.hWnd = owner;
    m_nid.uID = id;
    m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_nid.uCallbackMessage = callbackMsg;
    m_nid.hIcon = icon;
    if (tip) wcsncpy_s(m_nid.szTip, tip, _TRUNCATE);
    m_added = ::Shell_NotifyIconW(NIM_ADD, &m_nid) != FALSE;
    return m_added;
}
void TrayIcon::modifyTip(const wchar_t* tip) {
    if (!m_added) return;
    if (tip) wcsncpy_s(m_nid.szTip, tip, _TRUNCATE);
    ::Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}
void TrayIcon::reAdd() {
    // 资源管理器重启后重新添加（TaskbarCreated 时调用）
    m_added = ::Shell_NotifyIconW(NIM_ADD, &m_nid) != FALSE;
}
void TrayIcon::remove() {
    if (!m_added) return;
    ::Shell_NotifyIconW(NIM_DELETE, &m_nid);
    m_added = false;
}
