#include "app/AppHostWindow.h"

BEGIN_MESSAGE_MAP(CAppHostWindow, CWnd)
    ON_WM_HOTKEY()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

bool CAppHostWindow::Create() {
    LPCTSTR cls = AfxRegisterWndClass(0);
    if (!CreateEx(0, cls, _T("OwnAppHost"), WS_POPUP, CRect(0, 0, 0, 0), NULL, 0))
        return false;
    // Temporary quit hotkey Ctrl+Alt+Q (P5 replaces the whole hotkey scheme).
    ::RegisterHotKey(m_hWnd, kHotkeyQuit, MOD_CONTROL | MOD_ALT, 'Q');
    return true;
}

void CAppHostWindow::OnHotKey(UINT idHotKey, UINT, UINT) {
    if (idHotKey == kHotkeyQuit)
        ::PostQuitMessage(0);
}

void CAppHostWindow::OnDestroy() {
    ::UnregisterHotKey(m_hWnd, kHotkeyQuit);
    CWnd::OnDestroy();
}
